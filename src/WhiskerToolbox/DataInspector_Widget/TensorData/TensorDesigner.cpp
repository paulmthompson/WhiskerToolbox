#include "TensorDesigner.hpp"

#include "ColumnConfigDialog.hpp"

#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataManager/DataManager.hpp"
#include "TensorDesign/ColumnRecipePresetRegistry.hpp"
#include "TensorDesign/DesignPresetRegistry.hpp"
#include "TensorDesign/TensorDesignBuilder.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"
#include "TransformsV2/core/TensorColumnBuilders.hpp"
#define slots Q_SLOTS

#include "DataManager/utils/TimeIndexExtractor.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/SelectionContext.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <nlohmann/json.hpp>
#include <type_traits>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace Neuralyzer::TensorBuilders;

namespace {

using DesignRowType = Neuralyzer::TensorDesign::RowType;

[[nodiscard]] DesignRowType toTensorDesignRowType(DesignerRowType row_type) {
    switch (row_type) {
        case DesignerRowType::Interval:
            return DesignRowType::Interval;
        case DesignerRowType::Timestamp:
            return DesignRowType::Timestamp;
        case DesignerRowType::Ordinal:
            return DesignRowType::Ordinal;
        case DesignerRowType::DerivedFromSource:
            return DesignRowType::DerivedFromSource;
        case DesignerRowType::TimeFrame:
            return DesignRowType::TimeFrame;
        case DesignerRowType::None:
            return DesignRowType::None;
    }
    return DesignRowType::None;
}

[[nodiscard]] DesignerRowType fromTensorDesignRowType(DesignRowType row_type) {
    switch (row_type) {
        case DesignRowType::Interval:
            return DesignerRowType::Interval;
        case DesignRowType::Timestamp:
            return DesignerRowType::Timestamp;
        case DesignRowType::Ordinal:
            return DesignerRowType::Ordinal;
        case DesignRowType::DerivedFromSource:
            return DesignerRowType::DerivedFromSource;
        case DesignRowType::TimeFrame:
            return DesignerRowType::TimeFrame;
        case DesignRowType::None:
            return DesignerRowType::None;
    }
    return DesignerRowType::None;
}

[[nodiscard]] int rowTypeComboIndex(DesignerRowType row_type) {
    switch (row_type) {
        case DesignerRowType::Interval:
            return 1;
        case DesignerRowType::Timestamp:
            return 2;
        case DesignerRowType::Ordinal:
            return 3;
        case DesignerRowType::DerivedFromSource:
            return 4;
        case DesignerRowType::TimeFrame:
            return 5;
        case DesignerRowType::None:
            return 0;
    }
    return 0;
}

[[nodiscard]] QString rowTypeDescription(int combo_index) {
    switch (combo_index) {
        case 1:
            return QStringLiteral(
                    "One row per interval (e.g. contact bouts). "
                    "Columns summarize data within each interval.");
        case 2:
            return QStringLiteral(
                    "One row per event timestamp (e.g. each spike). "
                    "Columns extract a value at or around each event.");
        case 3:
            return QStringLiteral(
                    "One row per index 0…N−1. "
                    "Not yet supported for building tensors.");
        case 4:
            return QStringLiteral(
                    "One row per timestamp extracted from any time-series data object "
                    "(analog, events, intervals, masks, lines, or points).");
        case 5:
            return QStringLiteral(
                    "One row per video frame or time index. "
                    "Use for frame-aligned features (e.g. keypoint x/y).");
        case 0:
        default:
            return QStringLiteral("Choose how tensor rows are defined.");
    }
}

}// namespace

namespace {

template<typename Descriptor>
[[nodiscard]] QString presetParameterHint(Descriptor const & descriptor) {
    QStringList names;
    if constexpr (std::is_same_v<std::decay_t<Descriptor>, Neuralyzer::TensorDesign::DesignPresetDescriptor>) {
        for (auto const & field: descriptor.parameters.fields) {
            names << QString::fromStdString(field.name);
        }
    } else {
        for (auto const & field: descriptor.parameters().fields) {
            names << QString::fromStdString(field.name);
        }
    }
    return names.join(QStringLiteral(", "));
}

[[nodiscard]] std::vector<std::string> parseSourceKeys(QString const & text) {
    std::vector<std::string> keys;
    for (auto const & part: text.split(QStringLiteral(","), Qt::SkipEmptyParts)) {
        auto const trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            keys.push_back(trimmed.toStdString());
        }
    }
    return keys;
}

}// namespace

// =============================================================================
// Construction
// =============================================================================

