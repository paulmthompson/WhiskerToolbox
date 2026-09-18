#include "MediaLine_Widget.hpp"
#include "ui_MediaLine_Widget.h"

#include "Core/LineDrawOperations.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"
#include "Media_Widget/Rendering/Media_Window/Media_Window.hpp"
#include "Media_Widget/UI/Tools/MediaToolId.hpp"
#include "SelectionWidgets/LineAddSelectionWidget.hpp"
#include "SelectionWidgets/LineDrawAllFramesSelectionWidget.hpp"
#include "SelectionWidgets/LineEraseSelectionWidget.hpp"
#include "SelectionWidgets/LineNoneSelectionWidget.hpp"

#include "CoreGeometry/point_geometry.hpp"
#include "CoreMath/polynomial_fit.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Angle/line_angle.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"

#include "Core/LineStyleState.hpp"
#include "LineStyleControls.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <armadillo>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include <cmath>

namespace {

/**
 * @brief Whether the Pen toolbar tool owns selected-line edit gestures
 * @param state Media widget state
 * @param active_key Active line data key
 * @return True when Pen tool is active for a non-empty line key
 */
[[nodiscard]] bool isPenLineEditActive(MediaWidgetState const * state) {
    return state != nullptr && state->activeMediaTool() == MediaToolId::Pen;
}

/**
 * @brief Whether the Eraser toolbar tool owns erase gestures for the active line key
 */
[[nodiscard]] bool isEraserToolActive(MediaWidgetState const * state, std::string const & active_key) {
    return state != nullptr && !active_key.empty() && state->activeMediaTool() == MediaToolId::Eraser;
}

/**
 * @brief Resolve which line endpoint receives the next appended point
 * @param policy User-selected append policy
 * @param click Click position in media coordinates
 * @param line Selected line geometry
 * @return Tip or Base endpoint for the append operation
 */
[[nodiscard]] LineAppendEndpoint resolveLineAppendEndpoint(LineAppendEndpoint policy,
                                                           Point2D<float> const & click,
                                                           Line2D const & line) {
    if (line.empty()) {
        return LineAppendEndpoint::Tip;
    }
    if (policy != LineAppendEndpoint::Nearest) {
        return policy;
    }

    float const dist_to_base = calc_distance(click, line.front());
    float const dist_to_tip = calc_distance(click, line.back());
    return dist_to_base <= dist_to_tip ? LineAppendEndpoint::Base : LineAppendEndpoint::Tip;
}

/**
 * @brief Append interpolated samples between two points
 * @param points Output vector receiving interpolated samples (excluding endpoints)
 * @param from Start point of the interpolation segment
 * @param to End point of the interpolation segment
 */
void appendInterpolationSamples(std::vector<Point2D<float>> & points,
                                Point2D<float> const & from,
                                Point2D<float> const & to) {
    float const dx = to.x - from.x;
    float const dy = to.y - from.y;
    float const distance = std::sqrt(dx * dx + dy * dy);

    if (distance <= 5.0f) {
        return;
    }

    int const num_interp_points = std::max(2, static_cast<int>(distance / 5.0f));
    for (int i = 1; i <= num_interp_points; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(num_interp_points + 1);
        points.emplace_back(from.x + t * dx, from.y + t * dy);
    }
}

}// namespace

MediaLine_Widget::MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaLine_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state} {
    ui->setupUi(this);

    // Create LineStyleState and LineStyleControls
    _line_style_state = new LineStyleState(this);
    _line_style_controls = new LineStyleControls(_line_style_state, this);

    // Insert LineStyleControls into the layout after the header grids (position 2)
    ui->verticalLayout->insertWidget(2, _line_style_controls);

    // Connect LineStyleState changes to update LineDisplayOptions
    connect(_line_style_state, &LineStyleState::styleChanged,
            this, &MediaLine_Widget::_applyLineStyleToOptions);

    _selection_modes["(None)"] = Selection_Mode::None;
    _selection_modes["Draw Across All Frames"] = Selection_Mode::DrawAllFrames;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));

    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaLine_Widget::_toggleSelectionMode);

    // Initialize the selection mode to match the current combo box selection
    _toggleSelectionMode(ui->selection_mode_combo->currentText());

    connect(ui->show_points_checkbox, &QCheckBox::toggled, this, &MediaLine_Widget::_toggleShowPoints);

    // Connect position marker controls
    connect(ui->show_position_marker_checkbox, &QCheckBox::toggled,
            this, &MediaLine_Widget::_toggleShowPositionMarker);
    connect(ui->position_percentage_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setPositionPercentage);
    connect(ui->position_percentage_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setPositionPercentage);

    // Synchronize position percentage slider and spinbox
    connect(ui->position_percentage_slider, &QSlider::valueChanged,
            ui->position_percentage_spinbox, &QSpinBox::setValue);
    connect(ui->position_percentage_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->position_percentage_slider, &QSlider::setValue);

    // Connect segment controls
    connect(ui->show_segment_checkbox, &QCheckBox::toggled,
            this, &MediaLine_Widget::_toggleShowSegment);
    connect(ui->segment_start_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setSegmentStartPercentage);
    connect(ui->segment_start_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setSegmentStartPercentage);
    connect(ui->segment_end_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setSegmentEndPercentage);
    connect(ui->segment_end_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setSegmentEndPercentage);

    // Note: Removed direct slider-spinbox synchronization to prevent infinite loops
    // The synchronization is now handled within the percentage setter functions


    _setupSelectionModePages();

    if (_scene != nullptr) {
        connect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaLine_Widget::_clickedInVideoWithModifiers);
        connect(_scene, &Media_Window::mouseMove, this, &MediaLine_Widget::_mouseMovedInVideo);
        connect(_scene, &Media_Window::leftRelease, this, &MediaLine_Widget::_mouseReleasedInVideo);
        connect(_scene, &Media_Window::rightClickMedia, this, &MediaLine_Widget::_rightClickedInVideo);
    }
}

