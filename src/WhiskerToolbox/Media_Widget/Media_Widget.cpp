#include "Media_Widget.hpp"

#include "ui_Media_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/ImageSize/ImageSize.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"


//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS


#include "Collapsible_Widget/Section.hpp"
#include "Media_Widget/MediaInterval_Widget/MediaInterval_Widget.hpp"
#include "Media_Widget/MediaLine_Widget/MediaLine_Widget.hpp"
#include "Media_Widget/MediaMask_Widget/MediaMask_Widget.hpp"
#include "Media_Widget/MediaPoint_Widget/MediaPoint_Widget.hpp"
#include "Media_Widget/MediaProcessing_Widget/MediaProcessing_Widget.hpp"
#include "Media_Widget/MediaTensor_Widget/MediaTensor_Widget.hpp"
#include "Media_Widget/MediaText_Widget/MediaText_Widget/MediaText_Widget.hpp"
#include "Media_Window/Media_Window.hpp"


Media_Widget::Media_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Media_Widget) {
    ui->setupUi(this);

    // Configure splitter behavior
    ui->splitter->setStretchFactor(0, 0);// Left panel (scroll area) doesn't stretch
    ui->splitter->setStretchFactor(1, 1);// Right panel (graphics view) stretches

    // Set initial sizes: 200px for left panel, rest for canvas
    ui->splitter->setSizes({300, 563});

    // Set collapsible behavior
    ui->splitter->setCollapsible(0, false);// Prevent left panel from collapsing
    ui->splitter->setCollapsible(1, false);// Prevent canvas from collapsing

    // Connect splitter moved signal to update canvas size
    connect(ui->splitter, &QSplitter::splitterMoved, this, &Media_Widget::_updateCanvasSize);

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
}

Media_Widget::~Media_Widget() {
    delete ui;
}

void Media_Widget::updateMedia() {

    ui->graphicsView->setScene(_scene);
    ui->graphicsView->show();

    _updateCanvasSize();
}

void Media_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({DM_DataType::Line, DM_DataType::Mask, DM_DataType::Points, DM_DataType::DigitalInterval, DM_DataType::Tensor, DM_DataType::Video});
    ui->feature_table_widget->setDataManager(_data_manager);
    ui->feature_table_widget->populateTable();

    ui->stackedWidget->addWidget(new MediaPoint_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaLine_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaMask_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaInterval_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaTensor_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaProcessing_Widget(_data_manager, _scene));

    // Connect text widget to scene if both are available
    _connectTextWidgetToScene();

    _data_manager->addObserver([this]() {
        _createOptions();
    });
}

void Media_Widget::_createOptions() {
    // Setup line data
    auto line_keys = _data_manager->getKeys<LineData>();
    for (auto line_key: line_keys) {
        auto opts = _scene->getLineConfig(line_key);
        if (opts.has_value()) continue;

        _scene->addLineDataToScene(line_key);
    }

    // Setup mask data
    auto mask_keys = _data_manager->getKeys<MaskData>();
    for (auto mask_key: mask_keys) {
        auto opts = _scene->getMaskConfig(mask_key);
        if (opts.has_value()) continue;

        _scene->addMaskDataToScene(mask_key);
    }

    // Setup point data
    auto point_keys = _data_manager->getKeys<PointData>();
    for (auto point_key: point_keys) {
        auto opts = _scene->getPointConfig(point_key);
        if (opts.has_value()) continue;

        _scene->addPointDataToScene(point_key);
    }

    // Setup digital interval data
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    for (auto interval_key: interval_keys) {
        auto opts = _scene->getIntervalConfig(interval_key);
        if (opts.has_value()) continue;

        _scene->addDigitalIntervalSeries(interval_key);
    }

    // Setup tensor data
    auto tensor_keys = _data_manager->getKeys<TensorData>();
    for (auto tensor_key: tensor_keys) {
        auto opts = _scene->getTensorConfig(tensor_key);
        if (opts.has_value()) continue;

        _scene->addTensorDataToScene(tensor_key);
    }
}

void Media_Widget::_connectTextWidgetToScene() {
    if (_scene && _text_widget) {
        _scene->setTextWidget(_text_widget);

        // Connect text widget signals to update canvas when overlays change
        connect(_text_widget, &MediaText_Widget::textOverlayAdded, _scene, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayRemoved, _scene, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlayUpdated, _scene, &Media_Window::UpdateCanvas);
        connect(_text_widget, &MediaText_Widget::textOverlaysCleared, _scene, &Media_Window::UpdateCanvas);
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
    } else {
        ui->stackedWidget->setCurrentIndex(0);
        std::cout << "Unsupported feature type" << std::endl;
    }
}

void Media_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    _updateCanvasSize();
}

void Media_Widget::_updateCanvasSize() {
    if (_scene) {
        int width = ui->graphicsView->width();
        int height = ui->graphicsView->height();

        std::cout << "Updating canvas size to: " << width << "x" << height << std::endl;

        _scene->setCanvasSize(
                ImageSize{width, height});
        _scene->UpdateCanvas();

        // Ensure the view fits the scene properly
        ui->graphicsView->setSceneRect(0, 0, width, height);
        ui->graphicsView->fitInView(0, 0, width, height, Qt::IgnoreAspectRatio);
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
        if (enabled) {
            std::cout << "Enabling line data in scene" << std::endl;
            opts.value()->is_visible = true;
        } else {
            std::cout << "Disabling line data from scene" << std::endl;
            opts.value()->is_visible = false;
        }
    } else if (type == DM_DataType::Mask) {
        auto opts = _scene->getMaskConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        if (enabled) {
            std::cout << "Enabling mask data in scene" << std::endl;
            opts.value()->is_visible = true;
        } else {
            std::cout << "Disabling mask data from scene" << std::endl;
            opts.value()->is_visible = false;
        }
    } else if (type == DM_DataType::Points) {
        auto opts = _scene->getPointConfig(feature_key);
        if (!opts.has_value()) {
            std::cerr << "Table feature key "
                      << feature_key
                      << " not found in Media_Window Display Options"
                      << std::endl;
            return;
        }
        if (enabled) {
            std::cout << "Enabling point data in scene" << std::endl;
            opts.value()->is_visible = true;
        } else {
            std::cout << "Disabling point data from scene" << std::endl;
            opts.value()->is_visible = false;
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
        if (enabled) {
            std::cout << "Enabling digital interval series in scene" << std::endl;
            opts.value()->is_visible = true;
        } else {
            std::cout << "Disabling digital interval series from scene" << std::endl;
            opts.value()->is_visible = false;
        }
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

    // Update the canvas with the new color
    _scene->UpdateCanvas();
}

void Media_Widget::LoadFrame(int frame_id) {
    // First, update the Media_Window with the new frame
    if (_scene) {
        _scene->LoadFrame(frame_id);
    }

    // Then propagate the frame change to any active subwidgets
    // that need to respond to time changes

    // Check if we have a MediaLine_Widget active in the stackedWidget
    int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex > 0) {// Index 0 is typically the empty widget
        auto currentWidget = ui->stackedWidget->currentWidget();

        // Check for each type of widget and propagate the frame change
        auto lineWidget = dynamic_cast<MediaLine_Widget *>(currentWidget);
        if (lineWidget) {
            lineWidget->LoadFrame(frame_id);
        }

        // Add similar handling for other types of media subwidgets as needed
        // For example:
        // auto pointWidget = dynamic_cast<MediaPoint_Widget*>(currentWidget);
        // if (pointWidget) {
        //     pointWidget->LoadFrame(frame_id);
        // }
    }
}
