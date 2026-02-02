#include "AnalogImport_Widget.hpp"

#include "ui_AnalogImport_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "DataManager/AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "DataImportTypeRegistry.hpp"

#include <QFileDialog>
#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>

#include <iostream>

AnalogImport_Widget::AnalogImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::AnalogImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogImport_Widget::_onLoaderTypeChanged);

    connect(ui->csv_analog_import_widget, &CSVAnalogImport_Widget::loadAnalogCSVRequested,
            this, &AnalogImport_Widget::_handleCSVLoadRequested);
    
#ifdef ENABLE_CAPNPROTO
    connect(ui->binary_analog_import_widget, &BinaryAnalogImport_Widget::loadBinaryAnalogRequested,
            this, &AnalogImport_Widget::_handleBinaryLoadRequested);
#endif

    _onLoaderTypeChanged(0);
}

AnalogImport_Widget::~AnalogImport_Widget() {
    delete ui;
}

void AnalogImport_Widget::_onLoaderTypeChanged(int index) {
    static_cast<void>(index);

    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_analog_import_widget);
    } else if (ui->loader_type_combo->currentText() == "Binary") {
#ifdef ENABLE_CAPNPROTO
        ui->stacked_loader_options->setCurrentWidget(ui->binary_analog_import_widget);
#else
        QMessageBox::warning(this, tr("Feature Not Available"),
            tr("Binary analog loading requires CapnProto support. Please rebuild with ENABLE_CAPNPROTO=ON."));
        ui->loader_type_combo->setCurrentIndex(0);
#endif
    }
}

void AnalogImport_Widget::_handleCSVLoadRequested(CSVAnalogLoaderOptions options) {
    auto const analog_key = ui->data_name_text->text().toStdString();
    if (analog_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Data name cannot be empty."));
        return;
    }

    try {
        auto analog_series = load(options);
        std::cout << "Loaded analog time series with " << analog_series->getNumSamples() 
                  << " samples from " << options.filepath << std::endl;

        _data_manager->setData<AnalogTimeSeries>(analog_key, analog_series, TimeKey("time"));

        QMessageBox::information(this, tr("Import Successful"),
            tr("Loaded %1 samples into '%2'")
                .arg(analog_series->getNumSamples())
                .arg(QString::fromStdString(analog_key)));

        emit importCompleted(QString::fromStdString(analog_key), "AnalogTimeSeries");

    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV file " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading CSV file: %1").arg(QString::fromStdString(e.what())));
    }
}

void AnalogImport_Widget::_handleBinaryLoadRequested(BinaryAnalogLoaderOptions options) {
    auto const analog_key = ui->data_name_text->text().toStdString();
    if (analog_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Data name cannot be empty."));
        return;
    }

    try {
        auto analog_series_vec = load(options);
        
        if (analog_series_vec.empty()) {
            QMessageBox::warning(this, tr("Import Error"), tr("No data loaded from binary file."));
            return;
        }
        
        // If multiple channels, use indexed names
        if (analog_series_vec.size() == 1) {
            auto const & analog_series = analog_series_vec[0];
            std::cout << "Loaded analog time series with " << analog_series->getNumSamples() 
                      << " samples from " << options.filepath << std::endl;

            _data_manager->setData<AnalogTimeSeries>(analog_key, analog_series, TimeKey("time"));

            QMessageBox::information(this, tr("Import Successful"),
                tr("Loaded %1 samples into '%2'")
                    .arg(analog_series->getNumSamples())
                    .arg(QString::fromStdString(analog_key)));

            emit importCompleted(QString::fromStdString(analog_key), "AnalogTimeSeries");
        } else {
            // Multiple channels loaded
            size_t total_samples = 0;
            for (size_t i = 0; i < analog_series_vec.size(); ++i) {
                auto const & analog_series = analog_series_vec[i];
                std::string channel_key = analog_key + "_ch" + std::to_string(i);
                
                _data_manager->setData<AnalogTimeSeries>(channel_key, analog_series, TimeKey("time"));
                total_samples += analog_series->getNumSamples();
                
                std::cout << "Loaded channel " << i << " with " << analog_series->getNumSamples() 
                          << " samples as '" << channel_key << "'" << std::endl;
            }

            QMessageBox::information(this, tr("Import Successful"),
                tr("Loaded %1 channels with %2 total samples from '%3'")
                    .arg(analog_series_vec.size())
                    .arg(total_samples)
                    .arg(QString::fromStdString(analog_key)));

            // Emit for the first channel (or a representative)
            emit importCompleted(QString::fromStdString(analog_key + "_ch0"), "AnalogTimeSeries");
        }

    } catch (std::exception const & e) {
        std::cerr << "Error loading binary file " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading binary file: %1").arg(QString::fromStdString(e.what())));
    }
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct AnalogImportRegistrar {
    AnalogImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "AnalogTimeSeries",
            ImportWidgetFactory{
                .display_name = "Analog Time Series",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new AnalogImport_Widget(std::move(dm), parent);
                }
            });
    }
} analog_import_registrar;
}
