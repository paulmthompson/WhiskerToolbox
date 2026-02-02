#include "AnalogTimeSeriesInspector.hpp"
#include "ui_AnalogTimeSeriesInspector.h"

#include "DataExport_Widget/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "DataManager/DataManager.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>

#include <filesystem>
#include <iostream>

AnalogTimeSeriesInspector::AnalogTimeSeriesInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent),
      ui(new Ui::AnalogTimeSeriesInspector) {
    ui->setupUi(this);

    _connectSignals();

    // Setup collapsible export section
    ui->export_section->autoSetContentLayout();
    ui->export_section->setTitle("Export Options");
    ui->export_section->toggle(false);// Start collapsed

    _onExportTypeChanged(ui->export_type_combo->currentIndex());
}

AnalogTimeSeriesInspector::~AnalogTimeSeriesInspector() {
    removeCallbacks();
    delete ui;
}

void AnalogTimeSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;
}

void AnalogTimeSeriesInspector::removeCallbacks() {
    // AnalogTimeSeriesInspector doesn't use observers currently
}

void AnalogTimeSeriesInspector::updateView() {
    // AnalogTimeSeriesInspector auto-updates through setActiveKey
    // No explicit updateTable method
}

void AnalogTimeSeriesInspector::_connectSignals() {
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogTimeSeriesInspector::_onExportTypeChanged);

    connect(ui->csv_analog_saver_widget, &CSVAnalogSaver_Widget::saveAnalogCSVRequested,
            this, &AnalogTimeSeriesInspector::_handleSaveAnalogCSVRequested);
}

void AnalogTimeSeriesInspector::_onExportTypeChanged(int index) {
    QString current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_analog_saver_widget);
    } else {
        // Handle other export types if added in the future
    }
}

void AnalogTimeSeriesInspector::_handleSaveAnalogCSVRequested(CSVAnalogSaverOptions options) {
    options.filename = ui->filename_edit->text().toStdString();
    if (options.filename.empty()) {
        QMessageBox::warning(this, "Filename Missing", "Please enter a filename.");
        return;
    }
    AnalogSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void AnalogTimeSeriesInspector::_initiateSaveProcess(SaverType saver_type, AnalogSaverOptionsVariant & options_variant) {
    if (_active_key.empty()) {
        QMessageBox::warning(this, "No Data Selected", "Please select an AnalogTimeSeries item to save.");
        return;
    }

    auto analog_data_ptr = dataManager()->getData<AnalogTimeSeries>(_active_key);
    if (!analog_data_ptr) {
        QMessageBox::critical(this, "Error", "Could not retrieve AnalogTimeSeries for saving. Key: " + QString::fromStdString(_active_key));
        return;
    }

    bool save_successful = false;

    switch (saver_type) {
        case SaverType::CSV: {
            CSVAnalogSaverOptions & specific_csv_options = std::get<CSVAnalogSaverOptions>(options_variant);
            specific_csv_options.parent_dir = dataManager()->getOutputPath();
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

bool AnalogTimeSeriesInspector::_performActualCSVSave(CSVAnalogSaverOptions & options) {
    auto analog_data_ptr = dataManager()->getData<AnalogTimeSeries>(_active_key);
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
