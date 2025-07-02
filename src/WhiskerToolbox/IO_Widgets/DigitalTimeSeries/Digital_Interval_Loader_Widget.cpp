#include "Digital_Interval_Loader_Widget.hpp"

#include "ui_Digital_Interval_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"

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

    connect(ui->csv_digital_interval_loader_widget, &CSVDigitalIntervalLoader_Widget::loadCSVIntervalRequested,
            this, &Digital_Interval_Loader_Widget::_handleCSVLoadRequested);
    
    connect(ui->binary_digital_interval_loader_widget, &BinaryDigitalIntervalLoader_Widget::loadBinaryIntervalRequested,
            this, &Digital_Interval_Loader_Widget::_handleBinaryLoadRequested);

    _onLoaderTypeChanged(0);
}

Digital_Interval_Loader_Widget::~Digital_Interval_Loader_Widget() {
    delete ui;
}

void Digital_Interval_Loader_Widget::_onLoaderTypeChanged(int index) {

    static_cast<void>(index);

    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_digital_interval_loader_widget);
    } else if (ui->loader_type_combo->currentText() == "Binary") {
        ui->stacked_loader_options->setCurrentWidget(ui->binary_digital_interval_loader_widget);
    }
}

void Digital_Interval_Loader_Widget::_handleCSVLoadRequested(CSVIntervalLoaderOptions options) {
    auto const interval_key = ui->data_name_text->text().toStdString();
    if (interval_key.empty()) {
        std::cout << "Data name cannot be empty." << std::endl;
        return;
    }

    try {
        auto intervals = load(options);
        std::cout << "Loaded " << intervals.size() << " intervals from " << options.filepath << std::endl;

        auto digital_interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        _data_manager->setData<DigitalIntervalSeries>(interval_key, digital_interval_series);
    } catch (std::exception const& e) {
        std::cerr << "Error loading CSV file " << options.filepath << ": " << e.what() << std::endl;
    }
}

void Digital_Interval_Loader_Widget::_handleBinaryLoadRequested(BinaryIntervalLoaderOptions options) {
    auto const interval_key = ui->data_name_text->text().toStdString();
    if (interval_key.empty()) {
        std::cout << "Data name cannot be empty." << std::endl;
        return;
    }

    try {
        auto intervals = load(options);
        std::cout << "Loaded " << intervals.size() << " intervals from " << options.filepath << std::endl;

        auto digital_interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        _data_manager->setData<DigitalIntervalSeries>(interval_key, digital_interval_series);
    } catch (std::exception const& e) {
        std::cerr << "Error loading binary file " << options.filepath << ": " << e.what() << std::endl;
    }
}


