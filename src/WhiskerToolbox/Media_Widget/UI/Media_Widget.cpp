#include "Media_Widget.hpp"
#include "ui_Media_Widget.h"

#include "Core/MediaWidgetState.hpp"
#include "MediaRulerViewport.hpp"
#include "Rendering/Media_Window/Media_Window.hpp"
#include "Rulers/RulerCornerWidget.hpp"
#include "Selection/MediaSelectToolController.hpp"
#include "Tools/MediaToolId.hpp"
#include "Tools/MediaToolOptionsBar_Widget.hpp"
#include "Tools/MediaToolStrip_Widget.hpp"

#include "Plots/Common/AxisTickLayout.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/StrongTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include <QApplication>
#include <QGraphicsView>
#include <QGridLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

Media_Widget::Media_Widget(EditorRegistry * editor_registry, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Media_Widget),
      _editor_registry{editor_registry} {
    assert(editor_registry != nullptr && "EditorRegistry must not be null");
    ui->setupUi(this);

    _setupRulerLayout();

    // Install event filter on graphics view viewport for wheel zoom
    if (ui->graphicsView && ui->graphicsView->viewport()) {
        ui->graphicsView->viewport()->installEventFilter(this);
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        ui->graphicsView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    }

    // === Phase 2.4: Editor State Integration ===
    // Initialize state and register with EditorRegistry for serialization and inter-widget communication

    _state = std::make_shared<MediaWidgetState>();
    _connectStateSignals();


    _editor_registry->registerState(_state);
    _selection_context = _editor_registry->selectionContext();

    // Connect to global time changes for frame loading
    // Use the new timeChanged(TimePosition) signal (preferred)
    connect(_editor_registry,
            QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
            this, &Media_Widget::LoadFrame);


    // Connect to SelectionContext to respond to    external selection changes
    // When another widget (e.g., DataManager_Widget) selects data, update our state
    if (_selection_context) {
        connect(_selection_context, &SelectionContext::selectionChanged,
                this, &Media_Widget::_onExternalSelectionChanged);
    }
}

Media_Widget::~Media_Widget() {
    // Unregister state from EditorRegistry when widget is destroyed
    // Note: During application shutdown, _editor_registry may already be destroyed
    // before Qt's widget tree cleanup runs. We guard with a null check, but the
    // caller (MainWindow) should ensure proper destruction order.
    if (_editor_registry && _state) {
        _editor_registry->unregisterState(EditorInstanceId(_state->getInstanceId()));
    }

    // Ensure hover circle is cleared before scene destruction
    if (_scene) {
        _scene->setShowHoverCircle(false);
    }

    delete ui;
}

void Media_Widget::updateMedia() {

    ui->graphicsView->setScene(_scene.get());
    ui->graphicsView->show();

    _updateCanvasSize();
}

void Media_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    // Create the Media_Window now that we have a DataManager
    _createMediaWindow();
    _createOptions();

    _data_manager->addObserver([this]() {
        _pruneRemovedFeatures();
        _createOptions();
    },
                               "Media_Widget");

    // Wire up the scene to the graphics view immediately so that
    // data overlays render without requiring a separate media load.
    updateMedia();
}

