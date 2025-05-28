#include "Digital_Event_Loader_Widget.hpp"

#include "ui_Digital_Event_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"
#include "IO_Widgets/DigitalTimeSeries/CSV/CSVDigitalEventLoader_Widget.hpp"

#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>

#include <filesystem>
#include <iostream>

Digital_Event_Loader_Widget::Digital_Event_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Digital_Event_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    // Connect signals
    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Digital_Event_Loader_Widget::_onLoaderTypeChanged);

    connect(ui->csv_event_loader, &CSVDigitalEventLoader_Widget::loadCSVEventRequested,
            this, &Digital_Event_Loader_Widget::_handleLoadCSVEventRequested);

    _onLoaderTypeChanged(ui->loader_type_combo->currentIndex());
}

Digital_Event_Loader_Widget::~Digital_Event_Loader_Widget() {
    delete ui;
}

void Digital_Event_Loader_Widget::_onLoaderTypeChanged(int index) {
    QString current_text = ui->loader_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_event_loader);
    } else {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_event_loader);
    }
}

void Digital_Event_Loader_Widget::_handleLoadCSVEventRequested(CSVEventLoaderOptions options) {
    try {
        // Use the data name from the UI if the base_name wasn't set in options
        if (options.base_name == "events") { // default value
            std::string data_name = ui->data_name_text->text().toStdString();
            if (!data_name.empty()) {
                options.base_name = data_name;
            }
        }

        std::vector<std::shared_ptr<DigitalEventSeries>> event_series_list = load(options);
        
        if (event_series_list.empty()) {
            QMessageBox::warning(this, "Load Warning", "No event data found in CSV file.");
            return;
        }
        
        _loadCSVEventData(event_series_list, options);
        
    } catch (std::exception const& e) {
        std::cerr << "Error loading CSV event data from " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", QString::fromStdString("Error loading CSV: " + std::string(e.what())));
    }
}

void Digital_Event_Loader_Widget::_loadCSVEventData(std::vector<std::shared_ptr<DigitalEventSeries>> const & event_series_list, 
                                                   CSVEventLoaderOptions const & options) {
    int total_events = 0;
    int series_count = 0;

    // Store each event series in the data manager
    if (options.identifier_column >= 0) {
        // Multi-column case: we don't know the actual identifiers here, so we'll use indices
        for (size_t i = 0; i < event_series_list.size(); ++i) {
            std::string series_key = options.base_name + "_" + std::to_string(i);
            _data_manager->setData<DigitalEventSeries>(series_key, event_series_list[i]);
            
            total_events += event_series_list[i]->size();
            series_count++;
            std::cout << "Successfully loaded digital event series: " << series_key 
                      << " with " << event_series_list[i]->size() << " events" << std::endl;
        }
    } else {
        // Single column case: use base name directly
        if (!event_series_list.empty()) {
            _data_manager->setData<DigitalEventSeries>(options.base_name, event_series_list[0]);
            
            total_events = event_series_list[0]->size();
            series_count = 1;
            std::cout << "Successfully loaded digital event series: " << options.base_name 
                      << " with " << event_series_list[0]->size() << " events" << std::endl;
        }
    }

    // Show success message
    QString message;
    if (series_count == 1) {
        message = QString::fromStdString("Digital Event data loaded into " + options.base_name + 
                                        " (" + std::to_string(total_events) + " events)");
    } else {
        message = QString::fromStdString("Digital Event data loaded: " + std::to_string(series_count) + 
                                        " series with " + std::to_string(total_events) + " total events");
    }
    
    QMessageBox::information(this, "Load Successful", message);
} 