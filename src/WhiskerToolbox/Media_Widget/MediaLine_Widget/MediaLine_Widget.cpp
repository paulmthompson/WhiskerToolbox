#include "MediaLine_Widget.hpp"
#include "ui_MediaLine_Widget.h"

#include "CoreGeometry/point_geometry.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Media/Video_Data.hpp"
#include "DataManager/transforms/Lines/Line_Angle/line_angle.hpp"
#include "DataManager/utils/polynomial/polynomial_fit.hpp"
#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media_Widget/Media_Window/Media_Window.hpp"
#include "Media_Widget/MediaWidgetState.hpp"
#include "SelectionWidgets/LineAddSelectionWidget.hpp"
#include "SelectionWidgets/LineDrawAllFramesSelectionWidget.hpp"
#include "SelectionWidgets/LineEraseSelectionWidget.hpp"
#include "SelectionWidgets/LineNoneSelectionWidget.hpp"
#include "SelectionWidgets/LineSelectSelectionWidget.hpp"

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
#include <iostream>
#include <opencv2/opencv.hpp>

MediaLine_Widget::MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, MediaWidgetState * state, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MediaLine_Widget),
      _data_manager{std::move(data_manager)},
      _scene{scene},
      _state{state} {
    ui->setupUi(this);

    _selection_modes["(None)"] = Selection_Mode::None;
    _selection_modes["Select Line"] = Selection_Mode::Select;
    _selection_modes["Draw Across All Frames"] = Selection_Mode::DrawAllFrames;

    ui->selection_mode_combo->addItems(QStringList(_selection_modes.keys()));

    connect(ui->selection_mode_combo, &QComboBox::currentTextChanged, this, &MediaLine_Widget::_toggleSelectionMode);

    // Initialize the selection mode to match the current combo box selection
    _toggleSelectionMode(ui->selection_mode_combo->currentText());

    connect(ui->color_picker, &ColorPicker_Widget::colorChanged,
            this, &MediaLine_Widget::_setLineColor);
    connect(ui->color_picker, &ColorPicker_Widget::alphaChanged,
            this, &MediaLine_Widget::_setLineAlpha);

    // Connect line thickness controls
    connect(ui->line_thickness_slider, &QSlider::valueChanged,
            this, &MediaLine_Widget::_setLineThickness);
    connect(ui->line_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MediaLine_Widget::_setLineThickness);

    // Synchronize line thickness slider and spinbox
    connect(ui->line_thickness_slider, &QSlider::valueChanged,
            ui->line_thickness_spinbox, &QSpinBox::setValue);
    connect(ui->line_thickness_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->line_thickness_slider, &QSlider::setValue);

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

    _selectSelectionWidget = new line_widget::LineSelectSelectionWidget();
    ui->mode_stacked_widget->addWidget(_selectSelectionWidget);

    connect(_selectSelectionWidget, &line_widget::LineSelectSelectionWidget::selectionThresholdChanged,
            this, [this](float threshold) {
                _line_selection_threshold = threshold;
                std::cout << "Line selection threshold set to: " << threshold << std::endl;
            });

    _drawAllFramesSelectionWidget = new line_widget::LineDrawAllFramesSelectionWidget();
    ui->mode_stacked_widget->addWidget(_drawAllFramesSelectionWidget);

    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::lineDrawingStarted,
            this, [this]() {
                std::cout << "Line drawing started for all frames mode" << std::endl;
            });
    connect(_drawAllFramesSelectionWidget, &line_widget::LineDrawAllFramesSelectionWidget::lineDrawingCompleted,
            this, [this]() {
                std::cout << "Line drawing completed for all frames mode" << std::endl;
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

    std::cout << "Show Event" << std::endl;

    // Debug: Check initial selection state
    auto initial_selections = _scene->getSelectedEntities();
    std::cout << "Debug: Initial selected entities on show: " << initial_selections.size() << std::endl;

    connect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaLine_Widget::_clickedInVideoWithModifiers);
    connect(_scene, &Media_Window::rightClickMedia, this, &MediaLine_Widget::_rightClickedInVideo);
    connect(_scene, &Media_Window::mouseMove, this, [this](qreal x, qreal y) {
        _mouseMoved(x, y);
    });
}