void MediaLine_Widget::_setupSelectionModePages() {
    _noneSelectionWidget = new line_widget::LineNoneSelectionWidget();
    ui->mode_stacked_widget->addWidget(_noneSelectionWidget);

    _addSelectionWidget = new line_widget::LineAddSelectionWidget();
    ui->mode_stacked_widget->addWidget(_addSelectionWidget);

    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeSnappingToggled,
            this, &MediaLine_Widget::_toggleEdgeSnapping);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::smoothingModeChanged,
            this, &MediaLine_Widget::_setSmoothingMode);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::polynomialOrderChanged,
            this, &MediaLine_Widget::_setPolynomialOrder);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeThresholdChanged,
            this, &MediaLine_Widget::_setEdgeThreshold);
    connect(_addSelectionWidget, &line_widget::LineAddSelectionWidget::edgeSearchRadiusChanged,
            this, &MediaLine_Widget::_setEdgeSearchRadius);

    _eraseSelectionWidget = new line_widget::LineEraseSelectionWidget();
    ui->mode_stacked_widget->addWidget(_eraseSelectionWidget);

    connect(_eraseSelectionWidget, &line_widget::LineEraseSelectionWidget::eraserRadiusChanged,
            this, &MediaLine_Widget::_setEraserRadius);
    connect(_eraseSelectionWidget, &line_widget::LineEraseSelectionWidget::showCircleToggled,
            this, &MediaLine_Widget::_toggleShowHoverCircle);

    _drawAllFramesSelectionWidget = new line_widget::LineDrawAllFramesSelectionWidget();
    ui->mode_stacked_widget->addWidget(_drawAllFramesSelectionWidget);

    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::lineDrawingStarted,
            this, [this]() {
                spdlog::debug("Line drawing started for all frames mode");
            });
    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::lineDrawingCompleted,
            this, [this]() {
                spdlog::debug("Line drawing completed for all frames mode");
            });
    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::applyToAllFrames,
            this, &MediaLine_Widget::_applyLineToAllFrames);
    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::linePointsUpdated,
            this, &MediaLine_Widget::_updateTemporaryLineFromWidget);

    ui->mode_stacked_widget->setCurrentIndex(0);
}

MediaLine_Widget::~MediaLine_Widget() {
    delete ui;
}

void MediaLine_Widget::showEvent(QShowEvent * event) {

    static_cast<void>(event);

    spdlog::debug("MediaLine_Widget: showEvent");

    // Debug: Check initial selection state
    auto initial_selections = _scene->getSelectedEntities();
    spdlog::debug("MediaLine_Widget: initial selected entities on show: {}", initial_selections.size());
}

void MediaLine_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    spdlog::debug("MediaLine_Widget: hideEvent");

    // Guard against _scene being destroyed before hideEvent is called
    if (!_scene) {
        return;
    }

    _is_eraser_dragging = false;

    // Clean up hover circle when switching away from line widget
    _scene->setShowHoverCircle(false);

    // Note: We don't disable group selection here to preserve selections
}

void MediaLine_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Sync LineStyleState from the current LineDisplayOptions
    _syncLineStyleFromOptions();

    // Set domain-specific controls if available
    if (!key.empty() && _state) {
        auto const * config = _state->displayOptions().get<LineDisplayOptions>(QString::fromStdString(key));

        if (config) {
            // Update the show points checkbox directly from the UI file
            ui->show_points_checkbox->blockSignals(true);
            ui->show_points_checkbox->setChecked(config->show_points);
            ui->show_points_checkbox->blockSignals(false);

            // Set position marker controls
            ui->show_position_marker_checkbox->blockSignals(true);
            ui->show_position_marker_checkbox->setChecked(config->show_position_marker);
            ui->show_position_marker_checkbox->blockSignals(false);

            ui->position_percentage_slider->blockSignals(true);
            ui->position_percentage_spinbox->blockSignals(true);
            ui->position_percentage_slider->setValue(config->position_percentage);
            ui->position_percentage_spinbox->setValue(config->position_percentage);
            ui->position_percentage_slider->blockSignals(false);
            ui->position_percentage_spinbox->blockSignals(false);

            // Set segment controls
            ui->show_segment_checkbox->blockSignals(true);
            ui->show_segment_checkbox->setChecked(config->show_segment);
            ui->show_segment_checkbox->blockSignals(false);

            ui->segment_start_slider->blockSignals(true);
            ui->segment_start_spinbox->blockSignals(true);
            ui->segment_start_slider->setValue(config->segment_start_percentage);
            ui->segment_start_spinbox->setValue(config->segment_start_percentage);
            ui->segment_start_slider->blockSignals(false);
            ui->segment_start_spinbox->blockSignals(false);

            ui->segment_end_slider->blockSignals(true);
            ui->segment_end_spinbox->blockSignals(true);
            ui->segment_end_slider->setValue(config->segment_end_percentage);
            ui->segment_end_spinbox->setValue(config->segment_end_percentage);
            ui->segment_end_slider->blockSignals(false);
            ui->segment_end_spinbox->blockSignals(false);

            // Reset line selection
            _current_line_index = 0;
        }
    }
}

void MediaLine_Widget::_clickedInVideoWithModifiers(qreal x_canvas, qreal y_canvas, Qt::KeyboardModifiers modifiers) {
    auto const x_media = static_cast<float>(x_canvas);
    auto const y_media = static_cast<float>(y_canvas);

    if (isPenLineEditActive(_state)) {
        if (modifiers & Qt::AltModifier) {
            std::string const line_key = _resolveSelectedLineKey();
            auto const current_time_opt = _currentLineTime(line_key);
            if (!current_time_opt.has_value()) {
                return;
            }
            spdlog::debug("MediaLine_Widget: Pen tool Alt+click - deleting nearest vertex");
            _deleteNearestVertexFromLine(x_media, y_media, current_time_opt.value());
            return;
        }

        if (modifiers & Qt::ShiftModifier) {
            return;
        }

        LineInteractionPrefs const & prefs = _state->linePrefs();

        if (prefs.pen_target_mode == PenLineTargetMode::NewLine) {
            if (modifiers & Qt::ControlModifier) {
                return;
            }
            spdlog::debug("MediaLine_Widget: Pen tool click - new line");
            _handlePenToolClick(x_media, y_media);
            return;
        }

        if (modifiers & Qt::ControlModifier) {
            spdlog::debug("MediaLine_Widget: Pen tool Ctrl+click - append");
            _handlePenToolClick(x_media, y_media);
        }
        return;
    }

    if (_active_key.empty()) {
        spdlog::debug("MediaLine_Widget: no active key");
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data for active key");
        return;
    }

    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(line_data->getTimeFrame().get());

    if (isEraserToolActive(_state, _active_key)) {
        if ((modifiers & Qt::ControlModifier) || (modifiers & Qt::AltModifier)) {
            return;
        }

        spdlog::debug("MediaLine_Widget: Eraser tool click - erasing vertices within radius");
        _is_eraser_dragging = true;
        _erasePointsFromLine(x_media, y_media, current_time);
        return;
    }

    switch (_selection_mode) {
        case Selection_Mode::None: {
            spdlog::debug("MediaLine_Widget: selection mode is None");
            break;
        }
        case Selection_Mode::DrawAllFrames: {
            spdlog::debug("MediaLine_Widget: selection mode is DrawAllFrames");
            _addPointToDrawAllFrames(x_media, y_media);
            break;
        }
        case Selection_Mode::Add:
        case Selection_Mode::Erase:
            break;
    }
}

