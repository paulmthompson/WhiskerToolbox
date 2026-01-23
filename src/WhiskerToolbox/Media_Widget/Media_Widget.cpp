#include "Media_Widget.hpp"
#include "ui_Media_Widget.h"

#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Media_Window/Media_Window.hpp"
#include "MediaWidgetState.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QApplication>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

Media_Widget::Media_Widget(EditorRegistry * editor_registry, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Media_Widget),
      _editor_registry{editor_registry} {
    ui->setupUi(this);

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

    if (_editor_registry) {
        _editor_registry->registerState(_state);
        _selection_context = _editor_registry->selectionContext();

        // Connect to global time changes for frame loading
        connect(_editor_registry, &EditorRegistry::timeChanged,
                this, &Media_Widget::LoadFrame);

        // Connect to SelectionContext to respond to external selection changes
        // When another widget (e.g., DataManager_Widget) selects data, update our state
        if (_selection_context) {
            connect(_selection_context, &SelectionContext::selectionChanged,
                    this, &Media_Widget::_onExternalSelectionChanged);
        }
    }
}

Media_Widget::~Media_Widget() {
    // Unregister state from EditorRegistry when widget is destroyed
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
        _createOptions();
    });
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
        int width = ui->graphicsView->width();
        int height = ui->graphicsView->height();

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
    QString state_type;  // Type string for state synchronization

    if (type == DM_DataType::Line) {
        auto opts = _scene->getLineConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible() = enabled;
        state_type = QStringLiteral("line");
    } else if (type == DM_DataType::Mask) {
        auto opts = _scene->getMaskConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible() = enabled;
        state_type = QStringLiteral("mask");
    } else if (type == DM_DataType::Points) {
        auto opts = _scene->getPointConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible() = enabled;
        state_type = QStringLiteral("point");
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
            auto current_time = _data_manager->getCurrentTime();
            LoadFrame(current_time);

        } else {
            std::cout << "Disabling media data from scene" << std::endl;
            modified.is_visible() = false;
            _state->displayOptions().set(key, modified);
        }
        state_type = QStringLiteral("media");
    } else {
        std::cout << "Feature type " << convert_data_type_to_string(type) << " not supported" << std::endl;
    }
    
    // Sync feature enabled state to MediaWidgetState
    if (!state_type.isEmpty()) {
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

void Media_Widget::LoadFrame(int frame_id) {
    if (_scene) {
        _scene->LoadFrame(frame_id);
    }
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
    double current_zoom = _state->zoom();
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
}

bool Media_Widget::eventFilter(QObject * watched, QEvent * event) {
    if (watched == ui->graphicsView->viewport()) {
        // Handle wheel events for zoom
        if (event->type() == QEvent::Wheel) {
            auto * wheelEvent = dynamic_cast<QWheelEvent *>(event);
            double angle = wheelEvent->angleDelta().y();
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
                return true; // Consume the event to prevent it from reaching Media_Window
            }
        }
        
        if (event->type() == QEvent::MouseMove && _is_panning) {
            auto * mouseEvent = dynamic_cast<QMouseEvent *>(event);
            QPoint delta = mouseEvent->pos() - _last_pan_point;
            _last_pan_point = mouseEvent->pos();
            
            // Apply panning by translating the view
            ui->graphicsView->horizontalScrollBar()->setValue(
                ui->graphicsView->horizontalScrollBar()->value() - delta.x());
            ui->graphicsView->verticalScrollBar()->setValue(
                ui->graphicsView->verticalScrollBar()->value() - delta.y());
            
            mouseEvent->accept();
            return true; // Consume the event
        }
        
        if (event->type() == QEvent::MouseButtonRelease) {
            auto * mouseEvent = dynamic_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && _is_panning) {
                _is_panning = false;
                ui->graphicsView->viewport()->setCursor(Qt::ArrowCursor);
                
                // Sync final pan position to state directly
                if (_state && ui->graphicsView) {
                    double pan_x = ui->graphicsView->horizontalScrollBar()->value();
                    double pan_y = ui->graphicsView->verticalScrollBar()->value();
                    _state->setPan(pan_x, pan_y);
                }
                
                mouseEvent->accept();
                return true; // Consume the event
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

    QString selected_key = _selection_context->primarySelectedData().toString();
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
}

void Media_Widget::_onStateZoomChanged(double zoom) {
    // State is the source of truth - update the QGraphicsView to match
    // This handles both internal changes and external changes (e.g., workspace restore)
    if (!ui->graphicsView) return;
    
    // Get current transform scale
    QTransform const transform = ui->graphicsView->transform();
    double current_scale = transform.m11();  // Assumes uniform scaling
    
    // Only apply if different from current transform (avoid feedback loop)
    if (std::abs(current_scale - zoom) > 1e-6) {
        // Calculate scale factor to reach target zoom
        double factor = zoom / current_scale;
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        ui->graphicsView->scale(factor, factor);
    }
}

void Media_Widget::_onStatePanChanged(double x, double y) {
    if (!ui->graphicsView) return;
    
    // Only apply if different from current pan
    int current_x = ui->graphicsView->horizontalScrollBar()->value();
    int current_y = ui->graphicsView->verticalScrollBar()->value();
    
    if (std::abs(current_x - x) > 0.5 || std::abs(current_y - y) > 0.5) {
        ui->graphicsView->horizontalScrollBar()->setValue(static_cast<int>(x));
        ui->graphicsView->verticalScrollBar()->setValue(static_cast<int>(y));
    }
}

void Media_Widget::restoreFromState() {
    if (!_state) return;
    
    // Restore zoom - state is the source of truth
    double saved_zoom = _state->zoom();
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
    
    // Restore display options in Media_Window
    if (_scene) {
        _scene->restoreOptionsFromState();
    }
    
    // Restore enabled features by iterating through the state's options
    // and setting is_visible on the corresponding Media_Window configs
    auto const & data = _state->data();
    
    // Restore line features
    for (auto const & [key, opts] : data.line_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Restore mask features
    for (auto const & [key, opts] : data.mask_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Restore point features
    for (auto const & [key, opts] : data.point_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Restore tensor features
    for (auto const & [key, opts] : data.tensor_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Restore interval features
    for (auto const & [key, opts] : data.interval_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Restore media features
    for (auto const & [key, opts] : data.media_options) {
        if (opts.is_visible()) {
            _addFeatureToDisplay(QString::fromStdString(key), true);
        }
    }
    
    // Update canvas to reflect restored state
    if (_scene) {
        _scene->UpdateCanvas();
    }
}