void MediaLine_Widget::hideEvent(QHideEvent * event) {

    static_cast<void>(event);

    std::cout << "Hide Event" << std::endl;
    
    // Guard against _scene being destroyed before hideEvent is called
    if (!_scene) {
        return;
    }
    
    disconnect(_scene, &Media_Window::leftClickMediaWithEvent, this, &MediaLine_Widget::_clickedInVideoWithModifiers);
    disconnect(_scene, &Media_Window::rightClickMedia, this, &MediaLine_Widget::_rightClickedInVideo);
    disconnect(_scene, &Media_Window::mouseMove, this, nullptr);

    // Clean up hover circle when switching away from line widget
    _scene->setShowHoverCircle(false);

    // Note: We don't disable group selection here to preserve selections
}

void MediaLine_Widget::setActiveKey(std::string const & key) {
    _active_key = key;
    ui->name_label->setText(QString::fromStdString(key));

    // Set the color picker to the current line color if available
    if (!key.empty() && _state) {
        auto const * config = _state->displayOptions().get<LineDisplayOptions>(QString::fromStdString(key));

        if (config) {
            ui->color_picker->setColor(QString::fromStdString(config->hex_color()));
            ui->color_picker->setAlpha(static_cast<int>(config->alpha() * 100));

            // Set line thickness controls
            ui->line_thickness_slider->blockSignals(true);
            ui->line_thickness_spinbox->blockSignals(true);
            ui->line_thickness_slider->setValue(config->line_thickness);
            ui->line_thickness_spinbox->setValue(config->line_thickness);
            ui->line_thickness_slider->blockSignals(false);
            ui->line_thickness_spinbox->blockSignals(false);

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

void MediaLine_Widget::_setLineAlpha(int alpha) {
    float const alpha_float = static_cast<float>(alpha) / 100;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->alpha() = alpha_float;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_setLineColor(QString const & hex_color) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->hex_color() = hex_color.toStdString();
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }
}

void MediaLine_Widget::_clickedInVideoWithModifiers(qreal x_canvas, qreal y_canvas, Qt::KeyboardModifiers modifiers) {
    if (_active_key.empty()) {
        std::cout << "No active key" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        std::cout << "No line data for active key" << std::endl;
        return;
    }

    auto const x_media = static_cast<float>(x_canvas);
    auto const y_media = static_cast<float>(y_canvas);
    auto const current_time = TimeFrameIndex(_data_manager->getCurrentTime());

    switch (_selection_mode) {
        case Selection_Mode::None: {
            std::cout << "Selection mode is None" << std::endl;
            break;
        }
        case Selection_Mode::Select: {
            std::cout << "Selection mode is Select" << std::endl;

            // Check for modifier keys to determine action
            if (modifiers & Qt::ControlModifier) {
                // Ctrl+click: Add points to selected line
                std::cout << "Ctrl+click: Adding points to selected line" << std::endl;
                _addPointToLine(x_media, y_media, current_time);
            } else if (modifiers & Qt::AltModifier) {
                // Alt+click: Erase points from selected line
                std::cout << "Alt+click: Erasing points from selected line" << std::endl;
                _erasePointsFromLine(x_media, y_media, current_time);
            } else {
                // Normal click: Select/deselect lines
                QPointF scene_pos(x_canvas * _scene->getXAspect(), y_canvas * _scene->getYAspect());
                std::string data_key, data_type;
                EntityId entity_id = _scene->findEntityAtPosition(scene_pos, data_key, data_type);

                if (entity_id != EntityId(0) && data_type == "line" && data_key == _active_key) {
                    // Use the group-based selection system for consistency
                    _scene->selectEntity(entity_id, data_key, data_type);
                    std::cout << "Selected line entity " << entity_id.id << " in group system" << std::endl;
                } else {
                    // Clear selections if no line found
                    _scene->clearAllSelections();
                    std::cout << "No line found within threshold - cleared selections" << std::endl;
                }
            }
            break;
        }
        case Selection_Mode::DrawAllFrames: {
            std::cout << "Selection mode is DrawAllFrames" << std::endl;
            _addPointToDrawAllFrames(x_media, y_media);
            break;
        }
    }
}

void MediaLine_Widget::_mouseMoved(qreal x, qreal y) {
    // Only handle mouse move in Select Line mode
    if (_selection_mode != Selection_Mode::Select) {
        return;
    }

    // Check if Alt is currently held (we can't get modifier state from mouse move,
    // so we'll track it from the last click event)
    // For now, we'll show the eraser circle when in Select mode
    // This could be improved by tracking modifier state
    static bool alt_held = false;

    // Update hover circle position and show/hide based on modifier state
    if (alt_held) {
        _scene->setShowHoverCircle(true);
        if (_eraseSelectionWidget) {
            _scene->setHoverCircleRadius(_eraseSelectionWidget->getEraserRadius());
        }
    } else {
        _scene->setShowHoverCircle(false);
    }
}

void MediaLine_Widget::_addPointToLine(float x_media, float y_media, TimeFrameIndex current_time) {
    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        std::cout << "No line selected - cannot add points" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        std::cout << "No line data for active key" << std::endl;
        return;
    }

    EntityId selected_entity_id = *selected_entities.begin();

    auto line_ref = line_data->getMutableData(selected_entity_id, NotifyObservers::Yes);
    if (!line_ref.has_value()) {
        std::cout << "Could not get mutable reference to line with EntityID " << selected_entity_id.id << std::endl;
        return;
    }

    Line2D & line = line_ref.value().get();

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

    if (_smoothing_mode == Smoothing_Mode::SimpleSmooth) {
        // Use the original smoothing approach - add point directly
        line.push_back(Point2D<float>{x_media, y_media});
    } else if (_smoothing_mode == Smoothing_Mode::PolynomialFit) {
        // If the line already exists, add interpolated points between the last point and the new point
        if (!line.empty()) {
            Point2D<float> const last_point = line.back();

            // Calculate distance between last point and new point
            float dx = x_media - last_point.x;
            float dy = y_media - last_point.y;
            float distance = std::sqrt(dx * dx + dy * dy);

            // Add interpolated points if the distance is significant
            if (distance > 5.0f) {// Threshold for adding interpolation
                int num_interp_points = std::max(2, static_cast<int>(distance / 5.0f));
                for (int i = 1; i <= num_interp_points; ++i) {
                    float t = static_cast<float>(i) / (num_interp_points + 1);
                    float interp_x = last_point.x + t * dx;
                    float interp_y = last_point.y + t * dy;
                    line.push_back(Point2D<float>{interp_x, interp_y});
                }
            }
        }

        // Add the actual new point
        line.push_back(Point2D<float>{x_media, y_media});

        // Apply polynomial fitting if we have enough points
        if (line.size() >= 3) {
            _applyPolynomialFit(line, _polynomial_order);
        }
    }

    std::cout << "Added point (" << x_media << ", " << y_media << ") to line "
              << _active_key << " (EntityID: " << selected_entity_id.id << ")" << std::endl;
}

void MediaLine_Widget::_erasePointsFromLine(float x_media, float y_media, TimeFrameIndex current_time) {
    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        std::cout << "No line selected - cannot erase points" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        std::cout << "No line data for active key" << std::endl;
        return;
    }

    EntityId selected_entity_id = *selected_entities.begin();

    auto line_ref = line_data->getMutableData(selected_entity_id, NotifyObservers::Yes);
    if (!line_ref.has_value()) {
        std::cout << "Could not get mutable reference to line with EntityID " << selected_entity_id.id << std::endl;
        return;
    }

    Line2D & line = line_ref.value().get();

    if (line.empty()) {
        std::cout << "Selected line is empty - nothing to erase" << std::endl;
        return;
    }

    // Get eraser radius from the erase selection widget
    float eraser_radius = 10.0f;// Default radius
    if (_eraseSelectionWidget) {
        eraser_radius = static_cast<float>(_eraseSelectionWidget->getEraserRadius());
    }

    // Find points within eraser radius and remove them
    std::vector<Point2D<float>> remaining_points;
    Point2D<float> click_point{x_media, y_media};

    for (auto const & point: line) {
        float distance = calc_distance(click_point, point);
        if (distance > eraser_radius) {
            remaining_points.push_back(point);
        }
    }

    // Update the line with remaining points (this modifies the original line)
    line = Line2D(remaining_points);

    // Notify observers that the data has changed
    line_data->notifyObservers();

    _scene->UpdateCanvas();
    std::cout << "Erased points near (" << x_media << ", " << y_media << ") from line "
              << _active_key << " (EntityID: " << selected_entity_id.id << ")" << std::endl;
}