void MediaLine_Widget::_mouseMovedInVideo(qreal x_canvas, qreal y_canvas) {
    if (!_is_eraser_dragging || !isEraserToolActive(_state, _active_key)) {
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        return;
    }

    auto const current_time = _state->current_position.convertTo(line_data->getTimeFrame().get());
    _erasePointsFromLine(static_cast<float>(x_canvas),
                         static_cast<float>(y_canvas),
                         current_time);
}

void MediaLine_Widget::_mouseReleasedInVideo() {
    _is_eraser_dragging = false;
}

std::optional<TimeFrameIndex> MediaLine_Widget::_currentLineTime(std::string const & line_key) const {
    if (_state == nullptr || line_key.empty()) {
        return std::nullopt;
    }

    auto line_data = _data_manager->getData<LineData>(line_key);
    if (!line_data) {
        return std::nullopt;
    }

    return _state->current_position.convertTo(line_data->getTimeFrame().get());
}

std::string MediaLine_Widget::_resolveSelectedLineKey() const {
    if (_scene != nullptr && !_scene->selectedDataKey().empty()) {
        return _scene->selectedDataKey();
    }
    return _active_key;
}

void MediaLine_Widget::_switchPenTargetToSelectedLine() {
    if (_state == nullptr) {
        return;
    }

    LineInteractionPrefs prefs = _state->linePrefs();
    prefs.pen_target_mode = PenLineTargetMode::SelectedLine;
    prefs.pen_new_line_key.clear();
    _state->setLinePrefs(prefs);
}

void MediaLine_Widget::_handlePenToolClick(float x_media, float y_media) {
    if (_state == nullptr) {
        return;
    }

    LineInteractionPrefs const & prefs = _state->linePrefs();
    if (prefs.pen_target_mode == PenLineTargetMode::NewLine) {
        if (prefs.pen_new_line_key.empty()) {
            spdlog::debug("MediaLine_Widget: Pen new-line target has no key");
            return;
        }
        _createNewLineAtCurrentTime(x_media, y_media, prefs.pen_new_line_key);
        return;
    }

    std::string const line_key = _resolveSelectedLineKey();
    auto const current_time_opt = _currentLineTime(line_key);
    if (!current_time_opt.has_value()) {
        return;
    }

    _addPointToLine(x_media, y_media, current_time_opt.value());
}

void MediaLine_Widget::_createNewLineAtCurrentTime(float x_media, float y_media, std::string const & line_key) {
    auto line_data = _data_manager->getData<LineData>(line_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data for pen target key {}", line_key);
        return;
    }

    auto const current_time_opt = _currentLineTime(line_key);
    if (!current_time_opt.has_value()) {
        return;
    }

    Point2D<float> const data_coords =
            mediaCoordsToLineDataCoords(x_media, y_media, _scene, _state, *line_data, line_key);

    std::optional<EntityId> const new_entity_id =
            commitNewLineAtTime(*line_data, current_time_opt.value(), Line2D{data_coords}, NotifyObservers::Yes);
    if (!new_entity_id.has_value()) {
        spdlog::debug("MediaLine_Widget: failed to create new line at current time");
        return;
    }

    _scene->selectEntity(new_entity_id.value(), line_key, "line");
    _switchPenTargetToSelectedLine();
    _scene->UpdateCanvas();

    spdlog::debug("MediaLine_Widget: created new line entity {} in key {} at ({}, {})",
                  new_entity_id.value().id,
                  line_key,
                  data_coords.x,
                  data_coords.y);
}

void MediaLine_Widget::_addPointToLine(float x_media, float y_media, TimeFrameIndex current_time) {
    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        spdlog::debug("MediaLine_Widget: no line selected - cannot add points");
        return;
    }

    std::string const line_key = _resolveSelectedLineKey();
    auto line_data = _data_manager->getData<LineData>(line_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data for key {}", line_key);
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    if (!entityExistsAtTime(*line_data, selected_entity_id, current_time)) {
        spdlog::debug("MediaLine_Widget: selected line EntityID {} is not present at current frame",
                      selected_entity_id.id);
        return;
    }

    auto line_ref = line_data->getMutableData(selected_entity_id, NotifyObservers::Yes);
    if (!line_ref.has_value()) {
        spdlog::debug("MediaLine_Widget: could not get mutable reference to line with EntityID {}", selected_entity_id.id);
        return;
    }

    Line2D & line = line_ref.value().get();

    Point2D<float> const data_coords =
            mediaCoordsToLineDataCoords(x_media, y_media, _scene, _state, *line_data, line_key);
    x_media = data_coords.x;
    y_media = data_coords.y;

    // Check if edge snapping is enabled
    bool use_edge_snapping = false;
    if (_state) {
        auto const * line_opts = _state->displayOptions().get<LineDisplayOptions>(QString::fromStdString(_active_key));
        if (line_opts) {
            use_edge_snapping = line_opts->edge_snapping;
        }
    }

    if (use_edge_snapping && _edge_snapping_enabled) {
        if (_current_edges.empty()) {
            _detectEdges();
        }

        auto edge_point = _findNearestEdge(x_media, y_media);
        x_media = edge_point.first;
        y_media = edge_point.second;
    }

    Point2D<float> const new_point{x_media, y_media};
    LineAppendEndpoint append_endpoint = LineAppendEndpoint::Tip;
    if (_state) {
        append_endpoint = resolveLineAppendEndpoint(_state->linePrefs().append_endpoint,
                                                    new_point,
                                                    line);
    }

    if (append_endpoint == LineAppendEndpoint::Base) {
        if (_smoothing_mode == Smoothing_Mode::SimpleSmooth) {
            std::vector<Point2D<float>> updated_points;
            updated_points.reserve(line.size() + 1);
            updated_points.push_back(new_point);
            for (Point2D<float> const & existing_point: line) {
                updated_points.push_back(existing_point);
            }
            line = Line2D(std::move(updated_points));
        } else if (_smoothing_mode == Smoothing_Mode::PolynomialFit) {
            std::vector<Point2D<float>> updated_points;
            updated_points.reserve(line.size() + 1);
            updated_points.push_back(new_point);
            if (!line.empty()) {
                appendInterpolationSamples(updated_points, new_point, line.front());
            }
            for (Point2D<float> const & existing_point: line) {
                updated_points.push_back(existing_point);
            }
            line = Line2D(std::move(updated_points));

            if (line.size() >= 3) {
                _applyPolynomialFit(line, _polynomial_order);
            }
        }
    } else if (_smoothing_mode == Smoothing_Mode::SimpleSmooth) {
        line.push_back(new_point);
    } else if (_smoothing_mode == Smoothing_Mode::PolynomialFit) {
        if (!line.empty()) {
            Point2D<float> const last_point = line.back();
            std::vector<Point2D<float>> interpolated_points;
            appendInterpolationSamples(interpolated_points, last_point, new_point);
            for (Point2D<float> const & interpolated_point: interpolated_points) {
                line.push_back(interpolated_point);
            }
        }

        line.push_back(new_point);

        if (line.size() >= 3) {
            _applyPolynomialFit(line, _polynomial_order);
        }
    }

    spdlog::debug("MediaLine_Widget: added point ({}, {}) to {} endpoint of line {} (EntityID: {})",
                  x_media,
                  y_media,
                  append_endpoint == LineAppendEndpoint::Base ? "base" : "tip",
                  line_key,
                  selected_entity_id.id);
}

