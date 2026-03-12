#include "AnalogTimeSeriesInspector.hpp"
#include "ui_AnalogTimeSeriesInspector.h"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DataExport_Widget/AnalogTimeSeries/CSV/CSVAnalogSaver_Widget.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>

#include <filesystem>
#include <rfl/json.hpp>

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
            this, [this](CSVAnalogSaverOptions options) {
                auto filename = ui->filename_edit->text().toStdString();
                if (filename.empty()) {
                    QMessageBox::warning(this, "Filename Missing", "Please enter a filename.");
                    return;
                }

                auto output_path = dataManager()->getOutputPath();
                if (output_path.empty()) {
                    QMessageBox::warning(this, "Warning",
                                         "Please set an output directory in the Data Manager settings");
                    return;
                }

                auto const filepath = (std::filesystem::path(output_path) / filename).string();

                options.parent_dir = output_path;
                options.filename = filename;

                auto const opts_json = rfl::json::write(options);
                auto format_opts = rfl::json::read<rfl::Generic>(opts_json);

                commands::SaveDataParams params{
                        .data_key = _active_key,
                        .format = "csv",
                        .path = filepath,
                };
                if (format_opts) {
                    params.format_options = format_opts.value();
                }

                commands::CommandContext ctx;
                ctx.data_manager = dataManager();
                ctx.recorder = commandRecorder();

                auto const params_json = rfl::json::write(params);
                auto result = commands::executeSingleCommand("SaveData", params_json, ctx);

                if (result.success) {
                    QMessageBox::information(this, "Success", "Analog time series saved successfully to CSV");
                } else {
                    QMessageBox::critical(this, "Error",
                                          QString("Failed to save: %1").arg(QString::fromStdString(result.error_message)));
                }
            });
}

void AnalogTimeSeriesInspector::_onExportTypeChanged(int index) {
    QString const current_text = ui->export_type_combo->itemText(index);
    if (current_text == "CSV") {
        ui->stacked_saver_options->setCurrentWidget(ui->csv_analog_saver_widget);
    } else {
        // Handle other export types if added in the future
    }
}
