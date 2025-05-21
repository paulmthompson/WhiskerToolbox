#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include "DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "DataManager/ImageSize/ImageSize.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp"
#include "DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp"
#include "DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp"
#include "Line_Widget/Line_Widget.hpp"
#include "Mask_Widget/Mask_Widget.hpp"
#include "NewDataWidget/NewDataWidget.hpp"
#include "OutputDirectoryWidget/OutputDirectoryWidget.hpp"
#include "Point_Widget/Point_Widget.hpp"
#include "Tensor_Widget/Tensor_Widget.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>

DataManager_Widget::DataManager_Widget(
        Media_Window * scene,
        std::shared_ptr<DataManager> data_manager,
        TimeScrollBar * time_scrollbar,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataManager_Widget),
      _scene{scene},
      _time_scrollbar{time_scrollbar},
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    ui->stackedWidget->addWidget(new Point_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Mask_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Line_Widget(_data_manager));
    ui->stackedWidget->addWidget(new AnalogTimeSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalIntervalSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalEventSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Tensor_Widget(_data_manager));

    ui->output_dir_section->autoSetContentLayout();
    ui->output_dir_section->setTitle("Output Directory");

    ui->new_data_section->autoSetContentLayout();
    ui->new_data_section->setTitle("Create New Data");

    connect(ui->output_dir_widget, &OutputDirectoryWidget::dirChanged, this, &DataManager_Widget::_changeOutputDir);
    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataManager_Widget::_handleFeatureSelected);
    connect(ui->new_data_widget, &NewDataWidget::createNewData, this, &DataManager_Widget::_createNewData);
}

DataManager_Widget::~DataManager_Widget() {
    delete ui;
}

void DataManager_Widget::openWidget() {
    ui->feature_table_widget->populateTable();
    this->show();
}

void DataManager_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;

    auto key = feature.toStdString();

    auto feature_type = _data_manager->getType(feature.toStdString());

    switch (feature_type) {
        case DM_DataType::Points: {

            int const stacked_widget_index = 1;

            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto point_widget = dynamic_cast<Point_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            point_widget->setActiveKey(key);

            _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [this]() {
                _scene->UpdateCanvas();
            }));

            _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [point_widget]() {
                point_widget->updateTable();
            }));

            connect(_time_scrollbar, &TimeScrollBar::timeChanged, point_widget, &Point_Widget::loadFrame);
            break;
        }
        case DM_DataType::Mask: {

            int const stacked_widget_index = 2;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto mask_widget = dynamic_cast<Mask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            mask_widget->setActiveKey(key);

            _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [this]() {
                _scene->UpdateCanvas();
            }));
            break;

        }
        case DM_DataType::Line: {
            int const stacked_widget_index = 3;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto line_widget = dynamic_cast<Line_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (line_widget) {
                line_widget->setActiveKey(key);
                connect(line_widget, &Line_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
                _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [this]() {
                    _scene->UpdateCanvas();
                }));
            }
            break;
        }
        case DM_DataType::Analog: {

            int const stacked_widget_index = 4;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto analog_widget = dynamic_cast<AnalogTimeSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            analog_widget->setActiveKey(key);
            break;

        }
        case DM_DataType::DigitalInterval: {

            int const stacked_widget_index = 5;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            interval_widget->setActiveKey(key);

            connect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);

            break;
        }
        case DM_DataType::DigitalEvent: {


            int const stacked_widget_index = 6;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto event_widget = dynamic_cast<DigitalEventSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            event_widget->setActiveKey(key);

            connect(event_widget, &DigitalEventSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            break;

        }
        case DM_DataType::Tensor: {

            ui->stackedWidget->setCurrentIndex(7);
            dynamic_cast<Tensor_Widget *>(ui->stackedWidget->widget(7))->setActiveKey(key);
            break;
        }
        case DM_DataType::Unknown: {
            std::cout << "Unsupported feature type" << std::endl;
            break;
        }
        default:
        {
            std::cout << "You shouldn't be here" << std::endl;
        }
    }
}

void DataManager_Widget::_disablePreviousFeature(QString const & feature) {

    auto key = feature.toStdString();

    for (auto callback: _current_data_callbacks) {
        _data_manager->removeCallbackFromData(key, callback);
    }

    _current_data_callbacks.clear();

    auto feature_type = _data_manager->getType(feature.toStdString());

    switch(feature_type) {
        case DM_DataType::Points: {

            int const stacked_widget_index = 1;

            auto point_widget = dynamic_cast<Point_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(_time_scrollbar, &TimeScrollBar::timeChanged, point_widget, &Point_Widget::loadFrame);
            break;

        }
        case DM_DataType::Mask: {

            int const stacked_widget_index = 2;
            auto mask_widget = dynamic_cast<Mask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(_scene, &Media_Window::leftClickMedia, mask_widget, &Mask_Widget::selectPoint);
            break;
        }
        case DM_DataType::Line: {
            int const stacked_widget_index = 3;
            auto line_widget = dynamic_cast<Line_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (line_widget) {
                disconnect(line_widget, &Line_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
                line_widget->removeCallbacks();
            }
            break;
        }
        case DM_DataType::Analog: {
            int const stacked_widget_index = 4;
            break;
        }
        case DM_DataType::DigitalInterval: {

            int const stacked_widget_index = 5;
            auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            interval_widget->removeCallbacks();
            break;
        }
        case DM_DataType::DigitalEvent: {

            int const stacked_widget_index = 6;
            auto event_widget = dynamic_cast<DigitalEventSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(event_widget, &DigitalEventSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            event_widget->removeCallbacks();
            break;

        } case DM_DataType::Tensor: {
            int const stacked_widget_index = 7;
            break;
        } case DM_DataType::Unknown: {
            std::cout << "Unsupported feature type" << std::endl;
            break;
        }
    }
}

void DataManager_Widget::_changeOutputDir(QString dir_name) {


    if (dir_name.isEmpty()) {
        return;
    }

    _data_manager->setOutputPath(dir_name.toStdString());
    ui->output_dir_widget->setDirLabel(dir_name);
}

void DataManager_Widget::_createNewData(std::string key, std::string type) {

    if (key.empty()) {
        return;
    }

    if (type == "Point") {
        _data_manager->setData<PointData>(key);
    } else if (type == "Mask") {

        auto const image_size = _data_manager->getData<MediaData>("media")->getImageSize();

        _data_manager->setData<MaskData>(key);
        _data_manager->getData<MaskData>(key)->setImageSize(image_size);

    } else if (type == "Line") {
        _data_manager->setData<LineData>(key);
    } else if (type == "Analog Time Series") {
        _data_manager->setData<AnalogTimeSeries>(key);
    } else if (type == "Interval") {
        _data_manager->setData<DigitalIntervalSeries>(key);
    } else if (type == "Event") {
        _data_manager->setData<DigitalEventSeries>(key);
    } else if (type == "Tensor") {
        _data_manager->setData<TensorData>(key);
    } else {
        std::cout << "Unsupported data type" << std::endl;
    }
}

void DataManager_Widget::_changeScrollbar(int frame_id) {
    _time_scrollbar->changeScrollBarValue(frame_id);
}