void MediaLine_Widget::_deleteNearestVertexFromLine(float x_media, float y_media, TimeFrameIndex current_time) {
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        spdlog::debug("MediaLine_Widget: no line selected - cannot delete vertex");
        return;
    }

    std::string const line_key = _resolveSelectedLineKey();
    auto line_data = _data_manager->getData<LineData>(line_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data for key {}", line_key);
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    if (!entityExistsAtTime(*line_data, selected_entity_id, current_time)) {
        spdlog::debug("MediaLine_Widget: selected line EntityID {} is not present at current frame",
                      selected_entity_id.id);
        return;
    }

    Point2D<float> const data_coords =
            mediaCoordsToLineDataCoords(x_media, y_media, _scene, _state, *line_data, line_key);
    x_media = data_coords.x;
    y_media = data_coords.y;

    auto line_ref = line_data->getMutableData(selected_entity_id, NotifyObservers::Yes);
    if (!line_ref.has_value()) {
        spdlog::debug("MediaLine_Widget: could not get mutable reference to line with EntityID {}", selected_entity_id.id);
        return;
    }

    Line2D & line = line_ref.value().get();

    if (line.size() <= 1) {
        spdlog::debug("MediaLine_Widget: selected line has at most one vertex - cannot delete");
        return;
    }

    float pick_radius = 15.0f;
    if (_state) {
        pick_radius = _state->selectPrefs().pick_radius_px;
    }

    Point2D<float> const click_point{x_media, y_media};
    size_t nearest_index = 0;
    float min_distance = calc_distance(click_point, line[0]);

    for (size_t i = 1; i < line.size(); ++i) {
        float const distance = calc_distance(click_point, line[i]);
        if (distance < min_distance) {
            min_distance = distance;
            nearest_index = i;
        }
    }

    if (min_distance > pick_radius) {
        spdlog::debug("MediaLine_Widget: no vertex within pick radius {} px (nearest {:.1f} px)",
                      pick_radius, min_distance);
        return;
    }

    std::vector<Point2D<float>> remaining_points;
    remaining_points.reserve(line.size() - 1);
    for (size_t i = 0; i < line.size(); ++i) {
        if (i != nearest_index) {
            remaining_points.push_back(line[i]);
        }
    }

    line = Line2D(remaining_points);
    line_data->notifyObservers();
    _scene->UpdateCanvas();

    spdlog::debug("MediaLine_Widget: deleted vertex {} near ({}, {}) from line {} (EntityID: {})",
                  nearest_index, x_media, y_media, line_key, selected_entity_id.id);
}

float MediaLine_Widget::_eraserRadiusPx() const {
    if (_state && _state->activeMediaTool() == MediaToolId::Eraser) {
        return static_cast<float>(_state->eraserPrefs().radius_px);
    }

    if (_eraseSelectionWidget) {
        return static_cast<float>(_eraseSelectionWidget->getEraserRadius());
    }

    return 10.0f;
}

void MediaLine_Widget::_erasePointsFromLine(float x_media, float y_media, TimeFrameIndex current_time) {
    static_cast<void>(current_time);

    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        spdlog::debug("MediaLine_Widget: no line selected - cannot erase points");
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data for active key");
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    auto line_ref = line_data->getMutableData(selected_entity_id, NotifyObservers::Yes);
    if (!line_ref.has_value()) {
        spdlog::debug("MediaLine_Widget: could not get mutable reference to line with EntityID {}", selected_entity_id.id);
        return;
    }

    Line2D & line = line_ref.value().get();

    if (line.empty()) {
        spdlog::debug("MediaLine_Widget: selected line is empty - nothing to erase");
        return;
    }

    float const eraser_radius = _eraserRadiusPx();

    std::vector<Point2D<float>> remaining_points;
    remaining_points.reserve(line.size());
    Point2D<float> const click_point{x_media, y_media};

    for (auto const & point: line) {
        float const distance = calc_distance(click_point, point);
        if (distance > eraser_radius) {
            remaining_points.push_back(point);
        }
    }

    if (remaining_points.size() == line.size()) {
        return;
    }

    line = Line2D(std::move(remaining_points));

    line_data->notifyObservers();

    _scene->UpdateCanvas();
    spdlog::debug("MediaLine_Widget: erased vertices near ({}, {}) from line {} (EntityID: {}, {} remaining)",
                  x_media,
                  y_media,
                  _active_key,
                  selected_entity_id.id,
                  line.size());
}

