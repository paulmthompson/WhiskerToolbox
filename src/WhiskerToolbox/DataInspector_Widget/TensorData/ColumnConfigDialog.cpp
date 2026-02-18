#include "ColumnConfigDialog.hpp"

#include "TensorDesigner.hpp" // For DesignerRowType

#include "DataManager/DataManager.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "TransformsV2/core/TensorColumnBuilders.hpp"
#define slots Q_SLOTS

#include <nlohmann/json.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace WhiskerToolbox::TensorBuilders;

// =============================================================================
// Helper: get DM_DataType string for display
// =============================================================================

namespace {

struct OperationEntry {
    QString display_name;
    QString operation_key;    ///< Reduction name or special key
    QString gather_type;      ///< "AnalogTimeSeries", "DigitalEventSeries", etc.
    bool is_interval_property{false};
    bool is_passthrough{false};
    bool is_offset{false};
};

std::vector<OperationEntry> getOperationsForSource(
    DM_DataType source_type,
    DesignerRowType row_type) {

    std::vector<OperationEntry> ops;

    if (row_type == DesignerRowType::Interval) {
        if (source_type == DM_DataType::Analog) {
            ops.push_back({QStringLiteral("Mean Value"), "MeanValue", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Max Value"), "MaxValue", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Min Value"), "MinValue", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Std Dev"), "StdValue", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Sum"), "SumValue", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Value Range"), "ValueRange", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Area Under Curve"), "AreaUnderCurve", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Time of Max"), "TimeOfMax", "AnalogTimeSeries", false, false, false});
            ops.push_back({QStringLiteral("Time of Min"), "TimeOfMin", "AnalogTimeSeries", false, false, false});
        } else if (source_type == DM_DataType::DigitalEvent) {
            ops.push_back({QStringLiteral("Event Count"), "EventCount", "DigitalEventSeries", false, false, false});
            ops.push_back({QStringLiteral("Event Presence"), "EventPresence", "DigitalEventSeries", false, false, false});
            ops.push_back({QStringLiteral("First Positive Latency"), "FirstPositiveLatency", "DigitalEventSeries", false, false, false});
            ops.push_back({QStringLiteral("Last Negative Latency"), "LastNegativeLatency", "DigitalEventSeries", false, false, false});
            ops.push_back({QStringLiteral("Mean Inter-Event Interval"), "MeanInterEventInterval", "DigitalEventSeries", false, false, false});
        } else if (source_type == DM_DataType::DigitalInterval) {
            ops.push_back({QStringLiteral("Interval Count"), "IntervalCount", "DigitalIntervalSeries", false, false, false});
            ops.push_back({QStringLiteral("Interval Start"), "IntervalStartExtract", "DigitalIntervalSeries", false, false, false});
            ops.push_back({QStringLiteral("Interval End"), "IntervalEndExtract", "DigitalIntervalSeries", false, false, false});
            ops.push_back({QStringLiteral("Interval Source Index"), "IntervalSourceIndex", "DigitalIntervalSeries", false, false, false});
            // Also interval properties from the row itself
            ops.push_back({QStringLiteral("Row Start (property)"), "IntervalStart", "", true, false, false});
            ops.push_back({QStringLiteral("Row End (property)"), "IntervalEnd", "", true, false, false});
            ops.push_back({QStringLiteral("Row Duration (property)"), "IntervalDuration", "", true, false, false});
        }

        // Interval property operations available regardless of source type
        // (they extract from the row intervals themselves)
        if (source_type != DM_DataType::DigitalInterval) {
            // Add interval properties only once (not duplicated)
            ops.push_back({QStringLiteral("Row Start (property)"), "IntervalStart", "", true, false, false});
            ops.push_back({QStringLiteral("Row End (property)"), "IntervalEnd", "", true, false, false});
            ops.push_back({QStringLiteral("Row Duration (property)"), "IntervalDuration", "", true, false, false});
        }
    } else if (row_type == DesignerRowType::Timestamp) {
        if (source_type == DM_DataType::Analog) {
            ops.push_back({QStringLiteral("Direct Value (passthrough)"), "Passthrough", "AnalogTimeSeries", false, true, false});
            ops.push_back({QStringLiteral("Value at Offset"), "AnalogSampleAtOffset", "AnalogTimeSeries", false, false, true});
        } else if (source_type == DM_DataType::Line) {
            ops.push_back({QStringLiteral("Line Length"), "CalculateLineLength", "LineData", false, false, false});
        }
    }

