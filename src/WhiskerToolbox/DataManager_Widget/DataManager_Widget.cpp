
#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include "DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

#include "Mask_Widget/Mask_Widget.hpp"
#include "Point_Widget/Point_Widget.hpp"
#include "Line_Widget/Line_Widget.hpp"
#include "AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp"
#include "DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp"
#include "DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp"



#include <QFileDialog>

DataManager_Widget::DataManager_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataManager_Widget),
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
        ui->stackedWidget->setCurrentIndex(1);
    } else if (feature_type == "MaskData") {
        ui->stackedWidget->setCurrentIndex(2);
    } else if (feature_type == "LineData") {
        ui->stackedWidget->setCurrentIndex(3);
    } else if (feature_type == "AnalogTimeSeries") {
        ui->stackedWidget->setCurrentIndex(4);
    } else if (feature_type == "DigitalIntervalSeries") {

        ui->stackedWidget->setCurrentIndex(5);
        dynamic_cast<DigitalIntervalSeries_Widget*>(ui->stackedWidget->widget(5))->setActiveKey(key);

    } else if (feature_type == "DigitalEventSeries") {
        ui->stackedWidget->setCurrentIndex(6);
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
    } else {
        std::cout << "Unsupported data type" << std::endl;
    }
}
