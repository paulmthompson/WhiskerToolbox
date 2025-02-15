
#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "Mask_Widget/Mask_Widget.hpp"
#include "Point_Widget/Point_Widget.hpp"
#include "Line_Widget/Line_Widget.hpp"
#include "AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp"
#include "DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp"
#include "DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp"
#include "Tensor_Widget/Tensor_Widget.hpp"

#include "Media_Window/Media_Window.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>

DataManager_Widget::DataManager_Widget(
    Media_Window* scene,
    std::shared_ptr<DataManager> data_manager,
    TimeScrollBar* time_scrollbar,
    QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataManager_Widget),
    _scene{scene},
    _time_scrollbar{time_scrollbar},
    _data_manager{data_manager}
{
    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    ui->output_dir_label->setText(QString::fromStdString(std::filesystem::current_path().string()));

    ui->stackedWidget->addWidget(new Point_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Mask_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Line_Widget(_data_manager));
    ui->stackedWidget->addWidget(new AnalogTimeSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalIntervalSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalEventSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Tensor_Widget(_data_manager));

    connect(ui->output_dir_button, &QPushButton::clicked, this, &DataManager_Widget::_changeOutputDir);
    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataManager_Widget::_handleFeatureSelected);
    connect(ui->new_data_button, &QPushButton::clicked, this, &DataManager_Widget::_createNewData);
}

DataManager_Widget::~DataManager_Widget() {
    delete ui;
}

void DataManager_Widget::openWidget()
{
    ui->feature_table_widget->populateTable();
    this->show();
}

void DataManager_Widget::_handleFeatureSelected(const QString& feature)
{
    _highlighted_available_feature = feature;

    auto key = feature.toStdString();

    auto feature_type = _data_manager->getType(feature.toStdString());

    if (feature_type == "PointData") {

        const int stacked_widget_index = 1;

        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto point_widget = dynamic_cast<Point_Widget*>(ui->stackedWidget->widget(stacked_widget_index));
        point_widget->setActiveKey(key);
        connect(_scene, &Media_Window::leftClickMedia, point_widget, &Point_Widget::assignPoint);

        _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [this]() {
            _scene->UpdateCanvas();
        }));

        _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [point_widget]() {
            point_widget->updateTable();
        }));

        connect(_time_scrollbar, &TimeScrollBar::timeChanged, point_widget, &Point_Widget::loadFrame);

    } else if (feature_type == "MaskData") {

        const int stacked_widget_index = 2;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto mask_widget = dynamic_cast<Mask_Widget*>(ui->stackedWidget->widget(stacked_widget_index));
        mask_widget->setActiveKey(key);

        connect(_scene, &Media_Window::leftClickMedia, mask_widget, &Mask_Widget::selectPoint);

    } else if (feature_type == "LineData") {
        ui->stackedWidget->setCurrentIndex(3);
    } else if (feature_type == "AnalogTimeSeries") {

        const int stacked_widget_index = 4;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto analog_widget = dynamic_cast<AnalogTimeSeries_Widget*>(ui->stackedWidget->widget(stacked_widget_index));
        analog_widget->setActiveKey(key);

    } else if (feature_type == "DigitalIntervalSeries") {

        ui->stackedWidget->setCurrentIndex(5);
        auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget*>(ui->stackedWidget->widget(5));
        interval_widget->setActiveKey(key);

        connect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);

    } else if (feature_type == "DigitalEventSeries") {
        ui->stackedWidget->setCurrentIndex(6);
    } else if (feature_type == "TensorData") {

        ui->stackedWidget->setCurrentIndex(7);
        dynamic_cast<Tensor_Widget*>(ui->stackedWidget->widget(7))->setActiveKey(key);


    } else {
        std::cout << "Unsupported feature type" << std::endl;
    }
}

void DataManager_Widget::_disablePreviousFeature(const QString& feature)
{

    auto key = feature.toStdString();

    for (auto callback : _current_data_callbacks) {
        _data_manager->removeCallbackFromData(key, callback);
    }

    _current_data_callbacks.clear();

    auto feature_type = _data_manager->getType(feature.toStdString());

    if (feature_type == "PointData") {

        const int stacked_widget_index = 1;

        auto point_widget = dynamic_cast<Point_Widget*>(ui->stackedWidget->widget(stacked_widget_index));
        disconnect(_scene, &Media_Window::leftClickMedia, point_widget, &Point_Widget::assignPoint);
        disconnect(_time_scrollbar, &TimeScrollBar::timeChanged, point_widget, &Point_Widget::loadFrame);

    } else if (feature_type == "MaskData") {

        const int stacked_widget_index = 2;
        auto mask_widget = dynamic_cast<Mask_Widget*>(ui->stackedWidget->widget(stacked_widget_index));
        disconnect(_scene, &Media_Window::leftClickMedia, mask_widget, &Mask_Widget::selectPoint);


    } else if (feature_type == "LineData") {

    } else if (feature_type == "AnalogTimeSeries") {

    } else if (feature_type == "DigitalIntervalSeries") {

        auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget*>(ui->stackedWidget->widget(5));

        disconnect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
        interval_widget->removeCallbacks();
        
    } else if (feature_type == "DigitalEventSeries") {

    } else if (feature_type == "TensorData") {
    } else {
        std::cout << "Unsupported feature type" << std::endl;
    }
}


void DataManager_Widget::_changeOutputDir()
{
    QString dir_name = QFileDialog::getExistingDirectory(
        this,
        "Select Directory",
        QDir::currentPath());

    if (dir_name.isEmpty()) {
        return;
    }

    _data_manager->setOutputPath(dir_name.toStdString());
    ui->output_dir_label->setText(dir_name);

}

void DataManager_Widget::_createNewData()
{
    auto key = ui->new_data_name->toPlainText().toStdString();

    if (key.empty()) {
        return;
    }

    auto type = ui->new_data_type_combo->currentText().toStdString();

    if (type == "Point") {
        _data_manager->setData<PointData>(key);
    } else if (type == "Mask") {
        _data_manager->setData<MaskData>(key);
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

void DataManager_Widget::_changeScrollbar(int frame_id)
{
    _time_scrollbar->changeScrollBarValue(frame_id);
}

