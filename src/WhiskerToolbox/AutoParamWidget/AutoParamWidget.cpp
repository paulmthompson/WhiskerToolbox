#include "AutoParamWidget.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <rfl/json.hpp>

#include <limits>
#include <sstream>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Construction / Destruction
// ============================================================================

AutoParamWidget::AutoParamWidget(QWidget * parent)
    : QWidget(parent) {
    _main_layout = new QVBoxLayout(this);
    _main_layout->setContentsMargins(0, 0, 0, 0);
    _main_layout->setSpacing(4);

    _form_layout = new QFormLayout();
    _form_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    _main_layout->addLayout(_form_layout);

    // Advanced section (initially hidden)
    _advanced_group = new QGroupBox(tr("Advanced"), this);
    _advanced_group->setCheckable(true);
    _advanced_group->setChecked(false);
    _advanced_group->setVisible(false);
    _advanced_layout = new QFormLayout();
    _advanced_group->setLayout(_advanced_layout);
    _main_layout->addWidget(_advanced_group);

    _main_layout->addStretch();
}

AutoParamWidget::~AutoParamWidget() = default;

// ============================================================================
// setSchema — Rebuild form from ParameterSchema
// ============================================================================

void AutoParamWidget::setSchema(ParameterSchema const & schema) {
    clearLayout();

    bool has_advanced = false;

    for (auto const & desc: schema.fields) {
        if (desc.is_advanced) {
            has_advanced = true;
            buildFieldRow(desc, _advanced_layout);
        } else {
            buildFieldRow(desc, _form_layout);
        }
    }

    _advanced_group->setVisible(has_advanced);
}

// ============================================================================
// buildFieldRow — Create widgets for a single field descriptor
// ============================================================================

void AutoParamWidget::buildFieldRow(ParameterFieldDescriptor const & desc,
                                    QFormLayout * layout) {
    FieldRow row;
    row.field_name = desc.name;
    row.type_name = desc.type_name;
    row.is_optional = desc.is_optional;

    // Create the value widget based on type
    QWidget * value_widget = nullptr;

    if (desc.type_name == "float" || desc.type_name == "double") {
        auto * spin = new QDoubleSpinBox(this);
        spin->setDecimals(6);
        spin->setSingleStep(0.1);

        // Set range from constraints (or use very wide defaults)
        double min_val = desc.min_value.value_or(-1e9);
        double max_val = desc.max_value.value_or(1e9);

        // Nudge bounds for exclusive constraints
        if (desc.is_exclusive_min && desc.min_value.has_value()) {
            min_val = desc.min_value.value() + std::numeric_limits<double>::epsilon() * 100;
        }
        if (desc.is_exclusive_max && desc.max_value.has_value()) {
            max_val = desc.max_value.value() - std::numeric_limits<double>::epsilon() * 100;
        }

        spin->setRange(min_val, max_val);
        spin->setValue(0.0);

        connect(spin, &QDoubleSpinBox::valueChanged, this, &AutoParamWidget::onFieldChanged);

        row.double_spin = spin;
        value_widget = spin;

    } else if (desc.type_name == "int") {
        auto * spin = new QSpinBox(this);

        int min_val = desc.min_value.has_value() ? static_cast<int>(desc.min_value.value()) : -1000000;
        int max_val = desc.max_value.has_value() ? static_cast<int>(desc.max_value.value()) : 1000000;

        if (desc.is_exclusive_min && desc.min_value.has_value()) {
            min_val = static_cast<int>(desc.min_value.value()) + 1;
        }
        if (desc.is_exclusive_max && desc.max_value.has_value()) {
            max_val = static_cast<int>(desc.max_value.value()) - 1;
        }

        spin->setRange(min_val, max_val);
        spin->setValue(0);

        connect(spin, &QSpinBox::valueChanged, this, &AutoParamWidget::onFieldChanged);

        row.int_spin = spin;
        value_widget = spin;

    } else if (desc.type_name == "bool") {
        auto * check = new QCheckBox(this);
        check->setChecked(false);

        connect(check, &QCheckBox::toggled, this, &AutoParamWidget::onFieldChanged);

        row.bool_check = check;
        value_widget = check;

    } else if (desc.type_name == "std::string" && !desc.allowed_values.empty()) {
        auto * combo = new QComboBox(this);
        for (auto const & val: desc.allowed_values) {
            combo->addItem(QString::fromStdString(val));
        }

        connect(combo, &QComboBox::currentIndexChanged, this, &AutoParamWidget::onFieldChanged);

        row.combo_box = combo;
        value_widget = combo;

    } else {
        // Default to QLineEdit for std::string and unknown types
        auto * edit = new QLineEdit(this);

        connect(edit, &QLineEdit::textChanged, this, &AutoParamWidget::onFieldChanged);

        row.line_edit = edit;
        value_widget = edit;
    }

    // For optional fields, wrap with a checkbox gate
    if (desc.is_optional && value_widget) {
        auto * container = new QWidget(this);
        auto * h_layout = new QHBoxLayout(container);
        h_layout->setContentsMargins(0, 0, 0, 0);
        h_layout->setSpacing(4);

        auto * gate = new QCheckBox(this);
        gate->setToolTip(tr("Enable this parameter (unchecked = use default)"));
        gate->setChecked(false);

        h_layout->addWidget(gate);
        h_layout->addWidget(value_widget, 1);

        // Disable the value widget when unchecked
        value_widget->setEnabled(false);
        connect(gate, &QCheckBox::toggled, value_widget, &QWidget::setEnabled);
        connect(gate, &QCheckBox::toggled, this, &AutoParamWidget::onFieldChanged);

        row.optional_gate = gate;

        QString const label = QString::fromStdString(desc.display_name);
        layout->addRow(label, container);
    } else if (value_widget) {
        QString const label = QString::fromStdString(desc.display_name);
        layout->addRow(label, value_widget);
    }

    // Set tooltip if available
    if (!desc.tooltip.empty() && value_widget) {
        value_widget->setToolTip(QString::fromStdString(desc.tooltip));
    }

    _field_rows.push_back(std::move(row));
}