void MediaLine_Widget::_applyPolynomialFit(Line2D & line, int order) {

    assert(order >= 0 && "Order must be non-negative");

    if (line.size() < static_cast<size_t>(order + 1)) {
        // Not enough points for the requested polynomial order
        return;
    }

    // Extract x and y coordinates
    std::vector<double> t(line.size());
    std::vector<double> x_coords(line.size());
    std::vector<double> y_coords(line.size());

    // Use parameter t along the curve (0 to 1)
    for (size_t i = 0; i < line.size(); ++i) {
        t[i] = static_cast<double>(i) / (line.size() - 1);
        x_coords[i] = line[i].x;
        y_coords[i] = line[i].y;
    }

    // Fit polynomials to x(t) and y(t) using the function from line_angle.hpp
    std::vector<double> const x_coeffs = fit_polynomial(t, x_coords, order);
    std::vector<double> const y_coeffs = fit_polynomial(t, y_coords, order);

    if (x_coeffs.empty() || y_coeffs.empty()) {
        // Fall back to simple smoothing if fitting failed
        smooth_line(line);
        return;
    }

    // Generate smooth curve with more points
    int const num_points = std::max(100, static_cast<int>(line.size()) * 2);
    std::vector<Point2D<float>> smooth_line;
    smooth_line.reserve(num_points);

    for (int i = 0; i < num_points; ++i) {
        double const t_param = static_cast<double>(i) / (num_points - 1);

        // Evaluate polynomials at t_param using the function from line_angle.hpp
        double const x_val = evaluate_polynomial(x_coeffs, t_param);
        double const y_val = evaluate_polynomial(y_coeffs, t_param);

        smooth_line.emplace_back(static_cast<float>(x_val), static_cast<float>(y_val));
    }

    // Replace the original line with the smooth one
    line = Line2D(smooth_line);
}

void MediaLine_Widget::_setSmoothingMode(int index) {
    _smoothing_mode = static_cast<Smoothing_Mode>(index);
    spdlog::debug("MediaLine_Widget: smoothing mode set to {}", index);
}

void MediaLine_Widget::_setPolynomialOrder(int order) {
    _polynomial_order = order;
    spdlog::debug("MediaLine_Widget: polynomial order set to {}", order);
}

void MediaLine_Widget::_toggleSelectionMode(QString const & text) {
    _selection_mode = _selection_modes[text];
    spdlog::debug("MediaLine_Widget: selection mode changed to: {} (enum value: {})", text.toStdString(),
                  static_cast<int>(_selection_mode));

    // Switch to the appropriate page in the stacked widget
    int const pageIndex = static_cast<int>(_selection_mode);
    ui->mode_stacked_widget->setCurrentIndex(pageIndex);

    auto selected_entities = _scene->getSelectedEntities();
    spdlog::debug("MediaLine_Widget: selected entities after mode change: {}", selected_entities.size());

    _scene->setShowHoverCircle(false);

    // Enable/disable temporary line visualization for DrawAllFrames mode
    if (_selection_mode == Selection_Mode::DrawAllFrames) {
        _scene->setShowTemporaryLine(true);
    } else {
        _scene->setShowTemporaryLine(false);
    }
}

void MediaLine_Widget::_toggleShowPoints(bool checked) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->show_points = checked;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_toggleEdgeSnapping(bool checked) {
    _edge_snapping_enabled = checked;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->edge_snapping = checked;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }

        // If enabling edge snapping, perform edge detection immediately
        if (checked) {
            _detectEdges();
        } else {
            // Clear cached edges when disabling
            _current_edges.release();
        }

        _scene->UpdateCanvas();
    }

    spdlog::debug("MediaLine_Widget: edge snapping {}", checked ? "enabled" : "disabled");
}

void MediaLine_Widget::LoadFrame(int frame_id) {
    // Update the widget with the new frame
    // This could involve refreshing displays or updating UI elements
    // specific to the current frame

    // If we have an active line, we might want to update some UI
    // based on line data at this frame
    if (!_active_key.empty()) {
        auto line_data = _data_manager->getData<LineData>(_active_key);
        if (line_data) {
            auto lines = line_data->getAtTime(TimeFrameIndex(frame_id));
            int const num_lines = static_cast<int>(lines.size());

            spdlog::debug("MediaLine_Widget: frame {}: {} lines in {}", frame_id, num_lines, _active_key);
        }
    }
}


void MediaLine_Widget::_setEdgeThreshold(int threshold) {
    _edge_threshold = threshold;
    spdlog::debug("MediaLine_Widget: edge threshold set to {}", threshold);
}

void MediaLine_Widget::_setEdgeSearchRadius(int radius) {
    _edge_search_radius = radius;
    spdlog::debug("MediaLine_Widget: edge search radius set to {}", radius);
}

void MediaLine_Widget::_detectEdges() {

    auto media = _data_manager->getData<MediaData>("media");
    if (!media) {
        spdlog::debug("MediaLine_Widget: no media data available for edge detection");
        return;
    }

    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(media->getTimeFrame().get());
    auto frame_data = media->getProcessedData(current_time.getValue());

    // Convert raw frame data to cv::Mat
    /*
    int width = media->getWidth();
    int height = media->getHeight();
    
    // Create a grayscale image for edge detection
    cv::Mat gray_image;
    
    // Check the format of media data and create appropriate cv::Mat
    if (media->getFormat() == MediaData::DisplayFormat::Gray) {
        // Grayscale image
        gray_image = cv::Mat(height, width, CV_8UC1, frame_data.data());
    } else {
        // Color image (RGBA)
        _current_frame = cv::Mat(height, width, CV_8UC4, frame_data.data());
        
        // Convert to grayscale
        cv::cvtColor(_current_frame, gray_image, cv::COLOR_RGBA2GRAY);
    }
    */
    auto gray_image = ImageProcessing::convert_vector_to_mat(frame_data, media->getImageSize());

    //cv::Mat blurred;
    //cv::GaussianBlur(gray_image, blurred, cv::Size(5, 5), 1.5);
    //cv::Canny(blurred, _current_edges, _edge_threshold / 2, _edge_threshold);

    cv::Canny(gray_image, _current_edges, _edge_threshold / 2, _edge_threshold);

    spdlog::debug("MediaLine_Widget: edge detection completed for frame {}", current_time.getValue());
    spdlog::debug("MediaLine_Widget: edges mask {}x{} ({} elements)", _current_edges.cols, _current_edges.rows,
                  static_cast<std::size_t>(_current_edges.total()));
}

