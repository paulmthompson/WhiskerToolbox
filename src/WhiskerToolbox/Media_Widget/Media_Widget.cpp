#include "Media_Widget.hpp"
#include "ui_Media_Widget.h"

#include "Collapsible_Widget/Section.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "Media_Widget/MediaInterval_Widget/MediaInterval_Widget.hpp"
#include "Media_Widget/MediaLine_Widget/MediaLine_Widget.hpp"
#include "Media_Widget/MediaMask_Widget/MediaMask_Widget.hpp"
#include "Media_Widget/MediaPoint_Widget/MediaPoint_Widget.hpp"
#include "Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.hpp"
#include "Media_Widget/MediaTensor_Widget/MediaTensor_Widget.hpp"
#include "Media_Widget/MediaText_Widget/MediaText_Widget.hpp"
#include "Media_Window/Media_Window.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include <QApplication>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

Media_Widget::Media_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Media_Widget) {
    ui->setupUi(this);

    // Configure splitter behavior
    ui->splitter->setStretchFactor(0, 0);// Left panel (scroll area) doesn't stretch
    ui->splitter->setStretchFactor(1, 1);// Right panel (graphics view) stretches

    // Set initial sizes: 250px for left panel, rest for canvas (reduced from 350px)
    ui->splitter->setSizes({250, 513});

    // Set collapsible behavior
    ui->splitter->setCollapsible(0, false);// Prevent left panel from collapsing
    ui->splitter->setCollapsible(1, false);// Prevent canvas from collapsing

    // Connect splitter moved signal to update canvas size
    connect(ui->splitter, &QSplitter::splitterMoved, this, &Media_Widget::_updateCanvasSize);

    // Install event filter on graphics view viewport for wheel zoom
    if (ui->graphicsView && ui->graphicsView->viewport()) {
        ui->graphicsView->viewport()->installEventFilter(this);
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        ui->graphicsView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    }

    // Create text overlay section
    _text_section = new Section(this, "Text Overlays");
    _text_widget = new MediaText_Widget(this);
    _text_section->setContentLayout(*new QVBoxLayout());
    _text_section->layout()->addWidget(_text_widget);
    _text_section->autoSetContentLayout();

    // Add text section to the vertical layout in scroll area
    // Insert before the feature table widget (at index 0, after the Zoom button)
    ui->verticalLayout->insertWidget(1, _text_section);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &Media_Widget::_featureSelected);

    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](QString const & feature) {
        Media_Widget::_addFeatureToDisplay(feature, true);
    });

    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](QString const & feature) {
        Media_Widget::_addFeatureToDisplay(feature, false);
    });

    // Ensure the feature table and other widgets are properly sized on startup
    QTimer::singleShot(0, this, [this]() {
        int scrollAreaWidth = ui->scrollArea->width();
        ui->feature_table_widget->setFixedWidth(scrollAreaWidth - 10);

        // Call the update function to size everything properly
        _updateCanvasSize();

        // Initialize the stacked widget with proper sizing
        ui->stackedWidget->setFixedWidth(scrollAreaWidth - 10);
    });
}

