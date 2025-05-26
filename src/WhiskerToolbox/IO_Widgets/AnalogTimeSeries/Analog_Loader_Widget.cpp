
#include "Analog_Loader_Widget.hpp"
#include "ui_Analog_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "DataManager/AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.hpp"
#include "IO_Widgets/AnalogTimeSeries/Binary/BinaryAnalogLoader_Widget.hpp"

#include <QComboBox>
#include <QStackedWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <iostream>

Analog_Loader_Widget::Analog_Loader_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Analog_Loader_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    // Connect loader type combo box
    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Analog_Loader_Widget::_onLoaderTypeChanged);

    // Connect signals from CSV loader widget
    connect(ui->csv_analog_loader_widget, &CSVAnalogLoader_Widget::loadAnalogCSVRequested,
            this, &Analog_Loader_Widget::_handleAnalogCSVLoadRequested);

    // Connect signals from Binary loader widget
    connect(ui->binary_analog_loader_widget, &BinaryAnalogLoader_Widget::loadBinaryAnalogRequested,
            this, &Analog_Loader_Widget::_handleBinaryAnalogLoadRequested);

    // Set initial state of stacked widget
    ui->stacked_loader_options->setCurrentWidget(ui->csv_analog_loader_widget);
}

Analog_Loader_Widget::~Analog_Loader_Widget() {
    delete ui;
}

void Analog_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_analog_loader_widget);
    } else if (ui->loader_type_combo->itemText(index) == "Binary") {
        ui->stacked_loader_options->setCurrentWidget(ui->binary_analog_loader_widget);
    }
}

void Analog_Loader_Widget::_handleAnalogCSVLoadRequested(CSVAnalogLoaderOptions options) {
    auto data_key = ui->data_name_text->text().toStdString();
    if (data_key.empty()) {
        data_key = "analog";
    }

    try {
        auto analog_data_ptr = load(options);
        _data_manager->setData<AnalogTimeSeries>(data_key, analog_data_ptr);
        
        std::cout << "Successfully loaded analog time series data with " 
                  << analog_data_ptr->getNumSamples() << " samples to key: " << data_key << std::endl;
                  
        QMessageBox::information(this, "Load Successful", 
                                QString::fromStdString("Loaded analog data with " + 
                                std::to_string(analog_data_ptr->getNumSamples()) + " samples."));
    } catch (std::exception const & e) {
        std::cerr << "Failed to load analog data: " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", 
                             QString::fromStdString("Failed to load analog data: " + std::string(e.what())));
    }
}

void Analog_Loader_Widget::_handleBinaryAnalogLoadRequested(BinaryAnalogLoaderOptions options) {
    auto base_data_key = ui->data_name_text->text().toStdString();
    if (base_data_key.empty()) {
        base_data_key = "analog";
    }

    try {
        auto analog_data_vector = load(options);
        
        if (analog_data_vector.empty()) {
            QMessageBox::warning(this, "Load Warning", "No analog data was loaded from the binary file.");
            return;
        }
        
        // Load each channel as a separate time series
        for (size_t channel = 0; channel < analog_data_vector.size(); ++channel) {
            std::string channel_key;
            if (analog_data_vector.size() == 1) {
                // Single channel - use base name
                channel_key = base_data_key;
            } else {
                // Multiple channels - append channel number
                channel_key = base_data_key + "_" + std::to_string(channel);
            }
            
            _data_manager->setData<AnalogTimeSeries>(channel_key, analog_data_vector[channel]);
            
            std::cout << "Successfully loaded analog time series channel " << channel 
                      << " with " << analog_data_vector[channel]->getNumSamples() 
                      << " samples to key: " << channel_key << std::endl;
        }
        
        QString message;
        if (analog_data_vector.size() == 1) {
            message = QString::fromStdString("Loaded analog data with " + 
                     std::to_string(analog_data_vector[0]->getNumSamples()) + " samples.");
        } else {
            message = QString::fromStdString("Loaded " + std::to_string(analog_data_vector.size()) + 
                     " analog channels with " + std::to_string(analog_data_vector[0]->getNumSamples()) + 
                     " samples each.");
        }
        
        QMessageBox::information(this, "Load Successful", message);
        
    } catch (std::exception const & e) {
        std::cerr << "Failed to load binary analog data: " << e.what() << std::endl;
        QMessageBox::critical(this, "Load Error", 
                             QString::fromStdString("Failed to load binary analog data: " + std::string(e.what())));
    }
}
