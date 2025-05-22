#include "Digital_Interval_Loader_Widget.hpp"

#include "ui_Digital_Interval_Loader_Widget.h"

#include "../../DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_Loader.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <QFileDialog>
#include <QComboBox>
#include <QStackedWidget>
#include <iostream>

Digital_Interval_Loader_Widget::Digital_Interval_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Digital_Interval_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Digital_Interval_Loader_Widget::_onLoaderTypeChanged);

    connect(ui->csv_digital_interval_loader_widget, &CSVDigitalIntervalLoader_Widget::loadFileRequested,
            this, &Digital_Interval_Loader_Widget::_handleCSVLoadRequested);

    _onLoaderTypeChanged(0);
}

Digital_Interval_Loader_Widget::~Digital_Interval_Loader_Widget() {
    delete ui;
}

void Digital_Interval_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_digital_interval_loader_widget);
    }
}

void Digital_Interval_Loader_Widget::_handleCSVLoadRequested(QString delimiterText) {
    auto interval_filename = QFileDialog::getOpenFileName(
            this,
            "Load Intervals from CSV",
            QDir::currentPath(),
            "CSV files (*.csv);;All files (*.*)");

    if (interval_filename.isNull() || interval_filename.isEmpty()) {
        return;
    }

    _loadCSVFile(interval_filename, delimiterText);
}

void Digital_Interval_Loader_Widget::_loadCSVFile(QString const& filename, QString const& delimiterText) {
    auto const interval_key = ui->data_name_text->text().toStdString();
    if (interval_key.empty()) {
        std::cout << "Data name cannot be empty." << std::endl;
        return;
    }

    char delimiter;
    if (delimiterText == "Space") {
        delimiter = ' ';
    } else if (delimiterText == "Comma") {
        delimiter = ',';
    } else {
        std::cout << "Unsupported delimiter: " << delimiterText.toStdString() << std::endl;
        return;
    }

    try {
        auto intervals = load_digital_series_from_csv(filename.toStdString(), delimiter);
        std::cout << "Loaded " << intervals.size() << " intervals from " << filename.toStdString() << std::endl;

        _data_manager->setData<DigitalIntervalSeries>(interval_key);
        _data_manager->getData<DigitalIntervalSeries>(interval_key)->setData(intervals);
    } catch (std::exception const& e) {
        std::cerr << "Error loading CSV file " << filename.toStdString() << ": " << e.what() << std::endl;
    }
}
