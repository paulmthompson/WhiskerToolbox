#include "DigitalEventSeries_Widget.hpp"
#include "ui_DigitalEventSeries_Widget.h"

#include "EventTableModel.hpp"

#include "DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"
#include "DataExport_Widget/DigitalTimeSeries/CSV/CSVEventSaver_Widget.hpp"

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


DigitalEventSeries_Widget::DigitalEventSeries_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalEventSeries_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _event_table_model = new EventTableModel(this);
    ui->tableView->setModel(_event_table_model);

    ui->tableView->setEditTriggers(QAbstractItemView::SelectedClicked);

    connect(ui->add_event_button, &QPushButton::clicked, this, &DigitalEventSeries_Widget::_addEventButton);
    connect(ui->remove_event_button, &QPushButton::clicked, this, &DigitalEventSeries_Widget::_removeEventButton);
    connect(ui->tableView, &QTableView::doubleClicked, this, &DigitalEventSeries_Widget::_handleCellClicked);

    // Export section connections
    connect(ui->export_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalEventSeries_Widget::_onExportTypeChanged);
    connect(ui->csv_event_saver_widget, &CSVEventSaver_Widget::saveEventCSVRequested,
            this, &DigitalEventSeries_Widget::_handleSaveEventCSVRequested);

    // Set up export section
    ui->export_section->setTitle("Export");
    ui->export_section->autoSetContentLayout();
}

DigitalEventSeries_Widget::~DigitalEventSeries_Widget() {
    delete ui;
}

void DigitalEventSeries_Widget::openWidget() {
    // Populate the widget with data if needed
    this->show();
}

void DigitalEventSeries_Widget::setActiveKey(std::string key) {
    _active_key = std::move(key);

    _data_manager->removeCallbackFromData(_active_key, _callback_id);

    _assignCallbacks();

    _calculateEvents();
    
    // Update filename based on new active key
    _updateFilename();
}

void DigitalEventSeries_Widget::_changeDataTable(QModelIndex const & topLeft,
                                                 QModelIndex const & bottomRight,
                                                 QVector<int> const & roles) {
    static_cast<void>(roles);

    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);
    auto event_series = events->view();

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        auto newTime = _event_table_model->getEvent(row);
        if (static_cast<size_t>(row) < events->size()) {
            events->removeEvent(event_series[row].time());
            events->addEvent(newTime);
        }
    }
}

void DigitalEventSeries_Widget::_assignCallbacks() {
    _callback_id = _data_manager->addCallbackToData(_active_key, [this]() {
        _calculateEvents();
    });
}

void DigitalEventSeries_Widget::removeCallbacks() {
    _data_manager->removeCallbackFromData(_active_key, _callback_id);
}

void DigitalEventSeries_Widget::_calculateEvents() {
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);
    if (!events) return;

    ui->total_events_label->setText(QString::number(events->size()));

    auto event_view = events->view();
    std::vector<TimeFrameIndex> event_vector;
    for (auto const & event : event_view) {
        event_vector.push_back(event.time());
    }
    _event_table_model->setEvents(event_vector);
}


void DigitalEventSeries_Widget::_addEventButton() {

    auto current_time = _data_manager->getCurrentTime();
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    events->addEvent(TimeFrameIndex(current_time));

    std::cout << "Number of events is " << events->size() << std::endl;

    _calculateEvents();
}

void DigitalEventSeries_Widget::_removeEventButton() {

    auto current_time = _data_manager->getCurrentTime();
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);

    if (!events) return;

    bool removed = events->removeEvent(TimeFrameIndex(current_time));
    static_cast<void>(removed);

    _calculateEvents();
}

void DigitalEventSeries_Widget::_handleCellClicked(QModelIndex const & index) {
    if (!index.isValid()) {
        return;
    }

    auto const frameNumber = _event_table_model->getEvent(index.row());

    emit frameSelected(static_cast<int>(frameNumber.getValue()));
}

void DigitalEventSeries_Widget::_onExportTypeChanged(int index) {
    // Update the stacked widget to show the correct saver options widget
    ui->stacked_saver_options->setCurrentIndex(index);
    
    // Update filename when export type changes
    _updateFilename();
}

void DigitalEventSeries_Widget::_handleSaveEventCSVRequested(CSVEventSaverOptions options) {
    EventSaverOptionsVariant options_variant = options;
    _initiateSaveProcess(SaverType::CSV, options_variant);
}

void DigitalEventSeries_Widget::_initiateSaveProcess(SaverType saver_type, EventSaverOptionsVariant & options_variant) {
    // Get output path from DataManager
    auto output_path = _data_manager->getOutputPath();
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

bool DigitalEventSeries_Widget::_performActualCSVSave(CSVEventSaverOptions const & options) {
    auto events = _data_manager->getData<DigitalEventSeries>(_active_key);
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

std::string DigitalEventSeries_Widget::_generateFilename() const {
    if (_active_key.empty()) {
        return "events.csv";
    }
    
    // Get the export type
    QString export_type = ui->export_type_combo->currentText();
    
    if (export_type == "CSV") {
        return _active_key + ".csv";
    }
    
    return _active_key + ".csv"; // Default to CSV
}

void DigitalEventSeries_Widget::_updateFilename() {
    ui->filename_edit->setText(QString::fromStdString(_generateFilename()));
}
