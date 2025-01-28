#include "DigitalIntervalSeries_Widget.hpp"
#include "ui_DigitalIntervalSeries_Widget.h"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <QPushButton>

#include <filesystem>
#include <iostream>

DigitalIntervalSeries_Widget::DigitalIntervalSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DigitalIntervalSeries_Widget),
    _data_manager{data_manager}
{
    ui->setupUi(this);

    connect(ui->save_csv, &QPushButton::clicked, this, &DigitalIntervalSeries_Widget::_saveCSV);
}

DigitalIntervalSeries_Widget::~DigitalIntervalSeries_Widget() {
    delete ui;
}

void DigitalIntervalSeries_Widget::openWidget()
{
    // Populate the widget with data if needed
    this->show();

}

void DigitalIntervalSeries_Widget::_saveCSV()
{
    auto output_path = _data_manager->getOutputPath();
    std::cout << output_path.string() << std::endl;
    std::cout << _active_key << std::endl;

    auto filename = ui->filename_textbox->toPlainText().toStdString();
    output_path.append(filename);

    auto contactEvents = _data_manager->getData<DigitalIntervalSeries>(_active_key)->getDigitalIntervalSeries();

    save_intervals(contactEvents, output_path.string());
}

void DigitalIntervalSeries_Widget::setActiveKey(std::string key)
{
    _active_key = key;

    _data_manager->removeCallbackFromData(key, _callback_id);

    _callback_id = _data_manager->addCallbackToData(key, [this]() {
        _calculateIntervals();
    });
    _calculateIntervals();
}

void DigitalIntervalSeries_Widget::_calculateIntervals()
{
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key);
    ui->total_interval_label->setText(QString::number(intervals->size()));

    _buildIntervalTable();
}

void DigitalIntervalSeries_Widget::_buildIntervalTable()
{
    auto intervals = _data_manager->getData<DigitalIntervalSeries>(_active_key)->getDigitalIntervalSeries();

    ui->interval_table->setRowCount(0);
    for (int i=0; i < intervals.size(); i++)
    {
        ui->interval_table->insertRow(ui->interval_table->rowCount());
        ui->interval_table->setItem(i,0,new QTableWidgetItem(QString::number(std::round(intervals[i].start))));
        ui->interval_table->setItem(i,1,new QTableWidgetItem(QString::number(std::round(intervals[i].end))));

    }

}
