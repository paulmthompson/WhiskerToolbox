#include "DigitalIntervalImport_Widget.hpp"

#include "ui_DigitalIntervalImport_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DataImportTypeRegistry.hpp"

#include <QFileDialog>
#include <QComboBox>
#include <QStackedWidget>
#include <QMessageBox>

#include <iostream>

DigitalIntervalImport_Widget::DigitalIntervalImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DigitalIntervalImport_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    connect(ui->loader_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DigitalIntervalImport_Widget::_onLoaderTypeChanged);

    connect(ui->csv_digital_interval_import_widget, &CSVDigitalIntervalImport_Widget::loadCSVIntervalRequested,
            this, &DigitalIntervalImport_Widget::_handleCSVLoadRequested);
    
#ifdef ENABLE_CAPNPROTO
    connect(ui->binary_digital_interval_import_widget, &BinaryDigitalIntervalImport_Widget::loadBinaryIntervalRequested,
            this, &DigitalIntervalImport_Widget::_handleBinaryLoadRequested);
#endif

    _onLoaderTypeChanged(0);
}

DigitalIntervalImport_Widget::~DigitalIntervalImport_Widget() {
    delete ui;
}

void DigitalIntervalImport_Widget::_onLoaderTypeChanged(int index) {
    static_cast<void>(index);

    if (ui->loader_type_combo->currentText() == "CSV") {
        ui->stacked_loader_options->setCurrentWidget(ui->csv_digital_interval_import_widget);
    } else if (ui->loader_type_combo->currentText() == "Binary") {
#ifdef ENABLE_CAPNPROTO
        ui->stacked_loader_options->setCurrentWidget(ui->binary_digital_interval_import_widget);
#else
        QMessageBox::warning(this, tr("Feature Not Available"),
            tr("Binary interval loading requires CapnProto support. Please rebuild with ENABLE_CAPNPROTO=ON."));
        ui->loader_type_combo->setCurrentIndex(0);
#endif
    }
}

void DigitalIntervalImport_Widget::_handleCSVLoadRequested(CSVIntervalLoaderOptions options) {
    auto const interval_key = ui->data_name_text->text().toStdString();
    if (interval_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Data name cannot be empty."));
        return;
    }

    try {
        auto intervals = load(options);
        std::cout << "Loaded " << intervals.size() << " intervals from " << options.filepath << std::endl;

        auto digital_interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        _data_manager->setData<DigitalIntervalSeries>(interval_key, digital_interval_series, TimeKey("time"));

        QMessageBox::information(this, tr("Import Successful"),
            tr("Loaded %1 intervals into '%2'")
                .arg(intervals.size())
                .arg(QString::fromStdString(interval_key)));

        emit importCompleted(QString::fromStdString(interval_key), "DigitalIntervalSeries");

    } catch (std::exception const & e) {
        std::cerr << "Error loading CSV file " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading CSV file: %1").arg(QString::fromStdString(e.what())));
    }
}

void DigitalIntervalImport_Widget::_handleBinaryLoadRequested(BinaryIntervalLoaderOptions options) {
    auto const interval_key = ui->data_name_text->text().toStdString();
    if (interval_key.empty()) {
        QMessageBox::warning(this, tr("Import Error"), tr("Data name cannot be empty."));
        return;
    }

    try {
        auto intervals = load(options);
        std::cout << "Loaded " << intervals.size() << " intervals from " << options.filepath << std::endl;

        auto digital_interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        _data_manager->setData<DigitalIntervalSeries>(interval_key, digital_interval_series, TimeKey("time"));

        QMessageBox::information(this, tr("Import Successful"),
            tr("Loaded %1 intervals into '%2'")
                .arg(intervals.size())
                .arg(QString::fromStdString(interval_key)));

        emit importCompleted(QString::fromStdString(interval_key), "DigitalIntervalSeries");

    } catch (std::exception const & e) {
        std::cerr << "Error loading binary file " << options.filepath << ": " << e.what() << std::endl;
        QMessageBox::critical(this, tr("Import Error"),
            tr("Error loading binary file: %1").arg(QString::fromStdString(e.what())));
    }
}

// Register with DataImportTypeRegistry at static initialization
namespace {
struct DigitalIntervalImportRegistrar {
    DigitalIntervalImportRegistrar() {
        DataImportTypeRegistry::instance().registerType(
            "DigitalIntervalSeries",
            ImportWidgetFactory{
                .display_name = "Digital Interval Series",
                .create_widget = [](std::shared_ptr<DataManager> dm, QWidget * parent) {
                    return new DigitalIntervalImport_Widget(std::move(dm), parent);
                }
            });
    }
} digital_interval_import_registrar;
}