TensorDesigner::TensorDesigner(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      _data_manager(std::move(data_manager)) {
    _setupUi();
    _connectSignals();
}

TensorDesigner::~TensorDesigner() = default;

// =============================================================================
// SelectionContext
// =============================================================================

void TensorDesigner::setSelectionContext(SelectionContext * context) {
    if (_selection_context) {
        disconnect(_selection_context, nullptr, this, nullptr);
    }

    _selection_context = context;

    if (_selection_context) {
        connect(_selection_context, &SelectionContext::dataFocusChanged,
                this, [this](auto const & key, auto const & type, auto const & /*source*/) {
                    _onDataFocusChanged(key.toString(), type);
                });
    }
}

void TensorDesigner::setOperationContext(EditorLib::OperationContext * context) {
    _operation_context = context;
}

void TensorDesigner::setPipelineLibraryDir(QString const & library_dir) {
    _pipeline_library_dir = library_dir;
}

void TensorDesigner::setInspectorState(std::shared_ptr<DataInspectorState> state) {
    _inspector_state = std::move(state);
}

// =============================================================================
// Tensor management
// =============================================================================

void TensorDesigner::setTensorKey(std::string const & key) {
    _tensor_key = key;
    // TODO: If the tensor has LazyColumnTensorStorage, reconstruct recipes from it
}

// =============================================================================
// JSON Serialization
// =============================================================================

std::string TensorDesigner::toJson() const {
    Neuralyzer::TensorDesign::TensorDesignSpec spec;
    spec.tensor_key = _tensor_key;
    spec.row_type = toTensorDesignRowType(_row_type);
    if (_row_type == DesignerRowType::TimeFrame) {
        spec.row_time_key = _row_source_key;
    } else {
        spec.row_source_key = _row_source_key;
    }
    spec.columns = _column_recipes;
    return Neuralyzer::TensorDesign::serializeDesignJson(spec);
}

bool TensorDesigner::fromJson(std::string const & json) {
    auto const spec = Neuralyzer::TensorDesign::parseDesignJson(json);
    if (!spec.has_value()) {
        _updateStatus(QStringLiteral("Failed to load JSON"));
        return false;
    }

    _row_type = fromTensorDesignRowType(spec->row_type);
    _row_source_key = _row_type == DesignerRowType::TimeFrame ? spec->row_time_key
                                                              : spec->row_source_key;
    if (!spec->tensor_key.empty()) {
        _tensor_key = spec->tensor_key;
    }
    _column_recipes = spec->columns;

    switch (_row_type) {
        case DesignerRowType::Interval:
            _row_type_combo->setCurrentIndex(1);
            break;
        case DesignerRowType::Timestamp:
            _row_type_combo->setCurrentIndex(2);
            break;
        case DesignerRowType::Ordinal:
            _row_type_combo->setCurrentIndex(3);
            break;
        case DesignerRowType::DerivedFromSource:
            _row_type_combo->setCurrentIndex(4);
            break;
        case DesignerRowType::TimeFrame:
            _row_type_combo->setCurrentIndex(5);
            break;
        case DesignerRowType::None:
            _row_type_combo->setCurrentIndex(0);
            break;
    }

    for (int i = 0; i < _row_source_combo->count(); ++i) {
        if (_row_source_combo->itemData(i).toString().toStdString() == _row_source_key) {
            _row_source_combo->setCurrentIndex(i);
            break;
        }
    }

    _refreshColumnList();
    _updateStatus(QStringLiteral("Configuration loaded: %1 columns")
                          .arg(static_cast<int>(_column_recipes.size())));
    return true;
}

// =============================================================================
// =============================================================================
// Slots
// =============================================================================

void TensorDesigner::_onRowSourceTypeChanged(int index) {
    switch (index) {
        case 0:
            _row_type = DesignerRowType::None;
            break;
        case 1:
            _row_type = DesignerRowType::Interval;
            break;
        case 2:
            _row_type = DesignerRowType::Timestamp;
            break;
        case 3:
            _row_type = DesignerRowType::Ordinal;
            break;
        case 4:
            _row_type = DesignerRowType::DerivedFromSource;
            break;
        case 5:
            _row_type = DesignerRowType::TimeFrame;
            break;
        default:
            _row_type = DesignerRowType::None;
            break;
    }
    _updateRowTypeDescription(index);
    _populateRowSourceKeys();
}

void TensorDesigner::_updateRowTypeDescription(int combo_index) {
    auto const description = rowTypeDescription(combo_index);
    if (_row_type_description_label != nullptr) {
        _row_type_description_label->setText(description);
    }
    if (_row_type_combo != nullptr) {
        _row_type_combo->setToolTip(description);
    }
}

void TensorDesigner::_onRowSourceKeyChanged(int index) {
    if (index < 0) {
        _row_source_key.clear();
        _row_info_label->setText(QStringLiteral("No row source selected"));
        return;
    }

    _row_source_key = _row_source_combo->currentData().toString().toStdString();

    // Show row count info
    if (_row_type == DesignerRowType::Interval) {
        auto intervals = _data_manager->getData<DigitalIntervalSeries>(_row_source_key);
        if (intervals) {
            _row_info_label->setText(
                    QStringLiteral("Rows: %1 intervals").arg(static_cast<int>(intervals->size())));
        }
    } else if (_row_type == DesignerRowType::Timestamp) {
        auto events = _data_manager->getData<DigitalEventSeries>(_row_source_key);
        if (events) {
            _row_info_label->setText(
                    QStringLiteral("Rows: %1 timestamps").arg(static_cast<int>(events->size())));
        }
    } else if (_row_type == DesignerRowType::DerivedFromSource) {
        auto result = extractTimeIndices(*_data_manager, _row_source_key);
        _row_info_label->setText(
                QStringLiteral("Rows: %1 timestamps (derived from source)")
                        .arg(static_cast<int>(result.size())));
    } else if (_row_type == DesignerRowType::TimeFrame) {
        auto time_frame = _data_manager->getTime(TimeKey(_row_source_key));
        if (time_frame) {
            _row_info_label->setText(
                    QStringLiteral("Rows: %1 TimeFrame indices")
                            .arg(time_frame->getTotalFrameCount()));
        }
    }
}

void TensorDesigner::_onAddColumnClicked() {
    if (_row_type == DesignerRowType::None) {
        QMessageBox::warning(this, QStringLiteral("No Row Source"),
                             QStringLiteral("Please select a row source type first."));
        return;
    }

    // If a dialog is already open, just raise it
    if (_active_dialog) {
        _active_dialog->raise();
        _active_dialog->activateWindow();
        return;
    }

    _pinInspectorForDialog();

    auto * dialog = new ColumnConfigDialog(
            _data_manager, _row_type, _operation_context, _pipeline_library_dir, _row_source_key, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    _active_dialog = dialog;

    connect(dialog, &QDialog::accepted, this, &TensorDesigner::_onDialogAcceptedAdd);
    connect(dialog, &QDialog::finished, this, [this]() {
        _active_dialog = nullptr;
        _unpinInspectorAfterDialog();
    });

    dialog->show();
}

void TensorDesigner::_onApplyDesignPresetClicked() {
    auto registry = Neuralyzer::TensorDesign::createBuiltInDesignPresetRegistry();
    auto descriptors = registry.descriptors();
    if (descriptors.empty()) {
        _updateStatus(QStringLiteral("No design presets are registered."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Apply Table Preset"));
    auto * layout = new QVBoxLayout(&dialog);
    auto * form = new QFormLayout();

    auto * preset_combo = new QComboBox(&dialog);
    for (auto const * descriptor: descriptors) {
        preset_combo->addItem(
                QString::fromStdString(descriptor->display_name),
                QString::fromStdString(descriptor->id));
    }
    form->addRow(QStringLiteral("Preset"), preset_combo);

    auto * description_label = new QLabel(&dialog);
    description_label->setWordWrap(true);
    form->addRow(QStringLiteral("Description"), description_label);

    auto * row_source_key_edit = new QLineEdit(&dialog);
    auto * curvature_source_key_edit = new QLineEdit(&dialog);
    auto * spike_source_key_edit = new QLineEdit(&dialog);
    auto * angle_source_key_edit = new QLineEdit(&dialog);
    auto * keypoint_source_keys_edit = new QLineEdit(&dialog);
    auto * onset_pre_spin = new QSpinBox(&dialog);
    auto * onset_post_spin = new QSpinBox(&dialog);
    onset_pre_spin->setRange(0, 1'000'000'000);
    onset_post_spin->setRange(0, 1'000'000'000);

    form->addRow(QStringLiteral("Row source key"), row_source_key_edit);
    form->addRow(QStringLiteral("Curvature source key"), curvature_source_key_edit);
    form->addRow(QStringLiteral("Spike source key"), spike_source_key_edit);
    form->addRow(QStringLiteral("Angle source key"), angle_source_key_edit);
    form->addRow(QStringLiteral("Keypoint keys (comma-separated)"), keypoint_source_keys_edit);
    form->addRow(QStringLiteral("Onset pre"), onset_pre_spin);
    form->addRow(QStringLiteral("Onset post"), onset_post_spin);

    auto * hint_label = new QLabel(&dialog);
    hint_label->setWordWrap(true);
    form->addRow(QStringLiteral("Used fields"), hint_label);
    layout->addLayout(form);

    auto update_description = [&]() {
        auto const id = preset_combo->currentData().toString().toStdString();
        auto const * descriptor = registry.find(id);
        if (descriptor == nullptr) {
            description_label->clear();
            hint_label->clear();
            return;
        }
        description_label->setText(QString::fromStdString(descriptor->description));
        hint_label->setText(presetParameterHint(*descriptor));
    };
    update_description();
    connect(preset_combo, &QComboBox::currentIndexChanged, &dialog, update_description);

    auto * buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (_row_type != DesignerRowType::None || !_row_source_key.empty() || !_column_recipes.empty()) {
        auto const answer = QMessageBox::question(
                this,
                QStringLiteral("Replace Current Design?"),
                QStringLiteral("Applying a table preset replaces the current row source and columns. Continue?"));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    Neuralyzer::TensorDesign::DesignPresetArgs args;
    args.row_source_key = row_source_key_edit->text().trimmed().toStdString();
    args.curvature_source_key = curvature_source_key_edit->text().trimmed().toStdString();
    args.spike_source_key = spike_source_key_edit->text().trimmed().toStdString();
    args.angle_source_key = angle_source_key_edit->text().trimmed().toStdString();
    args.keypoint_source_keys = parseSourceKeys(keypoint_source_keys_edit->text());
    args.onset_pre = onset_pre_spin->value();
    args.onset_post = onset_post_spin->value();

    auto const preset_id = preset_combo->currentData().toString().toStdString();
    auto expansion = registry.expand(preset_id, args);
    if (!expansion.has_value()) {
        QMessageBox::warning(this, QStringLiteral("Preset Expansion Failed"),
                             QStringLiteral("Required preset parameters are missing or invalid."));
        return;
    }

    _row_type = fromTensorDesignRowType(expansion->spec.row_type);
    _row_source_key = _row_type == DesignerRowType::TimeFrame ? expansion->spec.row_time_key
                                                              : expansion->spec.row_source_key;
    _column_recipes = std::move(expansion->spec.columns);
    _row_type_combo->setCurrentIndex(rowTypeComboIndex(_row_type));
    _populateRowSourceKeys();
    _refreshColumnList();
    _updateStatus(QStringLiteral("Table preset applied: %1 columns")
                          .arg(static_cast<int>(_column_recipes.size())));
}

void TensorDesigner::_onAddPresetClicked() {
    if (_row_type == DesignerRowType::None) {
        QMessageBox::warning(this, QStringLiteral("No Row Source"),
                             QStringLiteral("Please select a row source type first."));
        return;
    }

    auto registry = Neuralyzer::TensorDesign::createBuiltInColumnRecipePresetRegistry();
    auto descriptors = registry.descriptors();
    if (descriptors.empty()) {
        _updateStatus(QStringLiteral("No column presets are registered."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Add Column Preset"));
    auto * layout = new QVBoxLayout(&dialog);
    auto * form = new QFormLayout();

    auto * preset_combo = new QComboBox(&dialog);
    for (auto const * descriptor: descriptors) {
        preset_combo->addItem(
                QString::fromStdString(descriptor->display_name()),
                QString::fromStdString(descriptor->id()));
    }
    form->addRow(QStringLiteral("Preset"), preset_combo);

    auto * description_label = new QLabel(&dialog);
    description_label->setWordWrap(true);
    form->addRow(QStringLiteral("Description"), description_label);

    auto * output_name_edit = new QLineEdit(&dialog);
    auto * source_key_edit = new QLineEdit(&dialog);
    auto * binding_source_key_edit = new QLineEdit(&dialog);
    auto * store_key_edit = new QLineEdit(QStringLiteral("row_alignment_time"), &dialog);
    auto * name_prefix_edit = new QLineEdit(&dialog);
    auto * source_keys_edit = new QLineEdit(&dialog);
    auto * pre_spin = new QSpinBox(&dialog);
    auto * post_spin = new QSpinBox(&dialog);
    auto * window_start_spin = new QDoubleSpinBox(&dialog);
    auto * window_end_spin = new QDoubleSpinBox(&dialog);

    pre_spin->setRange(0, 1'000'000'000);
    post_spin->setRange(0, 1'000'000'000);
    window_start_spin->setRange(-1'000'000'000.0, 1'000'000'000.0);
    window_end_spin->setRange(-1'000'000'000.0, 1'000'000'000.0);
    window_start_spin->setDecimals(3);
    window_end_spin->setDecimals(3);
    window_end_spin->setValue(15.0);

    form->addRow(QStringLiteral("Output name"), output_name_edit);
    form->addRow(QStringLiteral("Source key"), source_key_edit);
    form->addRow(QStringLiteral("Binding source key"), binding_source_key_edit);
    form->addRow(QStringLiteral("Store key"), store_key_edit);
    form->addRow(QStringLiteral("Name prefix"), name_prefix_edit);
    form->addRow(QStringLiteral("Source keys (comma-separated)"), source_keys_edit);
    form->addRow(QStringLiteral("Pre"), pre_spin);
    form->addRow(QStringLiteral("Post"), post_spin);
    form->addRow(QStringLiteral("Window start"), window_start_spin);
    form->addRow(QStringLiteral("Window end"), window_end_spin);

    auto * hint_label = new QLabel(&dialog);
    hint_label->setWordWrap(true);
    form->addRow(QStringLiteral("Used fields"), hint_label);

    layout->addLayout(form);

    auto update_description = [&]() {
        auto const id = preset_combo->currentData().toString().toStdString();
        auto const * descriptor = registry.find(id);
        if (descriptor == nullptr) {
            description_label->clear();
            hint_label->clear();
            return;
        }
        description_label->setText(QString::fromStdString(descriptor->description()));
        hint_label->setText(presetParameterHint(*descriptor));
    };
    update_description();
    connect(preset_combo, &QComboBox::currentIndexChanged, &dialog, update_description);

    auto * buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    Neuralyzer::TensorDesign::ColumnRecipePresetArgs args;
    args.output_name = output_name_edit->text().trimmed().toStdString();
    args.source_key = source_key_edit->text().trimmed().toStdString();
    args.binding_source_key = binding_source_key_edit->text().trimmed().toStdString();
    args.store_key = store_key_edit->text().trimmed().toStdString();
    args.name_prefix = name_prefix_edit->text().trimmed().toStdString();
    args.source_keys = parseSourceKeys(source_keys_edit->text());
    args.pre = pre_spin->value();
    args.post = post_spin->value();
    args.window_start = window_start_spin->value();
    args.window_end = window_end_spin->value();

    auto const preset_id = preset_combo->currentData().toString().toStdString();
    auto expansion = registry.expand(preset_id, args);
    if (!expansion.has_value()) {
        QMessageBox::warning(this, QStringLiteral("Preset Expansion Failed"),
                             QStringLiteral("Required preset parameters are missing or invalid."));
        return;
    }

    auto const added_count = expansion->columns.size();
    for (auto & recipe: expansion->columns) {
        _column_recipes.push_back(std::move(recipe));
    }
    _refreshColumnList();
    _updateStatus(QStringLiteral("Preset added: %1 columns total (+%2)")
                          .arg(static_cast<int>(_column_recipes.size()))
                          .arg(static_cast<int>(added_count)));
}

void TensorDesigner::_onRemoveColumnClicked() {
    auto * item = _column_list->currentItem();
    if (!item) {
        return;
    }

    int const row = _column_list->row(item);
    if (row >= 0 && row < static_cast<int>(_column_recipes.size())) {
        _column_recipes.erase(_column_recipes.begin() + row);
        _refreshColumnList();
        _updateStatus(QStringLiteral("Column removed: %1 columns remaining")
                              .arg(static_cast<int>(_column_recipes.size())));
    }
}

void TensorDesigner::_onEditColumnClicked() {
    auto * item = _column_list->currentItem();
    if (!item) {
        return;
    }

    int const row = _column_list->row(item);
    if (row < 0 || row >= static_cast<int>(_column_recipes.size())) {
        return;
    }

    // If a dialog is already open, just raise it
    if (_active_dialog) {
        _active_dialog->raise();
        _active_dialog->activateWindow();
        return;
    }

    _pinInspectorForDialog();

    auto * dialog = new ColumnConfigDialog(_data_manager,
                                           _row_type,
                                           _column_recipes[static_cast<size_t>(row)],
                                           _operation_context,
                                           _pipeline_library_dir,
                                           _row_source_key,
                                           this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    _active_dialog = dialog;

    connect(dialog, &QDialog::accepted, this, [this, row]() {
        _onDialogAcceptedEdit(row);
    });
    connect(dialog, &QDialog::finished, this, [this]() {
        _active_dialog = nullptr;
        _unpinInspectorAfterDialog();
    });

    dialog->show();
}

void TensorDesigner::_onColumnDoubleClicked(QListWidgetItem * item) {
    if (item) {
        _onEditColumnClicked();
    }
}

void TensorDesigner::_onBuildClicked() {
    _buildTensor();
}

void TensorDesigner::_onSaveJsonClicked() {
    QString const file_path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save Configuration"),
            QString(), QStringLiteral("JSON Files (*.json)"));

    if (file_path.isEmpty()) {
        return;
    }

    std::ofstream ofs(file_path.toStdString());
    if (ofs.is_open()) {
        ofs << toJson();
        _updateStatus(QStringLiteral("Configuration saved"));
    } else {
        _updateStatus(QStringLiteral("Failed to save configuration"));
    }
}

void TensorDesigner::_onLoadJsonClicked() {
    QString const file_path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Load Configuration"),
            QString(), QStringLiteral("JSON Files (*.json)"));

    if (file_path.isEmpty()) {
        return;
    }

    std::ifstream ifs(file_path.toStdString());
    if (ifs.is_open()) {
        std::string const content((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        fromJson(content);
    } else {
        _updateStatus(QStringLiteral("Failed to open file"));
    }
}

void TensorDesigner::_onDataFocusChanged(
        QString const & data_key,
        QString const & /*data_type*/) {

    // If user focuses a data key that could be a column source, do nothing
    // automatically — just update status to suggest it
    if (!data_key.isEmpty() && _data_manager) {
        auto const type = _data_manager->getType(data_key.toStdString());
        if (type == DM_DataType::Analog || type == DM_DataType::DigitalEvent ||
            type == DM_DataType::DigitalInterval || type == DM_DataType::Line) {
            _updateStatus(QStringLiteral("Focused: %1 — use 'Add Column' to include")
                                  .arg(data_key));
        }
    }
}

// =============================================================================
// Private implementation
// =============================================================================

void TensorDesigner::_setupUi() {
    _main_layout = new QVBoxLayout(this);
    _main_layout->setContentsMargins(4, 4, 4, 4);
    _main_layout->setSpacing(6);

    // === Row Source Section ===
    _row_section_label = new QLabel(QStringLiteral("<b>Row Source</b>"), this);
    _main_layout->addWidget(_row_section_label);

    auto * row_layout = new QHBoxLayout();
    row_layout->setSpacing(4);

    _row_type_combo = new QComboBox(this);
    _row_type_combo->addItem(QStringLiteral("(Select type...)"));
    _row_type_combo->addItem(QStringLiteral("Interval Rows"));
    _row_type_combo->addItem(QStringLiteral("Timestamp Rows"));
    _row_type_combo->addItem(QStringLiteral("Ordinal Rows"));
    _row_type_combo->addItem(QStringLiteral("Derived from Source"));
    _row_type_combo->addItem(QStringLiteral("TimeFrame Rows"));
    for (int i = 0; i < _row_type_combo->count(); ++i) {
        _row_type_combo->setItemData(i, rowTypeDescription(i), Qt::ToolTipRole);
    }
    _row_type_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row_layout->addWidget(_row_type_combo);

    _row_source_combo = new QComboBox(this);
    _row_source_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row_layout->addWidget(_row_source_combo);

    _main_layout->addLayout(row_layout);

    _row_type_description_label = new QLabel(this);
    _row_type_description_label->setWordWrap(true);
    _row_type_description_label->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    _main_layout->addWidget(_row_type_description_label);

    _row_info_label = new QLabel(QStringLiteral("No row source selected"), this);
    _row_info_label->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    _main_layout->addWidget(_row_info_label);

    // === Column Management Section ===
    _col_section_label = new QLabel(QStringLiteral("<b>Columns</b>"), this);
    _main_layout->addWidget(_col_section_label);

    _column_list = new QListWidget(this);
    _column_list->setAlternatingRowColors(true);
    _column_list->setMaximumHeight(200);
    _main_layout->addWidget(_column_list);

    _col_button_layout = new QHBoxLayout();
    _col_button_layout->setSpacing(4);

    _add_col_btn = new QPushButton(QStringLiteral("Add Column"), this);
    _apply_design_preset_btn = new QPushButton(QStringLiteral("Apply Table Preset..."), this);
    _add_preset_btn = new QPushButton(QStringLiteral("Add Preset..."), this);
    _edit_col_btn = new QPushButton(QStringLiteral("Edit"), this);
    _remove_col_btn = new QPushButton(QStringLiteral("Remove"), this);

    _col_button_layout->addWidget(_add_col_btn);
    _col_button_layout->addWidget(_apply_design_preset_btn);
    _col_button_layout->addWidget(_add_preset_btn);
    _col_button_layout->addWidget(_edit_col_btn);
    _col_button_layout->addWidget(_remove_col_btn);
    _col_button_layout->addStretch();

    _main_layout->addLayout(_col_button_layout);

    // === Build / Config Section ===
    _action_layout = new QHBoxLayout();
    _action_layout->setSpacing(4);

    _build_btn = new QPushButton(QStringLiteral("Build Tensor"), this);
    _build_btn->setStyleSheet(QStringLiteral("font-weight: bold;"));
    _save_json_btn = new QPushButton(QStringLiteral("Save Config"), this);
    _load_json_btn = new QPushButton(QStringLiteral("Load Config"), this);

    _action_layout->addWidget(_build_btn);
    _action_layout->addWidget(_save_json_btn);
    _action_layout->addWidget(_load_json_btn);

    _main_layout->addLayout(_action_layout);

    // === Status ===
    _status_label = new QLabel(this);
    _status_label->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    _status_label->setWordWrap(true);
    _main_layout->addWidget(_status_label);

    _main_layout->addStretch();

    _updateRowTypeDescription(_row_type_combo->currentIndex());
}

void TensorDesigner::_connectSignals() {
    connect(_row_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TensorDesigner::_onRowSourceTypeChanged);
    connect(_row_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TensorDesigner::_onRowSourceKeyChanged);
    connect(_add_col_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onAddColumnClicked);
    connect(_apply_design_preset_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onApplyDesignPresetClicked);
    connect(_add_preset_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onAddPresetClicked);
    connect(_edit_col_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onEditColumnClicked);
    connect(_remove_col_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onRemoveColumnClicked);
    connect(_column_list, &QListWidget::itemDoubleClicked,
            this, &TensorDesigner::_onColumnDoubleClicked);
    connect(_build_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onBuildClicked);
    connect(_save_json_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onSaveJsonClicked);
    connect(_load_json_btn, &QPushButton::clicked,
            this, &TensorDesigner::_onLoadJsonClicked);
}

void TensorDesigner::_populateRowSourceKeys() {
    _row_source_combo->blockSignals(true);
    _row_source_combo->clear();

    if (!_data_manager) {
        _row_source_combo->blockSignals(false);
        return;
    }

    if (_row_type == DesignerRowType::Interval) {
        auto keys = _data_manager->getKeys<DigitalIntervalSeries>();
        for (auto const & key: keys) {
            _row_source_combo->addItem(
                    QString::fromStdString(key), QString::fromStdString(key));
        }
    } else if (_row_type == DesignerRowType::Timestamp) {
        auto keys = _data_manager->getKeys<DigitalEventSeries>();
        for (auto const & key: keys) {
            _row_source_combo->addItem(
                    QString::fromStdString(key), QString::fromStdString(key));
        }
    } else if (_row_type == DesignerRowType::DerivedFromSource) {
        // Show all data sources that have timestamps (Analog, DigitalEvent,
        // DigitalInterval) so the user can derive row timestamps from any of them.
        auto all_keys = _data_manager->getAllKeys();
        for (auto const & key: all_keys) {
            auto const type = _data_manager->getType(key);
            bool const has_timestamps = (type == DM_DataType::Analog ||
                                         type == DM_DataType::DigitalEvent ||
                                         type == DM_DataType::DigitalInterval ||
                                         type == DM_DataType::Mask ||
                                         type == DM_DataType::Line ||
                                         type == DM_DataType::Points);
            if (has_timestamps) {
                auto type_str = QString::fromStdString(convert_data_type_to_string(type));
                auto display = QString::fromStdString(key) +
                               QStringLiteral(" [") + type_str + QStringLiteral("]");
                _row_source_combo->addItem(display, QString::fromStdString(key));
            }
        }
    } else if (_row_type == DesignerRowType::TimeFrame) {
        auto time_keys = _data_manager->getTimeFrameKeys();
        for (auto const & key: time_keys) {
            auto const key_string = key.str();
            _row_source_combo->addItem(
                    QString::fromStdString(key_string), QString::fromStdString(key_string));
        }
    }
    // For Ordinal, no source key needed

    _row_source_combo->blockSignals(false);

    _row_source_combo->setVisible(_row_type == DesignerRowType::Interval ||
                                  _row_type == DesignerRowType::Timestamp ||
                                  _row_type == DesignerRowType::DerivedFromSource ||
                                  _row_type == DesignerRowType::TimeFrame);

    if (_row_source_combo->count() > 0) {
        _onRowSourceKeyChanged(0);
    } else {
        _row_source_key.clear();
        _row_info_label->setText(
                _row_type == DesignerRowType::None
                        ? QStringLiteral("No row source selected")
                        : QStringLiteral("No compatible data found for this row type"));
    }
}

void TensorDesigner::_refreshColumnList() {
    _column_list->clear();
    for (auto const & recipe: _column_recipes) {
        QString text = QString::fromStdString(recipe.column_name);
        if (!recipe.source_key.empty()) {
            text += QStringLiteral(" [src: %1]").arg(QString::fromStdString(recipe.source_key));
        }
        if (recipe.interval_property.has_value()) {
            text += QStringLiteral(" (interval property)");
        } else if (!recipe.source_key.empty() && _data_manager) {
            auto const src_type = _data_manager->getType(recipe.source_key);
            if (src_type != DM_DataType::Unknown) {
                text += QStringLiteral(" \u2192 %1").arg(QString::fromStdString(convert_data_type_to_string(src_type)));
            }
        }
        _column_list->addItem(text);
    }
}

void TensorDesigner::_buildTensor() {
    if (_column_recipes.empty()) {
        _updateStatus(QStringLiteral("No columns configured. Add at least one column."));
        return;
    }

    if (_row_type == DesignerRowType::None) {
        _updateStatus(QStringLiteral("Please select a row source type."));
        return;
    }

    if (_tensor_key.empty()) {
        _tensor_key = "designed_tensor_" +
                      std::to_string(
                              std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // Pin the inspector so SelectionContext / feature-table refreshes during
    // setData() do not tear down this widget while the build is in flight.
    _pinInspectorForDialog();

    Neuralyzer::TensorDesign::TensorDesignSpec spec;
    spec.tensor_key = _tensor_key;
    spec.row_type = toTensorDesignRowType(_row_type);
    if (_row_type == DesignerRowType::TimeFrame) {
        spec.row_time_key = _row_source_key;
    } else {
        spec.row_source_key = _row_source_key;
    }
    spec.columns = _column_recipes;

    auto built = Neuralyzer::TensorDesign::buildTensor(*_data_manager, spec);
    if (!built.has_value()) {
        _unpinInspectorAfterDialog();
        _updateStatus(QStringLiteral("Build failed. Check row source and column configuration."));
        return;
    }

    auto const output_time_key = Neuralyzer::TensorDesign::resolveOutputTimeKey(*_data_manager, spec);
    if (!output_time_key.has_value()) {
        _unpinInspectorAfterDialog();
        _updateStatus(QStringLiteral(
                "Build succeeded but registration failed: no valid output TimeKey. "
                "Ensure the row source is assigned to a TimeFrame."));
        return;
    }

    spec.output_time_key = output_time_key->str();

    auto const num_rows = built->numRows();
    auto const column_count = _column_recipes.size();
    auto const tensor_key = _tensor_key;
    auto tensor_ptr = std::make_shared<TensorData>(std::move(built.value()));
    auto dm = _data_manager;
    auto const& time_key = output_time_key.value();

    // Defer registration so DataManager observer callbacks (feature table refresh,
    // inspector deletion checks) do not run while this click handler is unwinding.
    QPointer<TensorDesigner> const guard(this);
    QTimer::singleShot(0, this, [guard, dm, tensor_key, tensor_ptr, time_key, num_rows, column_count]() {
        if (!guard || !dm) {
            return;
        }

        dm->setData<TensorData>(tensor_key, tensor_ptr, time_key);

        auto const registered = dm->getData<TensorData>(tensor_key);
        if (!registered || dm->getTimeKey(tensor_key).empty()) {
            guard->_unpinInspectorAfterDialog();
            guard->_updateStatus(QStringLiteral(
                                         "Build succeeded but registration failed for '%1'. "
                                         "Check TimeFrame assignment and duplicate data keys.")
                                         .arg(QString::fromStdString(tensor_key)));
            return;
        }

        guard->_updateStatus(QStringLiteral("Tensor built: %1 rows × %2 columns → '%3'")
                                     .arg(static_cast<int>(num_rows))
                                     .arg(static_cast<int>(column_count))
                                     .arg(QString::fromStdString(tensor_key)));

        emit guard->tensorCreated(QString::fromStdString(tensor_key));
        guard->_unpinInspectorAfterDialog();
    });
}

void TensorDesigner::_updateStatus(QString const & message) {
    _status_label->setText(message);
}

// =============================================================================
// Dialog result handlers
// =============================================================================

void TensorDesigner::_onDialogAcceptedAdd() {
    if (!_active_dialog) return;

    auto recipes = _active_dialog->getRecipes();
    for (auto & recipe: recipes) {
        if (recipe.column_name.empty()) {
            recipe.column_name = "column_" + std::to_string(_column_recipes.size());
        }
        _column_recipes.push_back(std::move(recipe));
    }
    _refreshColumnList();
    _updateStatus(QStringLiteral("Column(s) added: %1 columns total")
                          .arg(static_cast<int>(_column_recipes.size())));
}

void TensorDesigner::_onDialogAcceptedEdit(int row) {
    if (!_active_dialog) return;
    if (row < 0 || row >= static_cast<int>(_column_recipes.size())) return;

    auto recipes = _active_dialog->getRecipes();
    if (recipes.empty()) return;

    _column_recipes[static_cast<size_t>(row)] = std::move(recipes[0]);
    if (recipes.size() > 1) {
        _column_recipes.insert(_column_recipes.begin() + row + 1,
                               std::make_move_iterator(recipes.begin() + 1),
                               std::make_move_iterator(recipes.end()));
    }
    _refreshColumnList();
}

// =============================================================================
// Inspector pin helpers
// =============================================================================

void TensorDesigner::_pinInspectorForDialog() {
    if (_inspector_state) {
        _was_pinned_before_dialog = _inspector_state->isPinned();
        if (!_was_pinned_before_dialog) {
            _inspector_state->setPinned(true);
        }
    }
}

void TensorDesigner::_unpinInspectorAfterDialog() {
    if (_inspector_state && !_was_pinned_before_dialog) {
        _inspector_state->setPinned(false);
    }
}