void MediaLine_Widget::_applyPolynomialFit(Line2D & line, int order) {
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
    std::vector<double> x_coeffs = fit_polynomial(t, x_coords, order);
    std::vector<double> y_coeffs = fit_polynomial(t, y_coords, order);

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
        double t_param = static_cast<double>(i) / (num_points - 1);

        // Evaluate polynomials at t_param using the function from line_angle.hpp
        double x_val = evaluate_polynomial(x_coeffs, t_param);
        double y_val = evaluate_polynomial(y_coeffs, t_param);

        smooth_line.push_back(Point2D<float>{static_cast<float>(x_val), static_cast<float>(y_val)});
    }

    // Replace the original line with the smooth one
    line = Line2D(smooth_line);
}

void MediaLine_Widget::_setSmoothingMode(int index) {
    _smoothing_mode = static_cast<Smoothing_Mode>(index);
    std::cout << "Smoothing mode set to: " << index << std::endl;
}

void MediaLine_Widget::_setPolynomialOrder(int order) {
    _polynomial_order = order;
    std::cout << "Polynomial order set to: " << order << std::endl;
}

void MediaLine_Widget::_toggleSelectionMode(QString text) {
    _selection_mode = _selection_modes[text];
    std::cout << "MediaLine_Widget: Selection mode changed to: " << text.toStdString()
              << " (enum value: " << static_cast<int>(_selection_mode) << ")" << std::endl;

    // Switch to the appropriate page in the stacked widget
    int pageIndex = static_cast<int>(_selection_mode);
    ui->mode_stacked_widget->setCurrentIndex(pageIndex);

    // For Select Line mode, show both add and erase options
    if (_selection_mode == Selection_Mode::Select) {
        // Show both add and erase widgets by creating a combined layout
        // For now, we'll show the add widget as the primary options
        // The erase options will be available through the existing erase widget
    }

    // Always enable group selection for line operations
    // This prevents selections from being cleared when switching modes
    _scene->setGroupSelectionEnabled(true);
    std::cout << "MediaLine_Widget: Group selection enabled for line operations" << std::endl;

    // Debug: Check if we have any selections after mode change
    auto selected_entities = _scene->getSelectedEntities();
    std::cout << "Debug: Selected entities after mode change: " << selected_entities.size() << std::endl;

    if (_selection_mode == Selection_Mode::Select) {
        // Show hover circle for eraser when in Select mode (will be controlled by Shift key)
        _scene->setShowHoverCircle(false);// Initially off, will be controlled by mouse move events
    } else {
        _scene->setShowHoverCircle(false);
    }

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

    std::cout << "Edge snapping " << (checked ? "enabled" : "disabled") << std::endl;
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
            int num_lines = static_cast<int>(lines.size());

            std::cout << "Frame " << frame_id << ": " << num_lines << " lines in " << _active_key << std::endl;
        }
    }
}


