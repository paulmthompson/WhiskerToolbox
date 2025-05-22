#include "AnalogTimeSeries_Widget.hpp"
#include "ui_AnalogTimeSeries_Widget.h"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

AnalogTimeSeries_Widget::AnalogTimeSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogTimeSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->save_csv, &QPushButton::clicked, this, &AnalogTimeSeries_Widget::_saveCSV);
}

AnalogTimeSeries_Widget::~AnalogTimeSeries_Widget() {
    delete ui;
}

void AnalogTimeSeries_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

void AnalogTimeSeries_Widget::_saveCSV() {
    auto output_path = _data_manager->getOutputPath();
    std::cout << output_path.string() << std::endl;
    std::cout << _active_key << std::endl;

    auto filename = ui->filename_textbox->text().toStdString();
    output_path.append(filename);

    auto analog_data = _data_manager->getData<AnalogTimeSeries>(_active_key)->getAnalogTimeSeries();
    auto time_stamps = _data_manager->getData<AnalogTimeSeries>(_active_key)->getTimeSeries();

    save_analog(analog_data, time_stamps, output_path.string());
}


void AnalogTimeSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);

    //_data_manager->removeCallbackFromData(key, _callback_id);

    //_assignCallbacks();

    //_calculateIntervals();
}