    return ops;
}

} // anonymous namespace

// =============================================================================
// Construction
// =============================================================================

ColumnConfigDialog::ColumnConfigDialog(
    std::shared_ptr<DataManager> data_manager,
    DesignerRowType row_type,
    QWidget * parent)
    : QDialog(parent)
    , _data_manager(std::move(data_manager))
    , _row_type(row_type) {
    _setupUi();
    _connectSignals();
    _populateSourceKeys();
}

ColumnConfigDialog::ColumnConfigDialog(
    std::shared_ptr<DataManager> data_manager,
    DesignerRowType row_type,
    ColumnRecipe const & recipe,
    QWidget * parent)
    : QDialog(parent)
    , _data_manager(std::move(data_manager))
    , _row_type(row_type) {
    _setupUi();
    _connectSignals();
    _populateSourceKeys();
    _applyRecipe(recipe);
}

ColumnConfigDialog::~ColumnConfigDialog() = default;

// =============================================================================
// Public API
// =============================================================================

ColumnRecipe ColumnConfigDialog::getRecipe() const {
    ColumnRecipe recipe;
    recipe.column_name = _name_edit->text().toStdString();

    // Source key
    int const source_idx = _source_combo->currentIndex();
    if (source_idx >= 0) {
        recipe.source_key = _source_combo->currentData().toString().toStdString();
    }

    // Operation
    int const op_idx = _operation_combo->currentIndex();
    if (op_idx >= 0) {
        auto const key = _operation_combo->currentData().toString().toStdString();
        auto const source_key_str = recipe.source_key;
        auto const source_type = _data_manager ? _data_manager->getType(source_key_str) : DM_DataType::Unknown;

        auto operations = getOperationsForSource(source_type, _row_type);
        for (auto const & op : operations) {
            if (op.operation_key.toStdString() == key) {
                if (op.is_interval_property) {
                    recipe.gather_type = "";
                    recipe.pipeline_json = "";
                    if (key == "IntervalStart") {
                        recipe.interval_property = IntervalProperty::Start;
                    } else if (key == "IntervalEnd") {
                        recipe.interval_property = IntervalProperty::End;
                    } else if (key == "IntervalDuration") {
                        recipe.interval_property = IntervalProperty::Duration;
                    }
                } else if (op.is_passthrough) {
                    recipe.gather_type = op.gather_type.toStdString();
                    recipe.pipeline_json = "";
                } else if (op.is_offset) {
                    recipe.gather_type = op.gather_type.toStdString();
                    recipe.pipeline_json = ""; // Offset handled separately
                    // For offset, we encode in pipeline_json
                    auto offset_val = static_cast<int64_t>(_offset_spin->value());
                    recipe.pipeline_json = R"({"offset": )" + std::to_string(offset_val) + "}";
                } else {
                    // Standard reduction
                    recipe.gather_type = op.gather_type.toStdString();
                    recipe.pipeline_json = R"({"range_reduction": {"name": ")" + key + R"(", "parameters": {}}})";
                }
                break;
            }
        }
    }

    return recipe;
}

// =============================================================================
// Slots
// =============================================================================

void ColumnConfigDialog::_onSourceKeyChanged(int /*index*/) {
    _populateOperations();
    _updateAutoName();
}

void ColumnConfigDialog::_onOperationChanged(int index) {
    if (index < 0) {
        _param_stack->setCurrentWidget(_empty_page);
        return;
    }

    auto const key = _operation_combo->currentData().toString();
    if (key == QStringLiteral("AnalogSampleAtOffset")) {
        _param_stack->setCurrentWidget(_offset_page);
    } else {
        _param_stack->setCurrentWidget(_empty_page);
    }

    _updateAutoName();
}

void ColumnConfigDialog::_onColumnNameEdited(QString const & /*text*/) {
    _auto_name = false; // User typed a custom name
}

