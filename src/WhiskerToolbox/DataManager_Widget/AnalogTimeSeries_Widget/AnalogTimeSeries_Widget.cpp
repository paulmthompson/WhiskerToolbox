#include "AnalogTimeSeries_Widget.hpp"
#include "ui_AnalogTimeSeries_Widget.h"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "DataManager/DataManager.hpp"
#include "IO_Widgets/AnalogLoaderWidget/CSV/CSVAnalogSaver_Widget.hpp"

#include <iostream>
#include <QStackedWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <filesystem>

AnalogTimeSeries_Widget::AnalogTimeSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogTimeSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogTimeSeries_Widget::_onExportTypeChanged);

    connect(ui->csv_analog_saver_widget, &CSVAnalogSaver_Widget::saveAnalogCSVRequested,
            this, &AnalogTimeSeries_Widget::_handleSaveAnalogCSVRequested);
    
    _onExportTypeChanged(ui->export_type_combo->currentIndex()); 
}

AnalogTimeSeries_Widget::~AnalogTimeSeries_Widget() {
    delete ui;
}

void AnalogTimeSeries_Widget::openWidget() {
    this->show();
}

void AnalogTimeSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);
}

void AnalogTimeSeries_Widget::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_analog_saver_widget);
    } else {
        // Handle other export types if added in the future
    }
}

void AnalogTimeSeries_Widget::_handleSaveAnalogCSVRequested(CSVAnalogSaverOptions options) {
    options.filename = ui->filename_edit->text().toStdString();
    if (options.filename.empty()){
        QMessageBox::warning(this, "Filename Missing", "Please enter a filename.");
        return;
    }
    AnalogSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void AnalogTimeSeries_Widget::_initiateSaveProcess(SaverType saver_type, AnalogSaverOptionsVariant& options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select an AnalogTimeSeries item to save.");
        return;
    }

    auto analog_data_ptr = _data_manager->getData<AnalogTimeSeries>(_active_key);
    if (!analog_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve AnalogTimeSeries for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;

    switch (saver_type) {
        case SaverType::CSV: {
            CSVAnalogSaverOptions& specific_csv_options = std::get<CSVAnalogSaverOptions>(options_variant);
            specific_csv_options.parent_dir = _data_manager->getOutputPath().string();
            save_successful = _performActualCSVSave(specific_csv_options);
            break;
        }
        // Future saver types can be added here
    }

    if (!save_successful) {
        return; 
    }
    // No media export for analog data as per requirements
}

bool AnalogTimeSeries_Widget::_performActualCSVSave(CSVAnalogSaverOptions & options) {
    auto analog_data_ptr = _data_manager->getData<AnalogTimeSeries>(_active_key);
    if (!analog_data_ptr) {
        QMessageBox::critical(this, "Save Error", "Critical: Could not retrieve AnalogTimeSeries for saving during actual save. Key: " + QString::fromStdString(_active_key));
        return false;
    }

    try {
        save(analog_data_ptr.get(), options);
        return true;
    } catch (std::exception const & e) {
        QMessageBox::critical(this, "Save Error", "Failed to save analog data: " + QString::fromStdString(e.what()));
        std::cerr << "Failed to save analog data (CSV): " << e.what() << std::endl;
        return false;
    }
}