Media_Widget::~Media_Widget() {
    // Proactively hide stacked pages while _scene is still alive so any hideEvent
    // handlers that interact with _scene do so safely before _scene is destroyed.
    if (ui && ui->stackedWidget) {
        for (int i = 0; i < ui->stackedWidget->count(); ++i) {
            QWidget * w = ui->stackedWidget->widget(i);
            if (w && w->isVisible()) {
                w->hide();
            }
        }
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

    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({DM_DataType::Line, DM_DataType::Mask, DM_DataType::Points, DM_DataType::DigitalInterval, DM_DataType::Tensor, DM_DataType::Video, DM_DataType::Images});
    ui->feature_table_widget->setDataManager(_data_manager);
    ui->feature_table_widget->populateTable();

    ui->stackedWidget->addWidget(new MediaPoint_Widget(_data_manager, _scene.get()));
    ui->stackedWidget->addWidget(new MediaLine_Widget(_data_manager, _scene.get()));
    ui->stackedWidget->addWidget(new MediaMask_Widget(_data_manager, _scene.get()));
    ui->stackedWidget->addWidget(new MediaInterval_Widget(_data_manager, _scene.get()));
    ui->stackedWidget->addWidget(new MediaTensor_Widget(_data_manager, _scene.get()));

    // Create and store reference to MediaProcessing_Widget
    _processing_widget = new MediaProcessing_Widget(_data_manager, _scene.get());
    ui->stackedWidget->addWidget(_processing_widget);

    // Connect text widget to scene if both are available
    _connectTextWidgetToScene();

    // Ensure all widgets in the stacked widget are properly sized
    QTimer::singleShot(100, this, [this]() {
        int scrollAreaWidth = ui->scrollArea->width();
        for (int i = 0; i < ui->stackedWidget->count(); ++i) {
            QWidget * widget = ui->stackedWidget->widget(i);
            if (widget) {
                widget->setFixedWidth(scrollAreaWidth - 10);
                widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                // If this is a MediaProcessing_Widget, make sure it fills its container
                auto processingWidget = qobject_cast<MediaProcessing_Widget *>(widget);
                if (processingWidget) {
                    processingWidget->setMinimumWidth(scrollAreaWidth - 10);
                    processingWidget->adjustSize();
                }
            }
        }
        _updateCanvasSize();
    });

    _data_manager->addObserver([this]() {
        _createOptions();
    });
}

void Media_Widget::_createOptions() {

    //Setup Media Data
    auto media_keys = _data_manager->getKeys<MediaData>();
    for (auto const & media_key: media_keys) {
        auto opts = _scene->getMediaConfig(media_key);
        if (opts.has_value()) continue;

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

void Media_Widget::_connectTextWidgetToScene() {
    if (_scene && _text_widget) {
        _scene->setTextWidget(_text_widget);

        // Connect text widget signals to update canvas when overlays change
        connect(_text_widget, &MediaText_Widget::textOverlayAdded, _scene.get(), &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayRemoved, _scene.get(), &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayUpdated, _scene.get(), &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlaysCleared, _scene.get(), &Media_Window::UpdateCanvas);
    }
}

void Media_Widget::_featureSelected(QString const & feature) {
    auto const type = _data_manager->getType(feature.toStdString());
    auto key = feature.toStdString();

    if (type == DM_DataType::Points) {

        int const stacked_widget_index = 1;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto point_widget = dynamic_cast<MediaPoint_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        point_widget->setActiveKey(key);

    } else if (type == DM_DataType::Line) {

        int const stacked_widget_index = 2;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto line_widget = dynamic_cast<MediaLine_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        line_widget->setActiveKey(key);

    } else if (type == DM_DataType::Mask) {

        int const stacked_widget_index = 3;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto mask_widget = dynamic_cast<MediaMask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        mask_widget->setActiveKey(key);


    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 4;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<MediaInterval_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        interval_widget->setActiveKey(key);

    } else if (type == DM_DataType::Tensor) {
        int const stacked_widget_index = 5;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto tensor_widget = dynamic_cast<MediaTensor_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        tensor_widget->setActiveKey(key);
    } else if (type == DM_DataType::Video) {
        int const stacked_widget_index = 6;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto processing_widget = dynamic_cast<MediaProcessing_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        processing_widget->setActiveKey(key);

        // Do NOT set as active media key or update canvas - only show controls for configuration
        // The media will only be displayed when it's enabled via the checkbox

    } else if (type == DM_DataType::Images) {
        int const stacked_widget_index = 6;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto processing_widget = dynamic_cast<MediaProcessing_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        processing_widget->setActiveKey(key);

        // Do NOT set as active media key or update canvas - only show controls for configuration
        // The media will only be displayed when it's enabled via the checkbox
    } else {
        ui->stackedWidget->setCurrentIndex(0);
        std::cout << "Unsupported feature type" << std::endl;
    }
}

void Media_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    // When user has zoomed, avoid rescaling scene contents destructively; just adjust scene rect
    if (_user_zoom_active) {
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
        if (!_user_zoom_active) {
            ui->graphicsView->resetTransform();
            _current_zoom = 1.0;
        }
        // Fit disabled: we manage zoom manually now
        // Update left panel sizing
        int scrollAreaWidth = ui->scrollArea->width();
        int featureTableWidth = scrollAreaWidth - 10;
        ui->feature_table_widget->setFixedWidth(featureTableWidth);
        ui->stackedWidget->setFixedWidth(featureTableWidth);
        for (int i = 0; i < ui->stackedWidget->count(); ++i) {
            QWidget * widget = ui->stackedWidget->widget(i);
            if (widget) {
                widget->setFixedWidth(featureTableWidth);
            }
        }
    }
}

void Media_Widget::_addFeatureToDisplay(QString const & feature, bool enabled) {
    std::cout << "Feature: " << feature.toStdString() << std::endl;

    auto const feature_key = feature.toStdString();
    auto const type = _data_manager->getType(feature_key);

    if (type == DM_DataType::Line) {
        auto opts = _scene->getLineConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible = enabled;
    } else if (type == DM_DataType::Mask) {
        auto opts = _scene->getMaskConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible = enabled;
    } else if (type == DM_DataType::Points) {
        auto opts = _scene->getPointConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible = enabled;
    } else if (type == DM_DataType::DigitalInterval) {
        auto opts = _scene->getIntervalConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        opts.value()->is_visible = enabled;
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
            opts.value()->is_visible = true;
        } else {
            std::cout << "Disabling tensor data from scene" << std::endl;
            opts.value()->is_visible = false;
        }
    } else if (type == DM_DataType::Video || type == DM_DataType::Images) {
        auto opts = _scene->getMediaConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        if (enabled) {
            std::cout << "Enabling media data in scene" << std::endl;
            opts.value()->is_visible = true;

            // This ensures new media is loaded from disk
            // Before the update
            auto current_time = _data_manager->getCurrentTime();
            LoadFrame(current_time);

        } else {
            std::cout << "Disabling media data from scene" << std::endl;
            opts.value()->is_visible = false;
        }
    } else {
        std::cout << "Feature type " << convert_data_type_to_string(type) << " not supported" << std::endl;
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

void Media_Widget::setFeatureColor(std::string const & feature, std::string const & hex_color) {
    auto const type = _data_manager->getType(feature);

    if (type == DM_DataType::Line) {
        auto opts = _scene->getLineConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color = hex_color;
        }
    } else if (type == DM_DataType::Mask) {
        auto opts = _scene->getMaskConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color = hex_color;
        }
    } else if (type == DM_DataType::Points) {
        auto opts = _scene->getPointConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color = hex_color;
        }
    } else if (type == DM_DataType::DigitalInterval) {
        auto opts = _scene->getIntervalConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color = hex_color;
        }
    } else if (type == DM_DataType::Tensor) {
        auto opts = _scene->getTensorConfig(feature);
        if (opts.has_value()) {
            opts.value()->hex_color = hex_color;
        }
    }

    _scene->UpdateCanvas();
}