std::pair<float, float> MediaLine_Widget::_findNearestEdge(float x, float y) {
    if (_current_edges.empty()) {
        return {x, y};
    }

    // Round x and y to integers (image coordinates)
    int const x_int = static_cast<int>(std::round(x));
    int const y_int = static_cast<int>(std::round(y));

    // Define search radius and initialize variables
    int const radius = _edge_search_radius;
    float min_distance = radius * radius + 1;     // Initialize to something larger than possible
    std::pair<float, float> nearest_edge = {x, y};// Default to original point

    // Get image dimensions
    int const width = _current_edges.cols;
    int const height = _current_edges.rows;

    // Check if the point is within image bounds
    if (x_int < 0 || x_int >= width || y_int < 0 || y_int >= height) {
        spdlog::debug("MediaLine_Widget: click point outside image bounds");
        return {x, y};
    }

    // Search in a square region around the clicked point
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate current point to check
            int const nx = x_int + dx;
            int const ny = y_int + dy;

            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
            }

            // Check if this is an edge pixel
            if (_current_edges.at<uchar>(ny, nx) > 0) {
                // Calculate distance squared (avoid square root for performance)
                float const d_squared = dx * dx + dy * dy;

                // Update nearest edge if this is closer
                if (d_squared < min_distance) {
                    min_distance = d_squared;
                    nearest_edge = {static_cast<float>(nx), static_cast<float>(ny)};
                }
            }
        }
    }

    if (min_distance < radius * radius + 1) {
        spdlog::debug("MediaLine_Widget: found edge point at ({}, {}), distance: {}", nearest_edge.first,
                      nearest_edge.second, std::sqrt(min_distance));
    } else {
        spdlog::debug("MediaLine_Widget: no edge found within radius {}", radius);
        cv::imwrite("edges.png", _current_edges);
    }

    return nearest_edge;
}

void MediaLine_Widget::_setEraserRadius(int radius) {
    if (_selection_mode == Selection_Mode::Erase) {
        _scene->setHoverCircleRadius(static_cast<double>(radius));
    }
    spdlog::debug("MediaLine_Widget: eraser radius set to {}", radius);
}

void MediaLine_Widget::_toggleShowHoverCircle(bool checked) {
    _scene->setShowHoverCircle(checked);
    spdlog::debug("MediaLine_Widget: show hover circle {}", checked ? "enabled" : "disabled");
}

void MediaLine_Widget::_toggleShowPositionMarker(bool checked) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->show_position_marker = checked;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
    spdlog::debug("MediaLine_Widget: show position marker {}", checked ? "enabled" : "disabled");
}

void MediaLine_Widget::_setPositionPercentage(int percentage) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->position_percentage = percentage;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }

    // Synchronize slider and spinbox if the signal came from one of them
    QObject * sender_obj = sender();
    if (sender_obj == ui->position_percentage_slider) {
        ui->position_percentage_spinbox->blockSignals(true);
        ui->position_percentage_spinbox->setValue(percentage);
        ui->position_percentage_spinbox->blockSignals(false);
    } else if (sender_obj == ui->position_percentage_spinbox) {
        ui->position_percentage_slider->blockSignals(true);
        ui->position_percentage_slider->setValue(percentage);
        ui->position_percentage_slider->blockSignals(false);
    }

    spdlog::debug("MediaLine_Widget: position percentage set to {}", percentage);
}

void MediaLine_Widget::_toggleShowSegment(bool checked) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->show_segment = checked;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
    spdlog::debug("MediaLine_Widget: show segment {}", checked ? "enabled" : "disabled");
}

void MediaLine_Widget::_setSegmentStartPercentage(int percentage) {
    if (_is_updating_percentages) return;
    _is_updating_percentages = true;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            // Ensure start percentage doesn't exceed end percentage - 1%
            int const max_start_percentage = line_opts->segment_end_percentage - 1;
            if (percentage > max_start_percentage) {
                // Don't allow start to exceed end - 1%
                QObject * sender_obj = sender();
                if (sender_obj == ui->segment_start_slider) {
                    ui->segment_start_slider->blockSignals(true);
                    ui->segment_start_slider->setValue(max_start_percentage);
                    ui->segment_start_slider->blockSignals(false);
                } else if (sender_obj == ui->segment_start_spinbox) {
                    ui->segment_start_spinbox->blockSignals(true);
                    ui->segment_start_spinbox->setValue(max_start_percentage);
                    ui->segment_start_spinbox->blockSignals(false);
                }
                percentage = max_start_percentage;
            }

            line_opts->segment_start_percentage = percentage;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }

    // Synchronize the other control if this one was changed
    QObject * sender_obj = sender();
    if (sender_obj == ui->segment_start_slider) {
        ui->segment_start_spinbox->blockSignals(true);
        ui->segment_start_spinbox->setValue(percentage);
        ui->segment_start_spinbox->blockSignals(false);
    } else if (sender_obj == ui->segment_start_spinbox) {
        ui->segment_start_slider->blockSignals(true);
        ui->segment_start_slider->setValue(percentage);
        ui->segment_start_slider->blockSignals(false);
    }

    spdlog::debug("MediaLine_Widget: segment start percentage set to {}", percentage);
    _is_updating_percentages = false;
}

void MediaLine_Widget::_setSegmentEndPercentage(int percentage) {
    if (_is_updating_percentages) return;
    _is_updating_percentages = true;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            // Ensure end percentage doesn't go below start percentage + 1%
            int const min_end_percentage = line_opts->segment_start_percentage + 1;
            if (percentage < min_end_percentage) {
                // Don't allow end to go below start + 1%
                QObject * sender_obj = sender();
                if (sender_obj == ui->segment_end_slider) {
                    ui->segment_end_slider->blockSignals(true);
                    ui->segment_end_slider->setValue(min_end_percentage);
                    ui->segment_end_slider->blockSignals(false);
                } else if (sender_obj == ui->segment_end_spinbox) {
                    ui->segment_end_spinbox->blockSignals(true);
                    ui->segment_end_spinbox->setValue(min_end_percentage);
                    ui->segment_end_spinbox->blockSignals(false);
                }
                percentage = min_end_percentage;
            }

            line_opts->segment_end_percentage = percentage;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }

    // Synchronize the other control if this one was changed
    QObject * sender_obj = sender();
    if (sender_obj == ui->segment_end_slider) {
        ui->segment_end_spinbox->blockSignals(true);
        ui->segment_end_spinbox->setValue(percentage);
        ui->segment_end_spinbox->blockSignals(false);
    } else if (sender_obj == ui->segment_end_spinbox) {
        ui->segment_end_slider->blockSignals(true);
        ui->segment_end_slider->setValue(percentage);
        ui->segment_end_slider->blockSignals(false);
    }

    spdlog::debug("MediaLine_Widget: segment end percentage set to {}", percentage);
    _is_updating_percentages = false;
}

void MediaLine_Widget::_rightClickedInVideo(qreal x_canvas, qreal y_canvas) {
    if (!isPenLineEditActive(_state)) {
        return;
    }

    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    auto x_media = static_cast<float>(x_canvas);
    auto y_media = static_cast<float>(y_canvas);

    // Check if the right-click is near the selected line
    auto nearest_entity_id = _findNearestLine(x_media, y_media);
    if (nearest_entity_id.has_value() && nearest_entity_id.value() == selected_entity_id) {
        // Show context menu at the click position
        QPoint const global_pos = QCursor::pos();
        _showLineContextMenu(global_pos);
    }
}

