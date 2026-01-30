#include "DigitalEventSeriesInspector.hpp"
#include "ui_DigitalEventSeriesInspector.h"

#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataExport_Widget/DigitalTimeSeries/CSV/CSVEventSaver_Widget.hpp"
#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

#include <filesystem>
#include <fstream>
#include <iostream>

DigitalEventSeriesInspector::DigitalEventSeriesInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent),
      ui(new Ui::DigitalEventSeriesInspector) {
    ui->setupUi(this);

    _connectSignals();

    // Set up export section
    ui->export_section->setTitle("Export");
    ui->export_section->autoSetContentLayout();
}

DigitalEventSeriesInspector::~DigitalEventSeriesInspector() {
    removeCallbacks();
    delete ui;
}

void DigitalEventSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;

    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
    }

    _assignCallbacks();

    _calculateEvents();

    // Update filename based on new active key
    _updateFilename();
}

void DigitalEventSeriesInspector::removeCallbacks() {
    if (!_active_key.empty() && _callback_id != -1) {
        dataManager()->removeCallbackFromData(_active_key, _callback_id);
        _callback_id = -1;
    }
}

void DigitalEventSeriesInspector::updateView() {
    // DigitalEventSeriesInspector auto-updates through callbacks
    // No explicit updateTable method
}

void DigitalEventSeriesInspector::_connectSignals() {
    connect(ui->add_event_button, &QPushButton::clicked, this, &DigitalEventSeriesInspector::_addEventButton);
    connect(ui->remove_event_button, &QPushButton::clicked, this, &DigitalEventSeriesInspector::_removeEventButton);

    // Export section connections
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalEventSeriesInspector::_onExportTypeChanged);
    connect(ui->csv_event_saver_widget, &CSVEventSaver_Widget::saveEventCSVRequested,
            this, &DigitalEventSeriesInspector::_handleSaveEventCSVRequested);
}

void DigitalEventSeriesInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        _callback_id = dataManager()->addCallbackToData(_active_key, [this]() {
            _calculateEvents();
        });
    }
}

void DigitalEventSeriesInspector::_calculateEvents() {
    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!events) return;

    ui->total_events_label->setText(QString::number(events->size()));
}

int64_t DigitalEventSeriesInspector::_getCurrentTimeInSeriesFrame() const {
    if (!state()) {
        return -1;
    }

    auto const & time_position = state()->current_position;
    if (!time_position.isValid() || !time_position.time_frame) {
        return -1;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!events) {
        return -1;
    }

    auto series_timeframe = events->getTimeFrame();
    if (!series_timeframe) {
        return -1;
    }

    // Convert the state's time_position to the series' timeframe
    TimeFrameIndex converted_index = time_position.convertTo(series_timeframe);
    return converted_index.getValue();
}

void DigitalEventSeriesInspector::_addEventButton() {
    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalEventSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    events->addEvent(TimeFrameIndex(current_time));

    std::cout << "Number of events is " << events->size() << std::endl;

    _calculateEvents();
}

void DigitalEventSeriesInspector::_removeEventButton() {
    auto current_time = _getCurrentTimeInSeriesFrame();
    if (current_time < 0) {
        std::cerr << "DigitalEventSeriesInspector: Failed to get current time in series frame" << std::endl;
        return;
    }

    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    bool const removed = events->removeEvent(TimeFrameIndex(current_time));
    static_cast<void>(removed);

    _calculateEvents();
}

void DigitalEventSeriesInspector::_onExportTypeChanged(int index) {
    // Update the stacked widget to show the correct saver options widget
    ui->stacked_saver_options->setCurrentIndex(index);

    // Update filename when export type changes
    _updateFilename();
}

void DigitalEventSeriesInspector::_handleSaveEventCSVRequested(CSVEventSaverOptions options) {
    EventSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(CSV, options_variant);
}

void DigitalEventSeriesInspector::_initiateSaveProcess(SaverType saver_type, EventSaverOptionsVariant & options_variant) {
    // Get output path from DataManager
    auto output_path = dataManager()->getOutputPath();
    if (output_path.empty()) {
        QMessageBox::warning(this, "Warning", "Please set an output directory in the Data Manager settings");
        return;
    }

    if (saver_type == SaverType::CSV) {
        auto & csv_options = std::get<CSVEventSaverOptions>(options_variant);
        csv_options.parent_dir = output_path;
        csv_options.filename = ui->filename_edit->text().toStdString();

        if (_performActualCSVSave(csv_options)) {
            QMessageBox::information(this, "Success", "Events saved successfully to CSV");
        }
    }
}

bool DigitalEventSeriesInspector::_performActualCSVSave(CSVEventSaverOptions const & options) {
    auto events = dataManager()->getData<DigitalEventSeries>(_active_key);
    if (!events) {
        QMessageBox::critical(this, "Error", "No event data available");
        return false;
    }

    try {
        ::save(events.get(), options);
        return true;
    } catch (std::runtime_error const & e) {
        QMessageBox::critical(this, "Error", QString("Failed to save CSV: %1").arg(e.what()));
        return false;
    }
}

std::string DigitalEventSeriesInspector::_generateFilename() const {
    if (_active_key.empty()) {
        return "events.csv";
    }

    // Get the export type
    QString const export_type = ui->export_type_combo->currentText();

    if (export_type == "CSV") {
        return _active_key + ".csv";
    }

    return _active_key + ".csv";// Default to CSV
}

void DigitalEventSeriesInspector::_updateFilename() {
    ui->filename_edit->setText(QString::fromStdString(_generateFilename()));
}
