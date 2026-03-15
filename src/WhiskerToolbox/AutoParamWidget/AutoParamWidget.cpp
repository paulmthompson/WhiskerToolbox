#include "AutoParamWidget.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
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
    // Variant (TaggedUnion) fields get a specialized layout
    if (desc.is_variant) {
        buildVariantRow(desc, layout);
        return;
    }

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

        // Apply default value if available, otherwise 0.0
        if (desc.default_value_json.has_value()) {
            auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
            if (def_result) {
                auto const & val = def_result.value().get();
                if (auto const * d = std::get_if<double>(&val)) {
                    spin->setValue(*d);
                } else if (auto const * i = std::get_if<int>(&val)) {
                    spin->setValue(static_cast<double>(*i));
                }
            }
        } else {
            spin->setValue(0.0);
        }

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

        // Apply default value if available, otherwise 0
        if (desc.default_value_json.has_value()) {
            auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
            if (def_result) {
                auto const & val = def_result.value().get();
                if (auto const * i = std::get_if<int>(&val)) {
                    spin->setValue(*i);
                } else if (auto const * d = std::get_if<double>(&val)) {
                    spin->setValue(static_cast<int>(*d));
                }
            }
        } else {
            spin->setValue(0);
        }

        connect(spin, &QSpinBox::valueChanged, this, &AutoParamWidget::onFieldChanged);

        row.int_spin = spin;
        value_widget = spin;

    } else if (desc.type_name == "bool") {
        auto * check = new QCheckBox(this);

        // Apply default value if available, otherwise false
        if (desc.default_value_json.has_value()) {
            auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
            if (def_result) {
                auto const & val = def_result.value().get();
                if (auto const * b = std::get_if<bool>(&val)) {
                    check->setChecked(*b);
                }
            }
        } else {
            check->setChecked(false);
        }

        connect(check, &QCheckBox::toggled, this, &AutoParamWidget::onFieldChanged);

        row.bool_check = check;
        value_widget = check;

    } else if ((desc.type_name == "std::string" || desc.type_name == "enum") && !desc.allowed_values.empty()) {
        auto * combo = new QComboBox(this);
        for (auto const & val: desc.allowed_values) {
            combo->addItem(QString::fromStdString(val));
        }

        // Apply default value if available
        if (desc.default_value_json.has_value()) {
            auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
            if (def_result) {
                auto const & val = def_result.value().get();
                if (auto const * s = std::get_if<std::string>(&val)) {
                    int const idx = combo->findText(QString::fromStdString(*s));
                    if (idx >= 0) {
                        combo->setCurrentIndex(idx);
                    }
                }
            }
        }

        connect(combo, &QComboBox::currentIndexChanged, this, &AutoParamWidget::onFieldChanged);

        row.combo_box = combo;
        value_widget = combo;

    } else {
        // Default to QLineEdit for std::string and unknown types
        auto * edit = new QLineEdit(this);

        // Apply default value if available
        if (desc.default_value_json.has_value()) {
            auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
            if (def_result) {
                auto const & val = def_result.value().get();
                if (auto const * s = std::get_if<std::string>(&val)) {
                    edit->setText(QString::fromStdString(*s));
                }
            }
        }

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
// buildVariantRow — Create combo box + stacked sub-forms for a variant field
// ============================================================================

void AutoParamWidget::buildVariantRow(ParameterFieldDescriptor const & desc,
                                      QFormLayout * layout) {
    FieldRow row;
    row.field_name = desc.name;
    row.type_name = "variant";
    row.is_variant = true;
    row.variant_discriminator = desc.variant_discriminator;

    // Container widget holds combo + stacked widget vertically
    auto * container = new QWidget(this);
    auto * v_layout = new QVBoxLayout(container);
    v_layout->setContentsMargins(0, 0, 0, 0);
    v_layout->setSpacing(4);

    // Combo box for selecting the active alternative
    auto * combo = new QComboBox(this);
    for (auto const & alt: desc.variant_alternatives) {
        combo->addItem(QString::fromStdString(alt.tag));
    }
    v_layout->addWidget(combo);
    row.variant_combo = combo;

    // Stacked widget: one page per alternative, each with its own form layout
    auto * stack = new QStackedWidget(this);
    row.variant_sub_rows.resize(desc.variant_alternatives.size());

    for (size_t i = 0; i < desc.variant_alternatives.size(); ++i) {
        auto const & alt = desc.variant_alternatives[i];
        auto * page = new QWidget(this);
        auto * page_layout = new QFormLayout(page);
        page_layout->setContentsMargins(4, 4, 4, 4);
        page_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

        // Build sub-field rows for this alternative's schema
        for (auto const & sub_desc: alt.schema->fields) {
            FieldRow sub_row;
            sub_row.field_name = sub_desc.name;
            sub_row.type_name = sub_desc.type_name;
            sub_row.is_optional = sub_desc.is_optional;

            QWidget * value_widget = nullptr;

            if (sub_desc.type_name == "float" || sub_desc.type_name == "double") {
                auto * spin = new QDoubleSpinBox(page);
                spin->setDecimals(6);
                spin->setSingleStep(0.1);
                double min_val = sub_desc.min_value.value_or(-1e9);
                double max_val = sub_desc.max_value.value_or(1e9);
                if (sub_desc.is_exclusive_min && sub_desc.min_value.has_value()) {
                    min_val = sub_desc.min_value.value() + std::numeric_limits<double>::epsilon() * 100;
                }
                if (sub_desc.is_exclusive_max && sub_desc.max_value.has_value()) {
                    max_val = sub_desc.max_value.value() - std::numeric_limits<double>::epsilon() * 100;
                }
                spin->setRange(min_val, max_val);
                if (sub_desc.default_value_json.has_value()) {
                    auto def_result = rfl::json::read<rfl::Generic>(sub_desc.default_value_json.value());
                    if (def_result) {
                        auto const & val = def_result.value().get();
                        if (auto const * d = std::get_if<double>(&val)) {
                            spin->setValue(*d);
                        } else if (auto const * ii = std::get_if<int>(&val)) {
                            spin->setValue(static_cast<double>(*ii));
                        }
                    }
                }
                connect(spin, &QDoubleSpinBox::valueChanged, this, &AutoParamWidget::onFieldChanged);
                sub_row.double_spin = spin;
                value_widget = spin;
            } else if (sub_desc.type_name == "int") {
                auto * spin = new QSpinBox(page);
                int const min_val = sub_desc.min_value.has_value() ? static_cast<int>(sub_desc.min_value.value()) : -1000000;
                int const max_val = sub_desc.max_value.has_value() ? static_cast<int>(sub_desc.max_value.value()) : 1000000;
                spin->setRange(min_val, max_val);
                if (sub_desc.default_value_json.has_value()) {
                    auto def_result = rfl::json::read<rfl::Generic>(sub_desc.default_value_json.value());
                    if (def_result) {
                        auto const & val = def_result.value().get();
                        if (auto const * ii = std::get_if<int>(&val)) {
                            spin->setValue(*ii);
                        } else if (auto const * d = std::get_if<double>(&val)) {
                            spin->setValue(static_cast<int>(*d));
                        }
                    }
                }
                connect(spin, &QSpinBox::valueChanged, this, &AutoParamWidget::onFieldChanged);
                sub_row.int_spin = spin;
                value_widget = spin;
            } else if (sub_desc.type_name == "bool") {
                auto * check = new QCheckBox(page);
                if (sub_desc.default_value_json.has_value()) {
                    auto def_result = rfl::json::read<rfl::Generic>(sub_desc.default_value_json.value());
                    if (def_result) {
                        auto const & val = def_result.value().get();
                        if (auto const * b = std::get_if<bool>(&val)) {
                            check->setChecked(*b);
                        }
                    }
                }
                connect(check, &QCheckBox::toggled, this, &AutoParamWidget::onFieldChanged);
                sub_row.bool_check = check;
                value_widget = check;
            } else if ((sub_desc.type_name == "std::string" || sub_desc.type_name == "enum") &&
                       !sub_desc.allowed_values.empty()) {
                auto * sub_combo = new QComboBox(page);
                for (auto const & val: sub_desc.allowed_values) {
                    sub_combo->addItem(QString::fromStdString(val));
                }
                if (sub_desc.default_value_json.has_value()) {
                    auto def_result = rfl::json::read<rfl::Generic>(sub_desc.default_value_json.value());
                    if (def_result) {
                        auto const & val = def_result.value().get();
                        if (auto const * s = std::get_if<std::string>(&val)) {
                            int const idx = sub_combo->findText(QString::fromStdString(*s));
                            if (idx >= 0) sub_combo->setCurrentIndex(idx);
                        }
                    }
                }
                connect(sub_combo, &QComboBox::currentIndexChanged, this, &AutoParamWidget::onFieldChanged);
                sub_row.combo_box = sub_combo;
                value_widget = sub_combo;
            } else {
                auto * edit = new QLineEdit(page);
                if (sub_desc.default_value_json.has_value()) {
                    auto def_result = rfl::json::read<rfl::Generic>(sub_desc.default_value_json.value());
                    if (def_result) {
                        auto const & val = def_result.value().get();
                        if (auto const * s = std::get_if<std::string>(&val)) {
                            edit->setText(QString::fromStdString(*s));
                        }
                    }
                }
                connect(edit, &QLineEdit::textChanged, this, &AutoParamWidget::onFieldChanged);
                sub_row.line_edit = edit;
                value_widget = edit;
            }

            if (value_widget) {
                QString const label = QString::fromStdString(sub_desc.display_name);
                page_layout->addRow(label, value_widget);
                if (!sub_desc.tooltip.empty()) {
                    value_widget->setToolTip(QString::fromStdString(sub_desc.tooltip));
                }
            }

            row.variant_sub_rows[i].push_back(std::move(sub_row));
        }

        stack->addWidget(page);
    }
    v_layout->addWidget(stack);
    row.variant_stack = stack;

    // Set initial size policies: only the first (default) page should contribute
    // to the stacked widget's size hint. All others are set to Ignored so the
    // widget doesn't reserve space for the tallest alternative.
    for (int i = 0; i < stack->count(); ++i) {
        QWidget * page_widget = stack->widget(i);
        QSizePolicy sp = page_widget->sizePolicy();
        sp.setVerticalPolicy(i == 0 ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        page_widget->setSizePolicy(sp);
    }

    // Connect combo box to switch stacked widget pages
    connect(combo, &QComboBox::currentIndexChanged, stack, &QStackedWidget::setCurrentIndex);
    connect(combo, &QComboBox::currentIndexChanged, this, &AutoParamWidget::onFieldChanged);

    // When the active page changes, update size policies so the stack
    // shrinks/grows to fit only the newly visible page.
    connect(combo, &QComboBox::currentIndexChanged, this, [stack, this](int new_index) {
        for (int i = 0; i < stack->count(); ++i) {
            QWidget * page_widget = stack->widget(i);
            QSizePolicy sp = page_widget->sizePolicy();
            sp.setVerticalPolicy(i == new_index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
            page_widget->setSizePolicy(sp);
        }
        stack->adjustSize();
        adjustSize();
    });

    // Set default if available: parse the discriminator from the default JSON
    if (desc.default_value_json.has_value()) {
        auto def_result = rfl::json::read<rfl::Generic>(desc.default_value_json.value());
        if (def_result) {
            auto const * obj = std::get_if<rfl::Generic::Object>(&def_result.value().get());
            if (obj) {
                auto disc_val = obj->get(desc.variant_discriminator);
                if (disc_val) {
                    if (auto const * s = std::get_if<std::string>(&disc_val.value().get())) {
                        int const idx = combo->findText(QString::fromStdString(*s));
                        if (idx >= 0) {
                            combo->setCurrentIndex(idx);
                        }
                    }
                }
            }
        }
    }

    QString const label = QString::fromStdString(desc.display_name);
    layout->addRow(label, container);

    _field_rows.push_back(std::move(row));
}

// ============================================================================
// variantSubRowsToJson — Serialize the active variant alternative to JSON
// ============================================================================

std::string AutoParamWidget::variantSubRowsToJson(FieldRow const & row) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"" << row.variant_discriminator << "\":\""
        << row.variant_combo->currentText().toStdString() << "\"";

    int const active_idx = row.variant_combo->currentIndex();
    if (active_idx >= 0 && active_idx < static_cast<int>(row.variant_sub_rows.size())) {
        for (auto const & sub: row.variant_sub_rows[static_cast<size_t>(active_idx)]) {
            oss << ",\"" << sub.field_name << "\":";
            if (sub.double_spin) {
                oss << sub.double_spin->value();
            } else if (sub.int_spin) {
                oss << sub.int_spin->value();
            } else if (sub.bool_check) {
                oss << (sub.bool_check->isChecked() ? "true" : "false");
            } else if (sub.combo_box) {
                oss << "\"" << sub.combo_box->currentText().toStdString() << "\"";
            } else if (sub.line_edit) {
                oss << "\"" << sub.line_edit->text().toStdString() << "\"";
            }
        }
    }
    oss << "}";
    return oss.str();
}

// ============================================================================
// variantSubRowsFromJson — Populate variant widgets from a JSON object string
// ============================================================================

void AutoParamWidget::variantSubRowsFromJson(FieldRow & row, std::string const & json_obj_str) {
    auto result = rfl::json::read<rfl::Generic>(json_obj_str);
    if (!result) return;

    auto const * obj = std::get_if<rfl::Generic::Object>(&result.value().get());
    if (!obj) return;

    // Set the discriminator combo box
    auto disc_val = obj->get(row.variant_discriminator);
    if (disc_val) {
        if (auto const * s = std::get_if<std::string>(&disc_val.value().get())) {
            int const idx = row.variant_combo->findText(QString::fromStdString(*s));
            if (idx >= 0) {
                row.variant_combo->setCurrentIndex(idx);
            }
        }
    }

    // Populate sub-row widgets from the JSON fields
    int const active_idx = row.variant_combo->currentIndex();
    if (active_idx < 0 || active_idx >= static_cast<int>(row.variant_sub_rows.size())) return;

    for (auto & sub: row.variant_sub_rows[static_cast<size_t>(active_idx)]) {
        auto field_result = obj->get(sub.field_name);
        if (!field_result) continue;

        auto const & value = field_result.value().get();
        if (sub.double_spin) {
            if (auto const * num = std::get_if<double>(&value)) {
                sub.double_spin->setValue(*num);
            } else if (auto const * inum = std::get_if<int>(&value)) {
                sub.double_spin->setValue(static_cast<double>(*inum));
            }
        } else if (sub.int_spin) {
            if (auto const * num = std::get_if<int>(&value)) {
                sub.int_spin->setValue(*num);
            } else if (auto const * dnum = std::get_if<double>(&value)) {
                sub.int_spin->setValue(static_cast<int>(*dnum));
            }
        } else if (sub.bool_check) {
            if (auto const * b = std::get_if<bool>(&value)) {
                sub.bool_check->setChecked(*b);
            }
        } else if (sub.combo_box) {
            if (auto const * s = std::get_if<std::string>(&value)) {
                int const idx = sub.combo_box->findText(QString::fromStdString(*s));
                if (idx >= 0) sub.combo_box->setCurrentIndex(idx);
            }
        } else if (sub.line_edit) {
            if (auto const * s = std::get_if<std::string>(&value)) {
                sub.line_edit->setText(QString::fromStdString(*s));
            }
        }
    }
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

        if (row.is_variant && row.variant_combo) {
            oss << variantSubRowsToJson(row);
        } else if (row.double_spin) {
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

        if (row.is_variant && row.variant_combo) {
            // Variant fields: the value is a nested JSON object
            auto variant_json = rfl::json::write(field_result.value());
            variantSubRowsFromJson(row, variant_json);
        } else if (row.double_spin) {
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

    bool const was_external = !_updating;
    _updating = false;
    if (was_external) {
        emit parametersChanged();
    }
    return true;
}

// ============================================================================
// clear — Remove all field rows
// ============================================================================

void AutoParamWidget::clear() {
    clearLayout();
}

void AutoParamWidget::setPostEditHook(PostEditHook hook) {
    _post_edit_hook = std::move(hook);
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
        if (_post_edit_hook) {
            auto const current = toJson();
            auto const modified = _post_edit_hook(current);
            if (modified != current) {
                // Re-populate without re-triggering the hook
                _updating = true;
                fromJson(modified);
                _updating = false;
            }
        }
        emit parametersChanged();
    }
}