void MediaLine_Widget::_setEdgeThreshold(int threshold) {
    _edge_threshold = threshold;
    std::cout << "Edge threshold set to: " << threshold << std::endl;
}

void MediaLine_Widget::_setEdgeSearchRadius(int radius) {
    _edge_search_radius = radius;
    std::cout << "Edge search radius set to: " << radius << std::endl;
}

void MediaLine_Widget::_detectEdges() {

    auto media = _data_manager->getData<MediaData>("media");
    if (!media) {
        std::cout << "No media data available for edge detection" << std::endl;
        return;
    }

    auto const current_time = _data_manager->getCurrentTime();
    auto frame_data = media->getProcessedData(current_time);

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

    std::cout << "Edge detection completed for frame " << current_time << std::endl;
    std::cout << "Edges detected: " << _current_edges.size() << std::endl;
}

std::pair<float, float> MediaLine_Widget::_findNearestEdge(float x, float y) {
    if (_current_edges.empty()) {
        return {x, y};
    }

    // Round x and y to integers (image coordinates)
    int x_int = static_cast<int>(std::round(x));
    int y_int = static_cast<int>(std::round(y));

    // Define search radius and initialize variables
    int radius = _edge_search_radius;
    float min_distance = radius * radius + 1;     // Initialize to something larger than possible
    std::pair<float, float> nearest_edge = {x, y};// Default to original point

    // Get image dimensions
    int width = _current_edges.cols;
    int height = _current_edges.rows;

    // Check if the point is within image bounds
    if (x_int < 0 || x_int >= width || y_int < 0 || y_int >= height) {
        std::cout << "Click point outside image bounds" << std::endl;
        return {x, y};
    }

    // Search in a square region around the clicked point
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate current point to check
            int nx = x_int + dx;
            int ny = y_int + dy;

            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
            }

            // Check if this is an edge pixel
            if (_current_edges.at<uchar>(ny, nx) > 0) {
                // Calculate distance squared (avoid square root for performance)
                float d_squared = dx * dx + dy * dy;

                // Update nearest edge if this is closer
                if (d_squared < min_distance) {
                    min_distance = d_squared;
                    nearest_edge = {static_cast<float>(nx), static_cast<float>(ny)};
                }
            }
        }
    }

    if (min_distance < radius * radius + 1) {
        std::cout << "Found edge point at (" << nearest_edge.first << ", "
                  << nearest_edge.second << "), distance: " << std::sqrt(min_distance) << std::endl;
    } else {
        std::cout << "No edge found within radius " << radius << std::endl;
        cv::imwrite("edges.png", _current_edges);
    }

    return nearest_edge;
}