void ColumnConfigDialog::_updateAutoName() {
    if (!_auto_name) {
        return;
    }

    QString name;
    int const source_idx = _source_combo->currentIndex();
    int const op_idx = _operation_combo->currentIndex();

    if (source_idx >= 0) {
        name = _source_combo->currentText();
    }
    if (op_idx >= 0) {
        QString op_name = _operation_combo->currentText();
        if (!name.isEmpty()) {
            name += QStringLiteral("_") + op_name;
        }
    }

    // Simplify: remove parenthetical descriptions
    name.remove(QStringLiteral(" (passthrough)"));
    name.remove(QStringLiteral(" (property)"));
    name.replace(QStringLiteral(" "), QStringLiteral("_"));

    _name_edit->blockSignals(true);
    _name_edit->setText(name);
    _name_edit->blockSignals(false);
}

// =============================================================================
// Setup
// =============================================================================

void ColumnConfigDialog::_setupUi() {
    setWindowTitle(QStringLiteral("Configure Column"));
    setMinimumWidth(400);

    _layout = new QVBoxLayout(this);

    // --- Source selection ---
    auto * source_group = new QGroupBox(QStringLiteral("Data Source"), this);
    auto * source_layout = new QFormLayout(source_group);

    _source_combo = new QComboBox(source_group);
    _source_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    source_layout->addRow(QStringLiteral("Source Key:"), _source_combo);

    _source_type_label = new QLabel(source_group);
    _source_type_label->setStyleSheet(QStringLiteral("color: gray;"));
    source_layout->addRow(QStringLiteral("Type:"), _source_type_label);

    _layout->addWidget(source_group);

    // --- Operation selection ---
    auto * op_group = new QGroupBox(QStringLiteral("Operation"), this);
    auto * op_layout = new QFormLayout(op_group);

    _operation_combo = new QComboBox(op_group);
    _operation_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    op_layout->addRow(QStringLiteral("Reduction/Transform:"), _operation_combo);

    // Parameter stack
    _param_stack = new QStackedWidget(op_group);

    // Empty page
    _empty_page = new QWidget(_param_stack);
    _param_stack->addWidget(_empty_page);

    // Offset page
    _offset_page = new QWidget(_param_stack);
    auto * offset_layout = new QFormLayout(_offset_page);
    offset_layout->setContentsMargins(0, 0, 0, 0);
    _offset_spin = new QDoubleSpinBox(_offset_page);
    _offset_spin->setRange(-100000, 100000);
    _offset_spin->setDecimals(0);
    _offset_spin->setValue(0);
    _offset_spin->setSuffix(QStringLiteral(" frames"));
    offset_layout->addRow(QStringLiteral("Offset:"), _offset_spin);
    _param_stack->addWidget(_offset_page);

    _param_stack->setCurrentWidget(_empty_page);
    op_layout->addRow(_param_stack);

    _layout->addWidget(op_group);

    // --- Column name ---
    auto * name_group = new QGroupBox(QStringLiteral("Column Name"), this);
    auto * name_layout = new QFormLayout(name_group);

    _name_edit = new QLineEdit(name_group);
    _name_edit->setPlaceholderText(QStringLiteral("Auto-generated from source and operation"));
    name_layout->addRow(QStringLiteral("Name:"), _name_edit);

    _layout->addWidget(name_group);

    // --- Dialog buttons ---
    auto * button_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    _layout->addWidget(button_box);
}

void ColumnConfigDialog::_connectSignals() {
    connect(_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColumnConfigDialog::_onSourceKeyChanged);
    connect(_operation_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColumnConfigDialog::_onOperationChanged);
    connect(_name_edit, &QLineEdit::textEdited,
            this, &ColumnConfigDialog::_onColumnNameEdited);
}

