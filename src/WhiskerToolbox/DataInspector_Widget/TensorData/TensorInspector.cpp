#include "TensorInspector.hpp"

#include "TensorColumnViewCreator.hpp"
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

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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
    _updateSectionVisibility();
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
    _updateSectionVisibility();
}

void TensorInspector::_onTensorCreated(QString const & key) {
    emit tensorCreated(key);
}

void TensorInspector::_onCreateColumnViews() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, QStringLiteral("Warning"),
                             QStringLiteral("No tensor selected"));
        return;
    }

    auto prefix = _view_prefix_edit->text().toStdString();
    if (prefix.empty()) {
        prefix = _active_key;
    }

    auto const count = createTensorColumnViews(
            *dataManager(), _active_key, prefix, {});

    if (count > 0) {
        QMessageBox::information(this, QStringLiteral("Column Views"),
                                 QStringLiteral("Created %1 AnalogTimeSeries column view(s)")
                                         .arg(count));
    } else {
        QMessageBox::warning(this, QStringLiteral("Column Views"),
                             QStringLiteral("Failed to create column views. "
                                            "Check that the tensor is 2D."));
    }
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

    // === Populate from Analog Section (collapsible, shown when tensor is empty) ===
    _populate_section = new Section(this, QStringLiteral("Create from AnalogTimeSeries"));
    auto * populate_content = new QWidget(this);
    auto * populate_layout = new QVBoxLayout(populate_content);
    populate_layout->setContentsMargins(0, 0, 0, 0);

    auto * group_layout = new QHBoxLayout();
    group_layout->addWidget(new QLabel(QStringLiteral("Channel group:"), this));
    _analog_group_combo = new QComboBox(this);
    _analog_group_combo->setToolTip(
            QStringLiteral("Groups of AnalogTimeSeries sharing a common prefix"));
    group_layout->addWidget(_analog_group_combo, 1);

    _refresh_groups_btn = new QPushButton(QStringLiteral("Refresh"), this);
    _refresh_groups_btn->setToolTip(QStringLiteral("Re-scan DataManager for analog channel groups"));
    group_layout->addWidget(_refresh_groups_btn);
    populate_layout->addLayout(group_layout);

    _populate_btn = new QPushButton(QStringLiteral("Populate Tensor"), this);
    _populate_btn->setToolTip(
            QStringLiteral("Pack the selected analog channels into this tensor"));
    populate_layout->addWidget(_populate_btn);

    _populate_section->setContentLayout(*populate_layout);
    layout->addWidget(_populate_section);

    connect(_refresh_groups_btn, &QPushButton::clicked,
            this, &TensorInspector::_onRefreshAnalogGroups);
    connect(_populate_btn, &QPushButton::clicked,
            this, &TensorInspector::_onPopulateFromAnalog);

    // === Column Views Section (collapsible) ===
    _column_views_section = new Section(this, QStringLiteral("Column Views"));
    auto * views_content = new QWidget(this);
    auto * views_layout = new QVBoxLayout(views_content);
    views_layout->setContentsMargins(0, 0, 0, 0);

    auto * prefix_layout = new QHBoxLayout();
    prefix_layout->addWidget(new QLabel(QStringLiteral("Key prefix:"), this));
    _view_prefix_edit = new QLineEdit(this);
    _view_prefix_edit->setPlaceholderText(QStringLiteral("e.g. tensor_views"));
    prefix_layout->addWidget(_view_prefix_edit);
    views_layout->addLayout(prefix_layout);

    _create_views_btn = new QPushButton(QStringLiteral("Create Column Views"), this);
    _create_views_btn->setToolTip(
            QStringLiteral("Create zero-copy AnalogTimeSeries views for each column"));
    views_layout->addWidget(_create_views_btn);

    _column_views_section->setContentLayout(*views_layout);
    layout->addWidget(_column_views_section);

    connect(_create_views_btn, &QPushButton::clicked,
            this, &TensorInspector::_onCreateColumnViews);

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
    if (_view_prefix_edit && !_active_key.empty()) {
        _view_prefix_edit->setText(QString::fromStdString(_active_key));
    }
}

void TensorInspector::_onPopulateFromAnalog() {
    if (_active_key.empty()) {
        QMessageBox::warning(this, QStringLiteral("Warning"),
                             QStringLiteral("No tensor selected"));
        return;
    }

    auto const idx = _analog_group_combo->currentIndex();
    if (idx < 0 || static_cast<std::size_t>(idx) >= _cached_groups.size()) {
        QMessageBox::warning(this, QStringLiteral("Warning"),
                             QStringLiteral("No channel group selected. Click Refresh first."));
        return;
    }

    auto const & group = _cached_groups[static_cast<std::size_t>(idx)];

    auto const success = populateTensorFromAnalogKeys(
            *dataManager(), _active_key, group.keys);

    if (success) {
        QMessageBox::information(this, QStringLiteral("Success"),
                                 QStringLiteral("Tensor populated with %1 channels from \"%2\"")
                                         .arg(group.keys.size())
                                         .arg(QString::fromStdString(group.prefix)));
        _updateSectionVisibility();
    } else {
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("Failed to populate tensor. Ensure all channels "
                                            "share the same TimeFrame and sample count."));
    }
}

void TensorInspector::_onRefreshAnalogGroups() {
    _cached_groups = discoverAnalogKeyGroups(*dataManager());

    _analog_group_combo->clear();
    for (auto const & group: _cached_groups) {
        auto const label = QString::fromStdString(group.prefix) +
                           QStringLiteral(" (%1 channels)").arg(group.keys.size());
        _analog_group_combo->addItem(label);
    }

    _populate_btn->setEnabled(!_cached_groups.empty());
}

void TensorInspector::_updateSectionVisibility() {
    if (_active_key.empty()) {
        return;
    }

    auto tensor = dataManager()->getData<TensorData>(_active_key);
    bool const is_empty = !tensor || tensor->isEmpty();

    // Show populate section only when tensor is empty
    if (_populate_section) {
        _populate_section->setVisible(is_empty);
        if (is_empty) {
            _onRefreshAnalogGroups();
        }
    }

    // Show column views and export only when tensor has data
    if (_column_views_section) {
        _column_views_section->setVisible(!is_empty);
    }
    if (_export_section) {
        _export_section->setVisible(!is_empty);
    }
}