void Media_Widget::_pruneRemovedFeatures() {
    if (!_data_manager || !_state || !_scene) {
        return;
    }

    auto const dm_keys = _data_manager->getAllKeys();
    auto key_still_present = [&](std::string const & key) {
        return std::ranges::find(dm_keys, key) != dm_keys.end();
    };

    auto remove_callbacks = [&](std::string const & key) {
        if (_callback_ids.count(key) == 0) {
            return;
        }
        for (auto callback_id: _callback_ids[key]) {
            _data_manager->removeCallbackFromData(key, callback_id);
        }
        _callback_ids.erase(key);
    };

    for (QString const & key_q: _state->displayOptions().keys<LineDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key) || _data_manager->getType(key) != DM_DataType::Line) {
            remove_callbacks(key);
            _scene->removeLineDataFromScene(key);
        }
    }

    for (QString const & key_q: _state->displayOptions().keys<MaskDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key) || _data_manager->getType(key) != DM_DataType::Mask) {
            remove_callbacks(key);
            _scene->removeMaskDataFromScene(key);
        }
    }

    for (QString const & key_q: _state->displayOptions().keys<PointDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key) || _data_manager->getType(key) != DM_DataType::Points) {
            remove_callbacks(key);
            _scene->removePointDataFromScene(key);
        }
    }

    for (QString const & key_q: _state->displayOptions().keys<DigitalIntervalDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key) || _data_manager->getType(key) != DM_DataType::DigitalInterval) {
            remove_callbacks(key);
            _scene->removeDigitalIntervalSeries(key);
        }
    }

    for (QString const & key_q: _state->displayOptions().keys<TensorDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key) || _data_manager->getType(key) != DM_DataType::Tensor) {
            remove_callbacks(key);
            _scene->removeTensorDataFromScene(key);
        }
    }

    for (QString const & key_q: _state->displayOptions().keys<MediaDisplayOptions>()) {
        std::string const key = key_q.toStdString();
        if (!key_still_present(key)) {
            remove_callbacks(key);
            _scene->removeMediaDataFromScene(key);
            continue;
        }

        auto const type = _data_manager->getType(key);
        if (type != DM_DataType::Video && type != DM_DataType::Images) {
            remove_callbacks(key);
            _scene->removeMediaDataFromScene(key);
        }
    }
}

void Media_Widget::_createOptions() {

    //Setup Media Data
    auto media_keys = _data_manager->getKeys<MediaData>();
    for (auto const & media_key: media_keys) {
        if (_state->displayOptions().has<MediaDisplayOptions>(QString::fromStdString(media_key))) continue;

        _scene->addMediaDataToScene(media_key);
    }

    // Setup line data
    auto line_keys = _data_manager->getKeys<LineData>();
    for (auto const & line_key: line_keys) {
        auto opts = _scene->getLineConfig(line_key);
        if (opts.has_value()) continue;

        _scene->addLineDataToScene(line_key);
    }

    // Setup mask data
    auto mask_keys = _data_manager->getKeys<MaskData>();
    for (auto const & mask_key: mask_keys) {
        auto opts = _scene->getMaskConfig(mask_key);
        if (opts.has_value()) continue;

        _scene->addMaskDataToScene(mask_key);
    }

    // Setup point data
    auto point_keys = _data_manager->getKeys<PointData>();
    for (auto const & point_key: point_keys) {
        auto opts = _scene->getPointConfig(point_key);
        if (opts.has_value()) continue;

        _scene->addPointDataToScene(point_key);
    }

    // Setup digital interval data
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    for (auto const & interval_key: interval_keys) {
        auto opts = _scene->getIntervalConfig(interval_key);
        if (opts.has_value()) continue;

        _scene->addDigitalIntervalSeries(interval_key);
    }

    // Setup tensor data
    auto tensor_keys = _data_manager->getKeys<TensorData>();
    for (auto const & tensor_key: tensor_keys) {
        auto opts = _scene->getTensorConfig(tensor_key);
        if (opts.has_value()) continue;

        _scene->addTensorDataToScene(tensor_key);
    }
}

void Media_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    _updateRulers();
    // When user has zoomed, avoid rescaling scene contents destructively; just adjust scene rect
    if (_isUserZoomActive()) {
        if (_scene) {
            auto size = ui->graphicsView->size();
            _scene->setSceneRect(0, 0, size.width(), size.height());
        }
    } else {
        _updateCanvasSize();
    }
}

void Media_Widget::_updateCanvasSize() {
    if (_scene) {
        int const width = ui->graphicsView->width();
        int const height = ui->graphicsView->height();

        _scene->setCanvasSize(
                ImageSize{width, height});
        _scene->UpdateCanvas();

        // Ensure the view fits the scene properly
        ui->graphicsView->setSceneRect(0, 0, width, height);
        if (!_isUserZoomActive()) {
            ui->graphicsView->resetTransform();
            if (_state) {
                _state->setZoom(1.0);
            }
        }

        // Sync canvas size to state
        _syncCanvasSizeToState();
    }
}