void MediaLine_Widget::_setEraserRadius(int radius) {
    if (_selection_mode == Selection_Mode::Erase) {
        _scene->setHoverCircleRadius(static_cast<double>(radius));
    }
    std::cout << "Eraser radius set to: " << radius << std::endl;
}

void MediaLine_Widget::_toggleShowHoverCircle(bool checked) {
    _scene->setShowHoverCircle(checked);
    std::cout << "Show hover circle " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::_setLineThickness(int thickness) {
    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            line_opts->line_thickness = thickness;
            _state->displayOptions().notifyChanged<LineDisplayOptions>(key);
        }
        _scene->UpdateCanvas();
    }

    // Synchronize slider and spinbox if the signal came from one of them
    QObject * sender_obj = sender();
    if (sender_obj == ui->line_thickness_slider) {
        ui->line_thickness_spinbox->blockSignals(true);
        ui->line_thickness_spinbox->setValue(thickness);
        ui->line_thickness_spinbox->blockSignals(false);
    } else if (sender_obj == ui->line_thickness_spinbox) {
        ui->line_thickness_slider->blockSignals(true);
        ui->line_thickness_slider->setValue(thickness);
        ui->line_thickness_slider->blockSignals(false);
    }

    std::cout << "Line thickness set to: " << thickness << std::endl;
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
    std::cout << "Show position marker " << (checked ? "enabled" : "disabled") << std::endl;
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

    std::cout << "Position percentage set to: " << percentage << std::endl;
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
    std::cout << "Show segment " << (checked ? "enabled" : "disabled") << std::endl;
}