// ============================================================================
// toJson — Serialize current widget values to JSON
// ============================================================================

std::string AutoParamWidget::toJson() const {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    for (auto const & row: _field_rows) {
        // For optional fields, skip if the gate checkbox is unchecked
        if (row.is_optional && row.optional_gate && !row.optional_gate->isChecked()) {
            continue;
        }

        if (!first) {
            oss << ",";
        }
        first = false;

        oss << "\"" << row.field_name << "\":";

        if (row.double_spin) {
            oss << row.double_spin->value();
        } else if (row.int_spin) {
            oss << row.int_spin->value();
        } else if (row.bool_check) {
            oss << (row.bool_check->isChecked() ? "true" : "false");
        } else if (row.combo_box) {
            oss << "\"" << row.combo_box->currentText().toStdString() << "\"";
        } else if (row.line_edit) {
            // Escape the string for JSON
            auto text = row.line_edit->text().toStdString();
            oss << "\"" << text << "\"";
        }
    }

    oss << "}";
    return oss.str();
}

// ============================================================================
// fromJson — Populate widget values from JSON
// ============================================================================

bool AutoParamWidget::fromJson(std::string const & json) {
    // Parse the JSON into a generic object
    auto result = rfl::json::read<rfl::Generic>(json);
    if (!result) {
        return false;
    }

    auto const & generic = result.value();

    // Check if generic is an object
    auto const * obj = std::get_if<rfl::Generic::Object>(&generic.get());
    if (!obj) {
        return false;
    }

    _updating = true;

    for (auto & row: _field_rows) {
        // Look for this field in the JSON object using the public get() API
        auto field_result = obj->get(row.field_name);
        bool const has_value = static_cast<bool>(field_result);

        // Handle the optional gate
        if (row.optional_gate) {
            row.optional_gate->setChecked(has_value);
        }

        if (!has_value) {
            continue;
        }

        auto const & value = field_result.value().get();

        if (row.double_spin) {
            if (auto const * num = std::get_if<double>(&value)) {
                row.double_spin->setValue(*num);
            } else if (auto const * inum = std::get_if<int>(&value)) {
                row.double_spin->setValue(static_cast<double>(*inum));
            }
        } else if (row.int_spin) {
            if (auto const * num = std::get_if<int>(&value)) {
                row.int_spin->setValue(*num);
            } else if (auto const * dnum = std::get_if<double>(&value)) {
                row.int_spin->setValue(static_cast<int>(*dnum));
            }
        } else if (row.bool_check) {
            if (auto const * b = std::get_if<bool>(&value)) {
                row.bool_check->setChecked(*b);
            }
        } else if (row.combo_box) {
            if (auto const * s = std::get_if<std::string>(&value)) {
                int const idx = row.combo_box->findText(QString::fromStdString(*s));
                if (idx >= 0) {
                    row.combo_box->setCurrentIndex(idx);
                }
            }
        } else if (row.line_edit) {
            if (auto const * s = std::get_if<std::string>(&value)) {
                row.line_edit->setText(QString::fromStdString(*s));
            }
        }
    }

    _updating = false;
    emit parametersChanged();
    return true;
}

// ============================================================================
// clear — Remove all field rows
// ============================================================================

void AutoParamWidget::clear() {
    clearLayout();
}

// ============================================================================
// Private helpers
// ============================================================================

void AutoParamWidget::clearLayout() {
    _field_rows.clear();

    // Remove all rows from form layout
    while (_form_layout->count() > 0) {
        auto * item = _form_layout->takeAt(0);
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    // Remove all rows from advanced layout
    while (_advanced_layout->count() > 0) {
        auto * item = _advanced_layout->takeAt(0);
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    _advanced_group->setVisible(false);
}

void AutoParamWidget::onFieldChanged() {
    if (!_updating) {
        emit parametersChanged();
    }
}