void Media_Widget::_addFeatureToDisplay(QString const & feature, bool enabled) {
    std::cout << "Feature: " << feature.toStdString() << std::endl;

    auto const feature_key = feature.toStdString();
    auto const type = _data_manager->getType(feature_key);
    QString state_type;// Type string for state synchronization

    if (type == DM_DataType::Line) {
        state_type = QStringLiteral("line");
        if (!_state->displayOptions().setVisible(feature, state_type, enabled)) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
    } else if (type == DM_DataType::Mask) {
        state_type = QStringLiteral("mask");
        if (!_state->displayOptions().setVisible(feature, state_type, enabled)) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
    } else if (type == DM_DataType::Points) {
        state_type = QStringLiteral("point");
        if (!_state->displayOptions().setVisible(feature, state_type, enabled)) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
    } else if (type == DM_DataType::DigitalInterval) {
        auto opts = _scene->getIntervalConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible() = enabled;
        state_type = QStringLiteral("interval");
    } else if (type == DM_DataType::Tensor) {
        auto opts = _scene->getTensorConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }

        if (enabled) {
            std::cout << "Enabling tensor data in scene" << std::endl;
            opts.value()->is_visible() = true;
        } else {
            std::cout << "Disabling tensor data from scene" << std::endl;
            opts.value()->is_visible() = false;
        }
        state_type = QStringLiteral("tensor");
    } else if (type == DM_DataType::Video || type == DM_DataType::Images) {
        auto const key = QString::fromStdString(feature_key);
        auto const * opts = _state->displayOptions().get<MediaDisplayOptions>(key);
        if (!opts) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        auto modified = *opts;
        if (enabled) {
            std::cout << "Enabling media data in scene" << std::endl;
            modified.is_visible() = true;
            _state->displayOptions().set(key, modified);

            // This ensures new media is loaded from disk
            // Before the update
            LoadFrame(_state->current_position);

        } else {
            std::cout << "Disabling media data from scene" << std::endl;
            modified.is_visible() = false;
            _state->displayOptions().set(key, modified);
        }
        state_type = QStringLiteral("media");
    } else {
        std::cout << "Feature type " << convert_data_type_to_string(type) << " not supported" << std::endl;
    }

    // Line, mask, and point visibility is updated via DisplayOptionsRegistry::setVisible(),
    // which emits featureEnabledChanged. Other types still sync below.
    if (!state_type.isEmpty() &&
        state_type != QStringLiteral("line") &&
        state_type != QStringLiteral("mask") &&
        state_type != QStringLiteral("point")) {
        _syncFeatureEnabledToState(feature, state_type, enabled);
    }

    _scene->UpdateCanvas();

    if (enabled) {
        _callback_ids[feature_key].push_back(_data_manager->addCallbackToData(feature_key, [this]() {
            _scene->UpdateCanvas();
        }));
    } else {
        for (auto callback_id: _callback_ids[feature_key]) {
            _data_manager->removeCallbackFromData(feature_key, callback_id);
        }
        _callback_ids[feature_key].clear();
    }
}

void Media_Widget::setFeatureEnabled(QString const & feature, bool enabled) {
    _addFeatureToDisplay(feature, enabled);
}

void Media_Widget::setFeatureColor(std::string const & feature, std::string const & hex_color) {
    auto const type = _data_manager->getType(feature);

    if (type == DM_DataType::Line) {
        auto opts = _scene->getLineConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::Mask) {
        auto opts = _scene->getMaskConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::Points) {
        auto opts = _scene->getPointConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::DigitalInterval) {
        auto opts = _scene->getIntervalConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::Tensor) {
        auto opts = _scene->getTensorConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color() = hex_color;
        }
    }

    _scene->UpdateCanvas();
}

