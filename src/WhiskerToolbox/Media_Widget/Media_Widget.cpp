#include "Media_Widget.hpp"

#include "ui_Media_Widget.h"

#include "DataManager/ImageSize/ImageSize.hpp"
#include "Main_Window/mainwindow.hpp"
#include "Media_Widget/MediaLine_Widget/MediaLine_Widget.hpp"
#include "Media_Widget/MediaMask_Widget/MediaMask_Widget.hpp"
#include "Media_Widget/MediaPoint_Widget/MediaPoint_Widget.hpp"
#include "Media_Widget/MediaTensor_Widget/MediaTensor_Widget.hpp"
#include "Media_Window/Media_Window.hpp"


Media_Widget::Media_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Media_Widget) {
    ui->setupUi(this);

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
}

void Media_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    ui->feature_table_widget->setColumns({"Feature", "Color", "Enabled", "Type"});
    ui->feature_table_widget->setTypeFilter({DM_DataType::Line, DM_DataType::Mask, DM_DataType::Points, DM_DataType::DigitalInterval, DM_DataType::Tensor});
    ui->feature_table_widget->setDataManager(_data_manager);
    ui->feature_table_widget->populateTable();

    ui->stackedWidget->addWidget(new MediaPoint_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaLine_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaMask_Widget(_data_manager, _scene));
    ui->stackedWidget->addWidget(new MediaTensor_Widget(_data_manager, _scene));
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


    } else if (type == DM_DataType::Tensor) {

        int const stacked_widget_index = 4;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto tensor_widget = dynamic_cast<MediaTensor_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        tensor_widget->setActiveKey(key);
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
        _scene->setCanvasSize(
                ImageSize{ui->graphicsView->width(),
                          ui->graphicsView->height()});
        _scene->UpdateCanvas();
    }
}

void Media_Widget::_addFeatureToDisplay(QString const & feature, bool enabled) {
    std::cout << "Feature: " << feature.toStdString() << std::endl;

    auto const feature_key = feature.toStdString();
    auto const type = _data_manager->getType(feature_key);

    std::string color = ui->feature_table_widget->getFeatureColor(feature_key);

    std::cout << "Color: " << color << std::endl;

    if (type == DM_DataType::Line) {
        if (enabled) {
            std::cout << "Adding line data to scene" << std::endl;
            _scene->addLineDataToScene(feature_key);
            auto line_opts = _scene->getLineConfig(feature_key);
            if (line_opts.has_value()) {
                line_opts.value()->hex_color = color;
            }
        } else {
            std::cout << "Removing line data from scene" << std::endl;
            _scene->removeLineDataFromScene(feature_key);
        }
    } else if (type == DM_DataType::Mask) {
        if (enabled) {
            std::cout << "Adding mask data to scene" << std::endl;
            _scene->addMaskDataToScene(feature_key);
            auto mask_opts = _scene->getMaskConfig(feature_key);
            if (mask_opts.has_value()) {
                mask_opts.value()->hex_color = color;
            }
        } else {
            std::cout << "Removing mask data from scene" << std::endl;
            _scene->removeMaskDataFromScene(feature_key);
        }
    } else if (type == DM_DataType::Points) {
        if (enabled) {
            std::cout << "Adding point data to scene" << std::endl;
            _scene->addPointDataToScene(feature_key);
            auto point_opts = _scene->getPointConfig(feature_key);
            if (point_opts.has_value()) {
                point_opts.value()->hex_color = color;
            }
        } else {
            std::cout << "Removing point data from scene" << std::endl;
            _scene->removePointDataFromScene(feature_key);
        }
    } else if (type == DM_DataType::DigitalInterval) {
        if (enabled) {
            std::cout << "Adding digital interval series to scene" << std::endl;
            _scene->addDigitalIntervalSeries(feature_key);
            auto interval_opts = _scene->getIntervalConfig(feature_key);
            if (interval_opts.has_value()) {
                interval_opts.value()->hex_color = color;
            }
        } else {
            std::cout << "Removing digital interval series from scene" << std::endl;
            _scene->removeDigitalIntervalSeries(feature_key);
        }
    } else if (type == DM_DataType::Tensor) {
        if (enabled) {
            std::cout << "Adding Tensor data to scene" << std::endl;
            _scene->addTensorDataToScene(feature_key);
        } else {
            std::cout << "Removing tensor data from scene" << std::endl;
            _scene->removeTensorDataFromScene(feature_key);
        }
    } else {
        std::cout << "Feature type " << convert_data_type_to_string(type) << " not supported" << std::endl;
    }
}

void Media_Widget::setFeatureColor(std::string const & feature, std::string const & hex_color) {
    ui->feature_table_widget->setFeatureColor(feature, hex_color);
}