void MediaLine_Widget::_setSegmentStartPercentage(int percentage) {
    if (_is_updating_percentages) return;
    _is_updating_percentages = true;

    if (!_active_key.empty() && _state) {
        auto const key = QString::fromStdString(_active_key);
        auto * line_opts = _state->displayOptions().getMutable<LineDisplayOptions>(key);
        if (line_opts) {
            // Ensure start percentage doesn't exceed end percentage - 1%
            int max_start_percentage = line_opts->segment_end_percentage - 1;
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

    std::cout << "Segment start percentage set to: " << percentage << std::endl;
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
            int min_end_percentage = line_opts->segment_start_percentage + 1;
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

    std::cout << "Segment end percentage set to: " << percentage << std::endl;
    _is_updating_percentages = false;
}

void MediaLine_Widget::_rightClickedInVideo(qreal x_canvas, qreal y_canvas) {
    // Only handle right-clicks in Select mode and when a line is selected
    if (_selection_mode != Selection_Mode::Select || _active_key.empty()) {
        return;
    }

    // Get the EntityID for the selected line from the group system
    auto selected_entities = _scene->getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }

    EntityId selected_entity_id = *selected_entities.begin();

    auto x_media = static_cast<float>(x_canvas);
    auto y_media = static_cast<float>(y_canvas);

    // Check if the right-click is near the selected line
    auto nearest_entity_id = _findNearestLine(x_media, y_media);
    if (nearest_entity_id.has_value() && nearest_entity_id.value() == selected_entity_id) {
        // Show context menu at the click position
        QPoint global_pos = QCursor::pos();
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
    float dx = line_end.x - line_start.x;
    float dy = line_end.y - line_start.y;

    // Handle degenerate case where line segment is actually a point
    if (dx == 0.0f && dy == 0.0f) {
        return calc_distance(point, line_start);
    }

    // Calculate the parameter t for the closest point on the line segment
    float t = ((point.x - line_start.x) * dx + (point.y - line_start.y) * dy) / (dx * dx + dy * dy);

    // Clamp t to [0, 1] to stay within the line segment
    t = std::max(0.0f, std::min(1.0f, t));

    // Calculate the closest point on the line segment
    Point2D<float> closest_point = {
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

    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());
    auto entity_ids = line_data->getEntityIdsAtTime(current_time);
    auto lines = line_data->getAtTime(current_time);

    if (lines.empty()) {
        return std::nullopt;
    }

    Point2D<float> click_point{x, y};
    std::optional<EntityId> nearest_entity_id = std::nullopt;
    float min_distance = _line_selection_threshold + 1;// Initialize beyond threshold

    for (size_t i = 0; i < lines.size(); ++i) {
        auto const & line = lines[i];
        auto const entity_id = entity_ids[i];

        if (line.empty()) {
            continue;
        }

        // Calculate distance to each line segment
        float min_segment_distance = std::numeric_limits<float>::max();
        for (size_t i = 0; i < line.size() - 1; ++i) {
            float segment_distance = _calculateDistanceToLineSegment(click_point, line[i], line[i + 1]);
            if (segment_distance < min_segment_distance) {
                min_segment_distance = segment_distance;
            }
        }

        // Also check distance to vertices for completeness
        float min_vertex_distance = std::numeric_limits<float>::max();
        for (auto const & vertex: line) {
            float vertex_distance = calc_distance(click_point, vertex);
            if (vertex_distance < min_vertex_distance) {
                min_vertex_distance = vertex_distance;
            }
        }

        float line_distance = std::min(min_vertex_distance, min_segment_distance);

        if (line_distance < min_distance) {
            min_distance = line_distance;
            nearest_entity_id = entity_id;
            std::cout << "  -> New closest line with EntityID: " << entity_id.id << std::endl;
        }
    }

    return (min_distance <= _line_selection_threshold) ? nearest_entity_id : std::nullopt;
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

    EntityId selected_entity_id = *selected_entities.begin();

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data || !target_line_data) {
        std::cerr << "Could not retrieve source or target LineData" << std::endl;
        return;
    }

    // Get the selected line data by EntityID
    auto selected_line_opt = source_line_data->getDataByEntityId(selected_entity_id);
    if (!selected_line_opt.has_value()) {
        std::cerr << "Could not find line with EntityID " << selected_entity_id.id << std::endl;
        return;
    }

    Line2D selected_line = selected_line_opt.value().get();

    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());

    // Add to target
    target_line_data->addAtTime(current_time, selected_line, NotifyObservers::No);

    // Remove from source using clearByEntityId
    if (!source_line_data->clearByEntityId(selected_entity_id, NotifyObservers::No)) {
        std::cerr << "Could not clear line with EntityID " << selected_entity_id.id << std::endl;
        return;
    }

    // Clear selection since the line was moved
    _scene->clearAllSelections();
    source_line_data->notifyObservers();
    target_line_data->notifyObservers();

    std::cout << "Moved line with EntityID " << selected_entity_id.id << " from " << _active_key << " to " << target_key << std::endl;
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

    EntityId selected_entity_id = *selected_entities.begin();

    auto source_line_data = _data_manager->getData<LineData>(_active_key);
    auto target_line_data = _data_manager->getData<LineData>(target_key);

    if (!source_line_data || !target_line_data) {
        std::cerr << "Could not retrieve source or target LineData" << std::endl;
        return;
    }

    // Get the selected line data by EntityID
    auto selected_line_opt = source_line_data->getDataByEntityId(selected_entity_id);
    if (!selected_line_opt.has_value()) {
        std::cerr << "Could not find line with EntityID " << selected_entity_id.id << std::endl;
        return;
    }

    Line2D selected_line = selected_line_opt.value().get();

    auto current_time = TimeFrameIndex(_data_manager->getCurrentTime());

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
        std::cout << "Cannot apply line to all frames: widget not available or no active key" << std::endl;
        return;
    }

    auto line_points = _drawAllFramesSelectionWidget->getCurrentLinePoints();
    if (line_points.empty()) {
        std::cout << "No line points to apply to all frames" << std::endl;
        return;
    }

    auto line_data = _data_manager->getData<LineData>(_active_key);
    if (!line_data) {
        std::cout << "No line data available for active key" << std::endl;
        return;
    }

    // Get all frame times
    auto frame_times = _getAllFrameTimes();
    if (frame_times.empty()) {
        std::cout << "No frame times available" << std::endl;
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
                float default_xAspect = _scene->getXAspect();
                float default_yAspect = _scene->getYAspect();
                float line_xAspect = static_cast<float>(_scene->getCanvasSize().first) / image_size.width;
                float line_yAspect = static_cast<float>(_scene->getCanvasSize().second) / image_size.height;

                // Convert each point from default media coordinates to line-specific coordinates
                for (auto const & point: line_points) {
                    float x_canvas = point.x * default_xAspect;
                    float y_canvas = point.y * default_yAspect;
                    float x_converted = x_canvas / line_xAspect;
                    float y_converted = y_canvas / line_yAspect;
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
        std::cout << "No media data available" << std::endl;
        return frame_times;
    }

    int total_frames = media_data->getTotalFrameCount();
    if (total_frames <= 0) {
        std::cout << "Invalid frame count: " << total_frames << std::endl;
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

    std::cout << "Debug: Selected entities count: " << selected_entities.size() << std::endl;

    // If no entities are selected, return nullopt
    if (selected_entities.empty()) {
        std::cout << "Debug: No entities selected" << std::endl;
        return std::nullopt;
    }

    // Return the first selected entity ID
    EntityId first_selected = *selected_entities.begin();
    std::cout << "Debug: First selected entity ID: " << first_selected.id << std::endl;
    return first_selected;
}

void MediaLine_Widget::_updateTemporaryLineFromWidget() {
    if (_drawAllFramesSelectionWidget) {
        auto current_points = _drawAllFramesSelectionWidget->getCurrentLinePoints();
        _scene->updateTemporaryLine(current_points, "");// Use default aspect ratios
    }
}