void Media_Widget::LoadFrame(TimePosition const & position) {

    if (_scene == nullptr) {
        std::cout << "Scene is null during LoadFrame" << std::endl;
        return;
    }

    if (!position.isValid()) {
        std::cout << "Position is invalid during LoadFrame" << std::endl;
        std::cout << "Position: " << position.index.getValue() << std::endl;
        return;
    }

    if (_data_manager == nullptr) {
        std::cout << "Data manager is null during LoadFrame" << std::endl;
        return;
    }

    _state->current_position = position;

    _scene->LoadFrame(position);
}

// Zoom API implementations
void Media_Widget::zoomIn() { _applyZoom(_zoom_step, false); }
void Media_Widget::zoomOut() { _applyZoom(1.0 / _zoom_step, false); }
void Media_Widget::resetZoom() {
    if (!ui->graphicsView || !_state) return;
    ui->graphicsView->resetTransform();
    _state->setZoom(1.0);
}

void Media_Widget::_applyZoom(double factor, bool anchor_under_mouse) {
    if (!ui->graphicsView || !_state) return;
    double const current_zoom = _state->zoom();
    double new_zoom = current_zoom * factor;
    new_zoom = std::clamp(new_zoom, _min_zoom, _max_zoom);
    factor = new_zoom / current_zoom;// Adjust factor if clamped
    if (qFuzzyCompare(factor, 1.0)) return;
    if (anchor_under_mouse) {
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    } else {
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }
    ui->graphicsView->scale(factor, factor);
    _state->setZoom(new_zoom);
    _updateRulers();
}