/**
 * @brief Calculate the minimum distance from a point to a line segment
 * @param point The point to measure distance from
 * @param line_start Start point of the line segment
 * @param line_end End point of the line segment
 * @return The minimum distance from the point to the line segment
 */
float MediaLine_Widget::_calculateDistanceToLineSegment(Point2D<float> const & point,
                                                        Point2D<float> const & line_start,
                                                        Point2D<float> const & line_end) {
    float const dx = line_end.x - line_start.x;
    float const dy = line_end.y - line_start.y;

    // Handle degenerate case where line segment is actually a point
    if (dx == 0.0f && dy == 0.0f) {
        return calc_distance(point, line_start);
    }

    // Calculate the parameter t for the closest point on the line segment
    float t = ((point.x - line_start.x) * dx + (point.y - line_start.y) * dy) / (dx * dx + dy * dy);

    // Clamp t to [0, 1] to stay within the line segment
    t = std::max(0.0f, std::min(1.0f, t));

    // Calculate the closest point on the line segment
    Point2D<float> const closest_point = {
            line_start.x + t * dx,
            line_start.y + t * dy};

    // Return the distance from the point to the closest point on the line segment
    return calc_distance(point, closest_point);
}

std::optional<EntityId> MediaLine_Widget::_findNearestLine(float x, float y) {
    if (_active_key.empty()) {
        return std::nullopt;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        return std::nullopt;
    }
    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(line_data->getTimeFrame().get());
    auto entity_ids = line_data->getEntityIdsAtTime(current_time);
    auto lines = line_data->getAtTime(current_time);

    if (lines.empty()) {
        return std::nullopt;
    }

    Point2D<float> const click_point{x, y};
    std::optional<EntityId> nearest_entity_id = std::nullopt;
    float const selection_threshold = _state != nullptr
                                              ? _state->selectPrefs().pick_radius_px
                                              : 15.0f;
    float min_distance = selection_threshold + 1;// Initialize beyond threshold

    for (size_t i = 0; i < lines.size(); ++i) {
        auto const & line = lines[i];
        auto const entity_id = entity_ids[i];

        if (line.empty()) {
            continue;
        }

        // Calculate distance to each line segment
        float min_segment_distance = std::numeric_limits<float>::max();
        for (size_t i = 0; i < line.size() - 1; ++i) {
            float const segment_distance = _calculateDistanceToLineSegment(click_point, line[i], line[i + 1]);
            if (segment_distance < min_segment_distance) {
                min_segment_distance = segment_distance;
            }
        }

        // Also check distance to vertices for completeness
        float min_vertex_distance = std::numeric_limits<float>::max();
        for (auto const & vertex: line) {
            float const vertex_distance = calc_distance(click_point, vertex);
            if (vertex_distance < min_vertex_distance) {
                min_vertex_distance = vertex_distance;
            }
        }

        float const line_distance = std::min(min_vertex_distance, min_segment_distance);

        if (line_distance < min_distance) {
            min_distance = line_distance;
            nearest_entity_id = entity_id;
            spdlog::debug("MediaLine_Widget: new closest line with EntityID {}", entity_id.id);
        }
    }

    return (min_distance <= selection_threshold) ? nearest_entity_id : std::nullopt;
}

void MediaLine_Widget::_selectLine(int line_index) {
    _selected_line_index = line_index;

    // Update the line display options to show the selected line differently
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->selected_line_index = line_index;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_clearLineSelection() {
    _selected_line_index = -1;

    // Update the line display options to clear selection
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->selected_line_index = -1;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_showLineContextMenu(QPoint const & position) {
    QMenu context_menu(this);

    // Create Move To submenu
    QMenu * move_menu = context_menu.addMenu("Move Line To");
    QMenu * copy_menu = context_menu.addMenu("Copy Line To");

    // Get available LineData keys
    auto available_keys = _getAvailableLineDataKeys();

    for (auto const & key: available_keys) {
        if (key != _active_key) {// Don't include the current key
            // Add to Move menu
            QAction * move_action = move_menu->addAction(QString::fromStdString(key));
            connect(move_action, &QAction::triggered, [this, key]() {
                _moveLineToTarget(key);
            });

            // Add to Copy menu
            QAction * copy_action = copy_menu->addAction(QString::fromStdString(key));
            connect(copy_action, &QAction::triggered, [this, key]() {
                _copyLineToTarget(key);
            });
        }
    }

    // Disable menus if no other LineData available
    if (available_keys.size() <= 1) {
        move_menu->setEnabled(false);
        copy_menu->setEnabled(false);
    }

    context_menu.exec(position);
}

std::vector<std::string> MediaLine_Widget::_getAvailableLineDataKeys() {
    return _data_manager->getKeys<LineData>();
}

void MediaLine_Widget::_moveLineToTarget(std::string const & target_key) {
    if (_active_key.empty()) {
        return;
    }

    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data || !target_line_data) {
        spdlog::warn("MediaLine_Widget: could not retrieve source or target LineData");
        return;
    }

    // Get the selected line data by EntityID
    auto selected_line_opt = source_line_data->getDataByEntityId(selected_entity_id);
    if (!selected_line_opt.has_value()) {
        spdlog::warn("MediaLine_Widget: could not find line with EntityID {}", selected_entity_id.id);
        return;
    }

    Line2D const selected_line = selected_line_opt.value().get();

    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(target_line_data->getTimeFrame().get());

    // Add to target
    target_line_data->addAtTime(current_time, selected_line, NotifyObservers::No);

    // Remove from source using clearByEntityId
    if (!source_line_data->clearByEntityId(selected_entity_id, NotifyObservers::No)) {
        spdlog::warn("MediaLine_Widget: could not clear line with EntityID {}", selected_entity_id.id);
        return;
    }

    // Clear selection since the line was moved
    _scene->clearAllSelections();
    source_line_data->notifyObservers();
    target_line_data->notifyObservers();

    spdlog::debug("MediaLine_Widget: moved line with EntityID {} from {} to {}", selected_entity_id.id, _active_key,
                  target_key);
}

void MediaLine_Widget::_copyLineToTarget(std::string const & target_key) {
    if (_active_key.empty()) {
        return;
    }

    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }

    EntityId const selected_entity_id = *selected_entities.begin();

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data || !target_line_data) {
        spdlog::warn("MediaLine_Widget: could not retrieve source or target LineData");
        return;
    }

    // Get the selected line data by EntityID
    auto selected_line_opt = source_line_data->getDataByEntityId(selected_entity_id);
    if (!selected_line_opt.has_value()) {
        spdlog::warn("MediaLine_Widget: could not find line with EntityID {}", selected_entity_id.id);
        return;
    }

    Line2D const selected_line = selected_line_opt.value().get();

    auto const current_position = _state->current_position;
    auto const current_time = current_position.convertTo(target_line_data->getTimeFrame().get());

    // Copy to target
    target_line_data->addAtTime(current_time, selected_line, NotifyObservers::No);
    target_line_data->notifyObservers();
}