void Media_Widget::LoadFrame(int frame_id) {
    if (_scene) {
        _scene->LoadFrame(frame_id);
    }
    int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex > 0) {
        auto currentWidget = ui->stackedWidget->currentWidget();
        auto lineWidget = dynamic_cast<MediaLine_Widget *>(currentWidget);
        if (lineWidget) {
            lineWidget->LoadFrame(frame_id);
        }
    }
}

// Zoom API implementations
void Media_Widget::zoomIn() { _applyZoom(_zoom_step, false); }
void Media_Widget::zoomOut() { _applyZoom(1.0 / _zoom_step, false); }
void Media_Widget::resetZoom() {
    if (!ui->graphicsView) return;
    ui->graphicsView->resetTransform();
    _current_zoom = 1.0;
    _user_zoom_active = false;
}

void Media_Widget::_applyZoom(double factor, bool anchor_under_mouse) {
    if (!ui->graphicsView) return;
    double new_zoom = _current_zoom * factor;
    new_zoom = std::clamp(new_zoom, _min_zoom, _max_zoom);
    factor = new_zoom / _current_zoom;// Adjust factor if clamped
    if (qFuzzyCompare(factor, 1.0)) return;
    if (anchor_under_mouse) {
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    } else {
        ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    }
    ui->graphicsView->scale(factor, factor);
    _current_zoom = new_zoom;
    _user_zoom_active = (_current_zoom != 1.0);
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

        _connectTextWidgetToScene();
    }
}