bool Media_Widget::eventFilter(QObject * watched, QEvent * event) {
    if (watched == ui->graphicsView->viewport()) {
        // Handle wheel events for zoom
        if (event->type() == QEvent::Wheel) {
            auto * wheelEvent = dynamic_cast<QWheelEvent *>(event);
            double const angle = wheelEvent->angleDelta().y();
            if (angle > 0) {
                _applyZoom(_zoom_step, true);
            } else if (angle < 0) {
                _applyZoom(1.0 / _zoom_step, true);
            }
            wheelEvent->accept();
            return true;
        }

        // Handle mouse events for shift+drag panning
        if (event->type() == QEvent::MouseButtonPress) {
            auto * mouseEvent = dynamic_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->modifiers() & Qt::ShiftModifier) {
                _is_panning = true;
                _last_pan_point = mouseEvent->pos();
                ui->graphicsView->viewport()->setCursor(Qt::ClosedHandCursor);
                mouseEvent->accept();
                return true;// Consume the event to prevent it from reaching Media_Window
            }
        }

        if (event->type() == QEvent::MouseMove && _is_panning) {
            auto * mouseEvent = dynamic_cast<QMouseEvent *>(event);
            QPoint const delta = mouseEvent->pos() - _last_pan_point;
            _last_pan_point = mouseEvent->pos();

            // Apply panning by translating the view
            ui->graphicsView->horizontalScrollBar()->setValue(
                    ui->graphicsView->horizontalScrollBar()->value() - delta.x());
            ui->graphicsView->verticalScrollBar()->setValue(
                    ui->graphicsView->verticalScrollBar()->value() - delta.y());

            _updateRulers();
            mouseEvent->accept();
            return true;// Consume the event
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto * mouseEvent = dynamic_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && _is_panning) {
                _is_panning = false;
                ui->graphicsView->viewport()->setCursor(Qt::ArrowCursor);

                // Sync final pan position to state directly
                if (_state && ui->graphicsView) {
                    double const pan_x = ui->graphicsView->horizontalScrollBar()->value();
                    double const pan_y = ui->graphicsView->verticalScrollBar()->value();
                    _state->setPan(pan_x, pan_y);
                }

                mouseEvent->accept();
                return true;// Consume the event
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void Media_Widget::_createMediaWindow() {
    if (_data_manager) {
        _scene = std::make_unique<Media_Window>(_data_manager, this);

        // Set parent widget reference for accessing enabled media keys
        _scene->setParentWidget(this);

        // Connect Media_Window to state for display options synchronization
        if (_state) {
            _scene->setMediaWidgetState(_state.get());
        }

        if (_select_controller) {
            _select_controller->setMediaWindow(_scene.get());
        }

        connect(_scene.get(), &Media_Window::groupSelectionInteracted, this, [this]() {
            if (!_selection_context || !_state) {
                return;
            }
            auto const instance_id = EditorLib::EditorInstanceId(_state->getInstanceId());
            _selection_context->setActiveEditor(instance_id);
            _selection_context->notifyInteraction(instance_id);
        });
    }
}

void Media_Widget::_onExternalSelectionChanged(SelectionSource const & source) {
    if (!_selection_context || !_state) {
        return;
    }

    // Don't respond to our own selection changes to avoid circular updates
    if (source.editor_instance_id.toString() == _state->getInstanceId()) {
        return;
    }

    QString const selected_key = _selection_context->primarySelectedData().toString();
    if (selected_key.isEmpty()) {
        return;
    }

    // Update our state with the externally selected data key
    // This tracks what was selected elsewhere
    // Note: This intentionally doesn't update the Feature_Table_Widget highlight -
    // the feature table will be removed in Phase 3, so we keep them decoupled for now
    _state->setDisplayedDataKey(selected_key);
}

// === Phase 4E: State as Single Source of Truth ===

bool Media_Widget::_isUserZoomActive() const {
    if (!_state) return false;
    return std::abs(_state->zoom() - 1.0) > 1e-6;
}

void Media_Widget::_syncCanvasSizeToState() {
    if (_state && _scene) {
        auto [width, height] = _scene->getCanvasSize();
        _state->setCanvasSize(width, height);
    }
}

void Media_Widget::_syncFeatureEnabledToState(QString const & feature_key, QString const & data_type, bool enabled) {
    if (_state) {
        _state->setFeatureEnabled(feature_key, data_type, enabled);
    }
}

void Media_Widget::_connectStateSignals() {
    if (!_state) return;

    // Connect to state signals to respond to external state changes
    // (e.g., from properties panel or workspace restore)
    connect(_state.get(), &MediaWidgetState::zoomChanged,
            this, &Media_Widget::_onStateZoomChanged);
    connect(_state.get(), &MediaWidgetState::panChanged,
            this, &Media_Widget::_onStatePanChanged);
    connect(_state.get(), &MediaWidgetState::rulerPrefsChanged,
            this, &Media_Widget::_applyRulerPrefs);
    connect(_state.get(), &MediaWidgetState::canvasCoordinateSystemChanged,
            this, &Media_Widget::_updateRulers);

    if (ui->graphicsView) {
        connect(ui->graphicsView->horizontalScrollBar(), &QScrollBar::valueChanged,
                this, &Media_Widget::_updateRulers);
        connect(ui->graphicsView->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &Media_Widget::_updateRulers);
    }

    _wireToolUi();
    _applyRulerPrefs();
}

void Media_Widget::_syncActiveMediaTool(MediaToolId tool) {
    if (_tool_strip) {
        _tool_strip->setActiveTool(tool);
    }
    if (_tool_options_bar) {
        _tool_options_bar->setActiveTool(tool);
    }
    if (_select_controller) {
        _select_controller->setActive(tool == MediaToolId::Select);
    }
    _syncBrushHoverCircle();
}

void Media_Widget::_syncBrushHoverCircle() {
    if (!_scene || !_state) {
        return;
    }

    switch (_state->activeMediaTool()) {
        case MediaToolId::Eraser:
            _scene->setHoverCircleRadius(_state->eraserPrefs().radius_px);
            _scene->setShowHoverCircle(true);
            return;
        case MediaToolId::Smooth:
            _scene->setHoverCircleRadius(_state->smoothPrefs().radius_px);
            _scene->setShowHoverCircle(true);
            return;
        case MediaToolId::None:
        case MediaToolId::Select:
        case MediaToolId::Pen:
        default:
            break;
    }

    _scene->setShowHoverCircle(false);
}

void Media_Widget::_wireToolUi() {
    if (!_tool_strip || !_tool_options_bar || !_state) {
        return;
    }

    qRegisterMetaType<MediaToolId>("MediaToolId");

    if (!_select_controller) {
        _select_controller = new MediaSelectToolController(this);
        _select_controller->setMediaWindow(_scene.get());
        _select_controller->setState(_state.get());
    }

    _tool_options_bar->setState(_state.get());

    connect(_tool_strip, &MediaToolStrip_Widget::activeToolChanged,
            this, [this](MediaToolId tool) {
                _state->setActiveMediaTool(tool);
            });

    connect(_state.get(), &MediaWidgetState::activeMediaToolChanged,
            this, [this](MediaToolId tool) {
                _syncActiveMediaTool(tool);
            });

    connect(_state.get(), &MediaWidgetState::eraserPrefsChanged,
            this, [this]() {
                _syncBrushHoverCircle();
            });

    connect(_state.get(), &MediaWidgetState::smoothPrefsChanged,
            this, [this]() {
                _syncBrushHoverCircle();
            });

    _syncActiveMediaTool(_state->activeMediaTool());
}

void Media_Widget::_setupRulerLayout() {
    if (!ui->graphicsView) {
        return;
    }

    auto * old_layout = ui->horizontalLayout;
    if (old_layout) {
        old_layout->removeWidget(ui->graphicsView);
    }

    _tool_strip = new MediaToolStrip_Widget(this);
    _tool_options_bar = new MediaToolOptionsBar_Widget(this);
    _ruler_corner = new RulerCornerWidget(this);
    _horizontal_ruler = new HorizontalAxisWidget(this);
    _vertical_ruler = new VerticalAxisWidget(this);

    _horizontal_ruler->setDisplayMode(Neuralyzer::Plots::AxisDisplayMode::Ruler);
    _vertical_ruler->setDisplayMode(Neuralyzer::Plots::AxisDisplayMode::Ruler);
    _vertical_ruler->setInverted(true);

    auto * grid_layout = new QGridLayout();
    grid_layout->setSpacing(0);
    grid_layout->setContentsMargins(0, 0, 0, 0);
    grid_layout->addWidget(_tool_strip, 0, 0, 3, 1);
    grid_layout->addWidget(_tool_options_bar, 0, 1, 1, 2);
    grid_layout->addWidget(_ruler_corner, 1, 1);
    grid_layout->addWidget(_horizontal_ruler, 1, 2);
    grid_layout->addWidget(_vertical_ruler, 2, 1);
    grid_layout->addWidget(ui->graphicsView, 2, 2);
    grid_layout->setColumnStretch(2, 1);
    grid_layout->setRowStretch(2, 1);


    delete old_layout;

    setLayout(grid_layout);

    _horizontal_ruler->setRangeGetter([this]() -> std::pair<double, double> {
        if (!_scene) {
            return {0.0, 1.0};
        }
        auto const vp = computeVisibleMediaViewport(
                *ui->graphicsView,
                _scene->getXAspect(),
                _scene->getYAspect());
        return {vp.min_x, vp.max_x};
    });

    _vertical_ruler->setRangeGetter([this]() -> std::pair<double, double> {
        if (!_scene) {
            return {0.0, 1.0};
        }
        auto const vp = computeVisibleMediaViewport(
                *ui->graphicsView,
                _scene->getXAspect(),
                _scene->getYAspect());
        return {vp.min_y, vp.max_y};
    });
}

void Media_Widget::_applyRulerPrefs() {
    if (!_state || !_horizontal_ruler || !_vertical_ruler) {
        return;
    }

    RulerPrefs const & prefs = _state->rulerPrefs();

    bool const visible = prefs.enabled;
    if (_ruler_corner) {
        _ruler_corner->setVisible(visible);
    }
    _horizontal_ruler->setVisible(visible);
    _vertical_ruler->setVisible(visible);

    Neuralyzer::Plots::AxisTickConfig tick_config;
    tick_config.mode = prefs.tick_mode == RulerTickMode::Fixed ? Neuralyzer::Plots::AxisTickMode::Fixed
                                                               : Neuralyzer::Plots::AxisTickMode::Auto;
    tick_config.fixed_interval = static_cast<double>(prefs.fixed_interval_px);
    tick_config.target_tick_count = prefs.target_tick_count;
    tick_config.show_minor_ticks = prefs.show_minor_ticks;

    _horizontal_ruler->setTickConfig(tick_config);
    _vertical_ruler->setTickConfig(tick_config);

    QColor const negative_color(QString::fromStdString(prefs.negative_color));
    _horizontal_ruler->setNegativeLabelColor(negative_color);
    _vertical_ruler->setNegativeLabelColor(negative_color);

    _updateRulers();
}

void Media_Widget::_updateRulers() {
    if (_horizontal_ruler) {
        _horizontal_ruler->update();
    }
    if (_vertical_ruler) {
        _vertical_ruler->update();
    }
}

void Media_Widget::_onStateZoomChanged(double zoom) {
    // State is the source of truth - update the QGraphicsView to match
    // This handles both internal changes and external changes (e.g., workspace restore)
    if (!ui->graphicsView) return;

    // Get current transform scale
    QTransform const transform = ui->graphicsView->transform();
    double const current_scale = transform.m11();// Assumes uniform scaling

    // Only apply if different from current transform (avoid feedback loop)
    if (std::abs(current_scale - zoom) > 1e-6) {
        // Calculate scale factor to reach target zoom
        double const factor = zoom / current_scale;
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        ui->graphicsView->scale(factor, factor);
    }
    _updateRulers();
}

void Media_Widget::_onStatePanChanged(double x, double y) {
    if (!ui->graphicsView) return;

    // Only apply if different from current pan
    int const current_x = ui->graphicsView->horizontalScrollBar()->value();
    int const current_y = ui->graphicsView->verticalScrollBar()->value();

    if (std::abs(current_x - x) > 0.5 || std::abs(current_y - y) > 0.5) {
        ui->graphicsView->horizontalScrollBar()->setValue(static_cast<int>(x));
        ui->graphicsView->verticalScrollBar()->setValue(static_cast<int>(y));
    }
    _updateRulers();
}

void Media_Widget::restoreFromState() {
    if (!_state) return;

    // Restore zoom - state is the source of truth
    double const saved_zoom = _state->zoom();
    if (saved_zoom > 0 && ui->graphicsView) {
        ui->graphicsView->resetTransform();
        if (std::abs(saved_zoom - 1.0) > 1e-6) {
            ui->graphicsView->scale(saved_zoom, saved_zoom);
        }
    }

    // Restore pan position
    auto [pan_x, pan_y] = _state->pan();
    if (ui->graphicsView) {
        ui->graphicsView->horizontalScrollBar()->setValue(static_cast<int>(pan_x));
        ui->graphicsView->verticalScrollBar()->setValue(static_cast<int>(pan_y));
    }

    // Restore enabled features by iterating through the state's options
    // and setting is_visible on the corresponding Media_Window configs
    auto const & data = _state->data();

    // Restore line features
    for (auto const & [key, opts]: data.line_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Restore mask features
    for (auto const & [key, opts]: data.mask_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Restore point features
    for (auto const & [key, opts]: data.point_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Restore tensor features
    for (auto const & [key, opts]: data.tensor_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Restore interval features
    for (auto const & [key, opts]: data.interval_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Restore media features
    for (auto const & [key, opts]: data.media_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }

    // Update canvas to reflect restored state
    if (_scene) {
        _scene->UpdateCanvas();
    }

    _applyRulerPrefs();
}