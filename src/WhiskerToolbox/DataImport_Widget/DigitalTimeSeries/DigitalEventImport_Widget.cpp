#include "DigitalEventImport_Widget.hpp"

#include "ui_DigitalEventImport_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"
#include "DigitalTimeSeries/CSV/CSVDigitalEventImport_Widget.hpp"
#include "DataImportTypeRegistry.hpp"

#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>

#include <filesystem>
#include <iostream>

DigitalEventImport_Widget::DigitalEventImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalEventImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalEventImport_Widget::_onLoaderTypeChanged);

    connect(ui->csv_event_import, &CSVDigitalEventImport_Widget::loadCSVEventRequested,
            this, &DigitalEventImport_Widget::_handleLoadCSVEventRequested);

    _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
}

DigitalEventImport_Widget::~DigitalEventImport_Widget() {
    delete ui;
}

void DigitalEventImport_Widget::_onLoaderTypeChanged(int index) {
    QString current_text = ui->loader_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_event_import);
    } else {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_event_import);
    }
}

void DigitalEventImport_Widget::_handleLoadCSVEventRequested(CSVEventLoaderOptions options) {
    try {
        std::string data_name = ui->data_name_text->text().toStdString();
        if (data_name.empty()) {
            data_name = "events";
        }
        
        if (options.base_name == "events") {
            options.base_name = data_name;
        }

        std::vector<std::shared_ptr<DigitalEventSeries>> event_series_list = load(options);
        
        if (event_series_list.empty()) {
            QMessageBox::warning(this, tr("Import Warning"), 
                                tr("No event data found in CSV file."));
            return;
        }
        
        _loadCSVEventData(event_series_list, options);
        
    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV event data from " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"), 
            QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void DigitalEventImport_Widget::_loadCSVEventData(
    std::vector<std::shared_ptr<DigitalEventSeries>> const & event_series_list, 
    CSVEventLoaderOptions const & options) {
    
    int total_events = 0;
    int series_count = 0;
    QString last_key;

    if (options.identifier_column >= 0) {
        for (size_t i = 0; i < event_series_list.size(); ++i) {
            std::string series_key = options.base_name + "_" + std::to_string(i);
            _data_manager->setData<DigitalEventSeries>(series_key, event_series_list[i], TimeKey("time"));
            
            total_events += event_series_list[i]->size();
            series_count++;
            last_key = QString::fromStdString(series_key);
            std::cout << "Successfully loaded digital event series: " << series_key 
                      << " with " << event_series_list[i]->size() << " events" << std::endl;
        }
    } else {
        if (!event_series_list.empty()) {
            _data_manager->setData<DigitalEventSeries>(options.base_name, event_series_list[0], TimeKey("time"));
            
            total_events = event_series_list[0]->size();
            series_count = 1;
            last_key = QString::fromStdString(options.base_name);
            std::cout << "Successfully loaded digital event series: " << options.base_name 
                      << " with " << event_series_list[0]->size() << " events" << std::endl;
        }
    }

    QString message;
    if (series_count == 1) {
        message = QString::fromStdString("Digital Event data loaded into " + options.base_name + 
                                        " (" + std::to_string(total_events) + " events)");
    } else {
        message = QString::fromStdString("Digital Event data loaded: " + std::to_string(series_count) + 
                                        " series with " + std::to_string(total_events) + " total events");
    }
    
    QMessageBox::information(this, tr("Import Successful"), message);
    emit importCompleted(last_key, "DigitalEventSeries");
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct DigitalEventImportRegistrar {
    DigitalEventImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "DigitalEventSeries",
            ImportWidgetFactory{
                .display_name = "Digital Event Series",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new DigitalEventImport_Widget(std::move(dm), parent);
                }
            });
    }
} digital_event_import_registrar;
}
