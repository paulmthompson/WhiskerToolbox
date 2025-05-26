
#include "Analog_Loader_Widget.hpp"
#include "ui_Analog_Loader_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "IO_Widgets/AnalogTimeSeries/CSV/CSVAnalogLoader_Widget.hpp"

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

    // Set initial state of stacked widget
    ui->stacked_loader_options->setCurrentWidget(ui->csv_analog_loader_widget);
}

Analog_Loader_Widget::~Analog_Loader_Widget() {
    delete ui;
}

void Analog_Loader_Widget::_onLoaderTypeChanged(int index) {
    if (ui->loader_type_combo->itemText(index) == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_analog_loader_widget);
    }
    // Add else-if for other types when implemented
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