void MediaLine_Widget::_addPointToDrawAllFrames(float x_media, float y_media) {
    if (_drawAllFramesSelectionWidget && _drawAllFramesSelectionWidget->isDrawingActive()) {
        // Store the points in default media coordinates for the temporary line
        _drawAllFramesSelectionWidget->addPoint(Point2D<float>{x_media, y_media});

        // Update the temporary line visualization using default aspect ratios
        auto current_points = _drawAllFramesSelectionWidget->getCurrentLinePoints();
        _scene->updateTemporaryLine(current_points, "");// Use default aspect ratios
    }
}

void MediaLine_Widget::_applyLineToAllFrames() {
    if (!_drawAllFramesSelectionWidget || _active_key.empty()) {
        spdlog::debug("MediaLine_Widget: cannot apply line to all frames: widget not available or no active key");
        return;
    }

    auto line_points = _drawAllFramesSelectionWidget->getCurrentLinePoints();
    if (line_points.empty()) {
        spdlog::debug("MediaLine_Widget: no line points to apply to all frames");
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        spdlog::debug("MediaLine_Widget: no line data available for active key");
        return;
    }

    // Get all frame times
    auto frame_times = _getAllFrameTimes();
    if (frame_times.empty()) {
        spdlog::debug("MediaLine_Widget: no frame times available");
        return;
    }

    // Convert coordinates from default media to line-specific coordinates if needed
    Line2D line_to_apply;
    if (!_active_key.empty()) {
        auto line_data_for_conversion = _data_manager->getData<LineData>(_active_key);
        if (line_data_for_conversion) {
            auto image_size = line_data_for_conversion->getImageSize();

            // If the line data has specific image dimensions, convert coordinates
            if (image_size.width != -1 && image_size.height != -1) {
                float const default_xAspect = _scene->getXAspect();
                float const default_yAspect = _scene->getYAspect();
                float const line_xAspect = static_cast<float>(_scene->getCanvasSize().first) / image_size.width;
                float const line_yAspect = static_cast<float>(_scene->getCanvasSize().second) / image_size.height;

                // Convert each point from default media coordinates to line-specific coordinates
                for (auto const & point: line_points) {
                    float const x_canvas = point.x * default_xAspect;
                    float const y_canvas = point.y * default_yAspect;
                    float const x_converted = x_canvas / line_xAspect;
                    float const y_converted = y_canvas / line_yAspect;
                    line_to_apply.push_back(Point2D<float>{x_converted, y_converted});
                }
            } else {
                line_to_apply = Line2D(line_points);
            }
        } else {
            line_to_apply = Line2D(line_points);
        }
    } else {
        line_to_apply = Line2D(line_points);
    }

    // Apply the line to all frames
    int frames_processed = 0;
    for (auto const & frame_time: frame_times) {
        line_data->addAtTime(frame_time, line_to_apply, NotifyObservers::No);// Don't notify for each frame
        frames_processed++;
    }

    // Notify observers once at the end
    line_data->notifyObservers();

    // Clear the line points after applying
    _drawAllFramesSelectionWidget->clearLinePoints();

    // Clear the temporary line visualization
    _scene->clearTemporaryLine();

    _scene->UpdateCanvas();
}

std::vector<TimeFrameIndex> MediaLine_Widget::_getAllFrameTimes() {
    std::vector<TimeFrameIndex> frame_times;

    // Get media data to determine total frame count
    auto media_data = _data_manager->getData<MediaData>("media");
    if (!media_data) {
        spdlog::debug("MediaLine_Widget: no media data available");
        return frame_times;
    }

    int const total_frames = media_data->getTotalFrameCount();
    if (total_frames <= 0) {
        spdlog::debug("MediaLine_Widget: invalid frame count: {}", total_frames);
        return frame_times;
    }

    // Create TimeFrameIndex for each frame
    frame_times.reserve(total_frames);
    for (int i = 0; i < total_frames; ++i) {
        frame_times.emplace_back(i);
    }

    return frame_times;
}

std::optional<EntityId> MediaLine_Widget::_getSelectedEntityIdFromGroupSystem() const {
    // Get the selected entities from the group system
    auto selected_entities = _scene->getSelectedEntities();

    spdlog::debug("MediaLine_Widget: selected entities count: {}", selected_entities.size());

    // If no entities are selected, return nullopt
    if (selected_entities.empty()) {
        spdlog::debug("MediaLine_Widget: no entities selected");
        return std::nullopt;
    }

    // Return the first selected entity ID
    EntityId first_selected = *selected_entities.begin();
    spdlog::debug("MediaLine_Widget: first selected entity ID: {}", first_selected.id);
    return first_selected;
}

void MediaLine_Widget::_updateTemporaryLineFromWidget() {
    if (_drawAllFramesSelectionWidget) {
        auto current_points = _drawAllFramesSelectionWidget->getCurrentLinePoints();
        _scene->updateTemporaryLine(current_points, "");// Use default aspect ratios
    }
}

void MediaLine_Widget::_syncLineStyleFromOptions() {
    if (_active_key.empty() || !_state || !_line_style_state) {
        return;
    }

    auto const * config = _state->displayOptions().get<LineDisplayOptions>(QString::fromStdString(_active_key));
    if (!config) {
        return;
    }

    CorePlotting::LineStyleData style;
    style.hex_color = config->hex_color();
    style.thickness = static_cast<float>(config->line_thickness);
    style.alpha = config->alpha();
    _line_style_state->setStyleSilent(style);
}

void MediaLine_Widget::_applyLineStyleToOptions() {
    if (_active_key.empty() || !_state || !_line_style_state) {
        return;
    }

    auto const key = QString::fromStdString(_active_key);
    auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
    if (!line_opts) {
        return;
    }

    auto const & style = _line_style_state->data();
    line_opts->hex_color() = style.hex_color;
    line_opts->line_thickness = static_cast<int>(style.thickness);
    line_opts->alpha() = style.alpha;
    _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
    _scene->UpdateCanvas();
}
