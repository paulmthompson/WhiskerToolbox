#include "TensorInspector.hpp"

#include "TensorDesigner.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/IO/SaveData.hpp"
#include "DataExport_Widget/Tensors/CSV/CSVTensorSaver_Widget.hpp"
#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "Tensors/TensorData.hpp"
#define slots Q_SLOTS

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "EditorState/SelectionContext.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <filesystem>
#include <iostream>

TensorInspector::TensorInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager,
        QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupDesignerUi();
}

TensorInspector::~TensorInspector() {
    removeCallbacks();
}

void TensorInspector::setActiveKey(std::string const & key) {
    if (_active_key == key && _callback_id != -1) {
        return;
    }

    removeCallbacks();
    _active_key = key;
    _assignCallbacks();

    // If the active key has a LazyColumnTensorStorage tensor, set it on the designer
    if (_designer && !key.empty()) {
        _designer->setTensorKey(key);
    }

    _updateFilename();
}

void TensorInspector::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void TensorInspector::updateView() {
    // TensorInspector doesn't maintain its own UI - TensorDataView handles the table
    // This method is called when data changes, but the actual view update
    // is handled by TensorDataView through its own callbacks
}

void TensorInspector::setSelectionContext(SelectionContext * context) {
    if (_designer) {
        _designer->setSelectionContext(context);
    }
}

void TensorInspector::setOperationContext(EditorLib::OperationContext * context) {
    if (_designer) {
        _designer->setOperationContext(context);
    }
}

// =============================================================================
// Private slots
// =============================================================================

void TensorInspector::_onDataChanged() {
    updateView();
}

void TensorInspector::_onTensorCreated(QString const & key) {
    emit tensorCreated(key);
}

// =============================================================================
// Private implementation
// =============================================================================

void TensorInspector::_setupDesignerUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto * title_label = new QLabel(QStringLiteral("<b>Tensor Inspector</b>"), this);
    layout->addWidget(title_label);

    // Embed TensorDesigner
    _designer = new TensorDesigner(dataManager(), this);
    layout->addWidget(_designer);

    connect(_designer, &TensorDesigner::tensorCreated,
            this, &TensorInspector::_onTensorCreated);

    // === Export Section (collapsible) ===
    _export_section = new Section(this, QStringLiteral("Export"));
    auto * export_content = new QWidget(this);
    auto * export_layout = new QVBoxLayout(export_content);
    export_layout->setContentsMargins(0, 0, 0, 0);

    // Filename row
    auto * filename_layout = new QHBoxLayout();
    filename_layout->addWidget(new QLabel(QStringLiteral("Filename:"), this));
    _filename_edit = new QLineEdit(this);
    filename_layout->addWidget(_filename_edit);
    export_layout->addLayout(filename_layout);

    // CSV saver widget
    _csv_saver_widget = new CSVTensorSaver_Widget(this);
    export_layout->addWidget(_csv_saver_widget);

    _export_section->setContentLayout(*export_layout);
    layout->addWidget(_export_section);

    // Wire the save signal through the SaveData command
    connect(_csv_saver_widget, &CSVTensorSaver_Widget::saveTensorCSVRequested,
            this, [this](CSVTensorSaverOptions options) {
                auto output_path = dataManager()->getOutputPath();
                if (output_path.empty()) {
                    QMessageBox::warning(this, QStringLiteral("Warning"),
                                         QStringLiteral("Please set an output directory in the Data Manager settings"));
                    return;
                }

                auto filename = _filename_edit->text().toStdString();
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
                    QMessageBox::information(this, QStringLiteral("Success"),
                                             QStringLiteral("Tensor saved successfully to CSV"));
                } else {
                    QMessageBox::critical(this, QStringLiteral("Error"),
                                          QStringLiteral("Failed to save: %1")
                                                  .arg(QString::fromStdString(result.error_message)));
                }
            });

    layout->addStretch();
}

void TensorInspector::_assignCallbacks() {
    if (!_active_key.empty()) {
        auto tensor_data = dataManager()->getData<TensorData>(_active_key);
        if (tensor_data) {
            _callback_id = dataManager()->addCallbackToData(_active_key, [this]() { _onDataChanged(); });
        } else {
            std::cerr << "TensorInspector: No TensorData found for key '" << _active_key
                      << "' to attach callback." << std::endl;
        }
    }
}

std::string TensorInspector::_generateFilename() const {
    if (_active_key.empty()) {
        return "tensor.csv";
    }
    return _active_key + ".csv";
}

void TensorInspector::_updateFilename() {
    if (_filename_edit) {
        _filename_edit->setText(QString::fromStdString(_generateFilename()));
    }
}
