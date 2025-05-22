

#include "Digital_Interval_Loader_Widget.hpp"

#include "ui_Digital_Interval_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series_Loader.hpp"

#include <QFileDialog>

#include <iostream>

Digital_Interval_Loader_Widget::Digital_Interval_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Digital_Interval_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->load_from_csv, &QPushButton::clicked, this, &Digital_Interval_Loader_Widget::_loadSingleInterval);
}

Digital_Interval_Loader_Widget::~Digital_Interval_Loader_Widget() {
    delete ui;
}

void Digital_Interval_Loader_Widget::_loadSingleInterval() {
    auto interval_filename = QFileDialog::getOpenFileName(
            this,
            "Load Intervals",
            QDir::currentPath(),
            "All files (*.*)");

    if (interval_filename.isNull()) {
        return;
    }

    auto const interval_key = ui->data_name_text->text().toStdString();

    char delimiter;
    if (ui->delimiter_combo->currentText() == "Space") {
        delimiter = ' ';
    } else if (ui->delimiter_combo->currentText() == "Comma") {
        delimiter = ',';
    } else {
        std::cout << "Unsupported delimiter" << std::endl;
        return;
    }

    auto intervals = load_digital_series_from_csv(interval_filename.toStdString(), delimiter);

    std::cout << "Loaded " << intervals.size() << " intervals" << std::endl;

    _data_manager->setData<DigitalIntervalSeries>(interval_key);

    _data_manager->getData<DigitalIntervalSeries>(interval_key)->setData(intervals);
}