void ColumnConfigDialog::_populateSourceKeys() {
    _source_combo->blockSignals(true);
    _source_combo->clear();

    if (!_data_manager) {
        _source_combo->blockSignals(false);
        return;
    }

    // Get all keys and filter by compatible types
    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key : all_keys) {
        auto const type = _data_manager->getType(key);

        // Filter based on row type
        bool compatible = false;
        if (_row_type == DesignerRowType::Interval) {
            compatible = (type == DM_DataType::Analog ||
                          type == DM_DataType::DigitalEvent ||
                          type == DM_DataType::DigitalInterval);
        } else if (_row_type == DesignerRowType::Timestamp) {
            compatible = (type == DM_DataType::Analog ||
                          type == DM_DataType::Line);
        } else {
            // For None/Ordinal, show all data types
            compatible = (type != DM_DataType::Unknown &&
                          type != DM_DataType::Time);
        }

        if (compatible) {
            auto type_str = QString::fromStdString(convert_data_type_to_string(type));
            auto display = QString::fromStdString(key) +
                           QStringLiteral(" [") + type_str + QStringLiteral("]");
            _source_combo->addItem(display, QString::fromStdString(key));
        }
    }

    _source_combo->blockSignals(false);

    if (_source_combo->count() > 0) {
        _onSourceKeyChanged(0);
    }
}

void ColumnConfigDialog::_populateOperations() {
    _operation_combo->blockSignals(true);
    _operation_combo->clear();

    int const source_idx = _source_combo->currentIndex();
    if (source_idx < 0 || !_data_manager) {
        _operation_combo->blockSignals(false);
        return;
    }

    auto const source_key = _source_combo->currentData().toString().toStdString();
    auto const source_type = _data_manager->getType(source_key);

    // Update source type label
    _source_type_label->setText(
        QString::fromStdString(convert_data_type_to_string(source_type)));

    auto operations = getOperationsForSource(source_type, _row_type);
    for (auto const & op : operations) {
        _operation_combo->addItem(op.display_name, op.operation_key);
    }

    _operation_combo->blockSignals(false);

    if (_operation_combo->count() > 0) {
        _onOperationChanged(0);
    }
}

void ColumnConfigDialog::_applyRecipe(ColumnRecipe const & recipe) {
    _auto_name = false;

    // Set source key
    for (int i = 0; i < _source_combo->count(); ++i) {
        if (_source_combo->itemData(i).toString().toStdString() == recipe.source_key) {
            _source_combo->setCurrentIndex(i);
            break;
        }
    }

    // Determine which operation to select
    if (recipe.interval_property.has_value()) {
        QString op_key;
        switch (recipe.interval_property.value()) {
            case IntervalProperty::Start: op_key = QStringLiteral("IntervalStart"); break;
            case IntervalProperty::End: op_key = QStringLiteral("IntervalEnd"); break;
            case IntervalProperty::Duration: op_key = QStringLiteral("IntervalDuration"); break;
        }
        for (int i = 0; i < _operation_combo->count(); ++i) {
            if (_operation_combo->itemData(i).toString() == op_key) {
                _operation_combo->setCurrentIndex(i);
                break;
            }
        }
    } else if (recipe.pipeline_json.empty() && !recipe.source_key.empty()) {
        // Passthrough
        for (int i = 0; i < _operation_combo->count(); ++i) {
            if (_operation_combo->itemData(i).toString() == QStringLiteral("Passthrough")) {
                _operation_combo->setCurrentIndex(i);
                break;
            }
        }
    } else if (!recipe.pipeline_json.empty()) {
        // Try to parse offset
        if (recipe.pipeline_json.find("\"offset\"") != std::string::npos) {
            for (int i = 0; i < _operation_combo->count(); ++i) {
                if (_operation_combo->itemData(i).toString() == QStringLiteral("AnalogSampleAtOffset")) {
                    _operation_combo->setCurrentIndex(i);
                    break;
                }
            }
            // Parse offset value
            try {
                auto j = nlohmann::json::parse(recipe.pipeline_json);
                if (j.contains("offset")) {
                    _offset_spin->setValue(j["offset"].get<double>());
                }
            } catch (...) {}
        } else if (recipe.pipeline_json.find("\"range_reduction\"") != std::string::npos) {
            // Parse reduction name
            try {
                auto j = nlohmann::json::parse(recipe.pipeline_json);
                if (j.contains("range_reduction") && j["range_reduction"].contains("name")) {
                    auto const name = j["range_reduction"]["name"].get<std::string>();
                    for (int i = 0; i < _operation_combo->count(); ++i) {
                        if (_operation_combo->itemData(i).toString().toStdString() == name) {
                            _operation_combo->setCurrentIndex(i);
                            break;
                        }
                    }
                }
            } catch (...) {}
        }
    }

    // Set column name
    _name_edit->setText(QString::fromStdString(recipe.column_name));
}
