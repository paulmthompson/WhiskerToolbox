#include "StepConfigPanel.hpp"
#include "AutoParamWidget/AutoParamWidget.hpp"
#include "ParamWidgetRegistry.hpp"

#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"

#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

// ============================================================================
// Construction / Destruction
// ============================================================================

StepConfigPanel::StepConfigPanel(QWidget * parent)
    : QWidget(parent) {

    _main_layout = new QVBoxLayout(this);
    _main_layout->setContentsMargins(0, 0, 0, 0);
    _main_layout->setSpacing(4);

    // Header label (transform name)
    _header_label = new QLabel(this);
    _header_label->setStyleSheet("font-weight: bold; font-size: 11pt;");
    _header_label->setVisible(false);
    _main_layout->addWidget(_header_label);

    // Description label
    _description_label = new QLabel(this);
    _description_label->setWordWrap(true);
    _description_label->setStyleSheet("color: gray; font-size: 9pt;");
    _description_label->setVisible(false);
    _main_layout->addWidget(_description_label);

    // Multi-input section (second DataManager key)
    _multi_input_group = new QWidget(this);
    auto * multi_layout = new QVBoxLayout(_multi_input_group);
    multi_layout->setContentsMargins(0, 0, 0, 0);
    multi_layout->setSpacing(2);

    _primary_input_label = new QLabel(_multi_input_group);
    _primary_input_label->setWordWrap(true);
    _primary_input_label->setStyleSheet("font-size: 9pt;");
    multi_layout->addWidget(_primary_input_label);

    _secondary_input_label = new QLabel(tr("Second input:"), _multi_input_group);
    _secondary_input_label->setStyleSheet("font-size: 9pt;");
    multi_layout->addWidget(_secondary_input_label);

    _secondary_input_combo = new QComboBox(_multi_input_group);
    _secondary_input_combo->setToolTip(
            tr("Second input is not saved in pipeline JSON. Re-select after loading a saved pipeline."));
    connect(_secondary_input_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index <= 0) {
                    emit additionalInputChanged(std::string{});
                    return;
                }
                emit additionalInputChanged(_secondary_input_combo->currentText().toStdString());
            });
    multi_layout->addWidget(_secondary_input_combo);

    _multi_input_group->setVisible(false);
    _main_layout->addWidget(_multi_input_group);

    // Scrollable area for the parameter widget
    _scroll_area = new QScrollArea(this);
    _scroll_area->setWidgetResizable(true);
    _scroll_area->setFrameShape(QFrame::NoFrame);
    _main_layout->addWidget(_scroll_area, 1);

    _scroll_content = new QWidget();
    _scroll_layout = new QVBoxLayout(_scroll_content);
    _scroll_layout->setContentsMargins(0, 0, 0, 0);
    _scroll_layout->addStretch();
    _scroll_area->setWidget(_scroll_content);
}

StepConfigPanel::~StepConfigPanel() = default;

// ============================================================================
// Public API
// ============================================================================

void StepConfigPanel::showStepConfig(std::string const & transform_name,
                                     std::string const & params_json,
                                     MultiInputStepContext const & multi_input) {
    clearCurrentWidget();
    clearMultiInputSection();

    _current_transform_name = transform_name;

    // Look up metadata for display — try element first, then container
    auto const * meta = Neuralyzer::Transforms::V2::ElementRegistry::instance().getMetadata(transform_name);
    std::string display_name = transform_name;
    std::string description;
    std::type_index params_type = typeid(void);

    if (meta) {
        display_name = meta->name;
        description = meta->description;
        params_type = meta->params_type;
    } else {
        auto const * cmeta =
                Neuralyzer::Transforms::V2::ElementRegistry::instance().getContainerMetadata(transform_name);
        if (cmeta) {
            display_name = cmeta->name;
            description = cmeta->description;
            params_type = cmeta->params_type;
        }
    }

    _header_label->setText(QString::fromStdString(display_name));
    _header_label->setVisible(true);

    if (!description.empty()) {
        _description_label->setText(QString::fromStdString(description));
        _description_label->setVisible(true);
    } else {
        _description_label->setVisible(false);
    }

    if (multi_input.enabled) {
        setupMultiInputSection(multi_input);
    }

    // Check for custom widget override first
    if (params_type != typeid(void) && params_type != typeid(Neuralyzer::Transforms::V2::NoParams)) {
        auto & widget_registry = ParamWidgetRegistry::instance();
        if (widget_registry.hasCustomWidget(params_type)) {
            setupCustomWidget(transform_name, params_json);
            return;
        }
    }

    // Fall back to auto-generated parameter widget
    setupAutoParamWidget(transform_name, params_json);
}

void StepConfigPanel::clearConfig() {
    clearCurrentWidget();
    clearMultiInputSection();
    _current_transform_name.clear();
    _header_label->setVisible(false);
    _description_label->setVisible(false);
}

std::string StepConfigPanel::currentParamsJson() const {
    if (_auto_param_widget) {
        return _auto_param_widget->toJson();
    }
    // Custom widgets would need their own toJson mechanism
    return "{}";
}

// ============================================================================
// Private Helpers
// ============================================================================

void StepConfigPanel::setupMultiInputSection(MultiInputStepContext const & multi_input) {
    _multi_input_group->setVisible(true);

    QString const primary_text =
            tr("Primary input: %1 (%2)")
                    .arg(QString::fromStdString(multi_input.primary_input_key))
                    .arg(QString::fromStdString(multi_input.primary_input_type_name));
    _primary_input_label->setText(primary_text);

    _secondary_input_label->setText(
            tr("Second input (%1):").arg(QString::fromStdString(multi_input.secondary_input_type_name)));

    _secondary_input_combo->blockSignals(true);
    _secondary_input_combo->clear();
    _secondary_input_combo->addItem(tr("(Select a data key)"), QString());

    for (auto const & key: multi_input.available_secondary_keys) {
        _secondary_input_combo->addItem(QString::fromStdString(key));
    }

    if (multi_input.additional_input_key.has_value() &&
        !multi_input.additional_input_key->empty()) {
        int const idx = _secondary_input_combo->findText(
                QString::fromStdString(*multi_input.additional_input_key));
        if (idx >= 0) {
            _secondary_input_combo->setCurrentIndex(idx);
        }
    } else {
        _secondary_input_combo->setCurrentIndex(0);
    }

    _secondary_input_combo->setEnabled(!multi_input.available_secondary_keys.empty());
    _secondary_input_combo->blockSignals(false);
}

void StepConfigPanel::clearMultiInputSection() {
    if (_multi_input_group) {
        _multi_input_group->setVisible(false);
    }
    if (_primary_input_label) {
        _primary_input_label->clear();
    }
    if (_secondary_input_combo) {
        _secondary_input_combo->blockSignals(true);
        _secondary_input_combo->clear();
        _secondary_input_combo->blockSignals(false);
    }
}

void StepConfigPanel::setupAutoParamWidget(std::string const & transform_name,
                                           std::string const & params_json) {
    auto const * schema =
            Neuralyzer::Transforms::V2::ElementRegistry::instance().getParameterSchema(transform_name);
    if (!schema || schema->fields.empty()) {
        // Transform has no parameters (NoParams)
        auto * label = new QLabel(tr("This transform has no configurable parameters."), _scroll_content);
        label->setStyleSheet("color: gray; font-style: italic;");
        label->setAlignment(Qt::AlignCenter);
        _scroll_layout->insertWidget(0, label);
        return;
    }

    _auto_param_widget = new AutoParamWidget(_scroll_content);
    _auto_param_widget->setSchema(*schema);

    // Populate from existing JSON
    if (!params_json.empty() && params_json != "{}") {
        _auto_param_widget->fromJson(params_json);
    }

    // Connect parameter changes
    connect(_auto_param_widget, &AutoParamWidget::parametersChanged,
            this, [this]() {
                if (_auto_param_widget) {
                    emit parametersChanged(_auto_param_widget->toJson());
                }
            });

    _scroll_layout->insertWidget(0, _auto_param_widget);
}

void StepConfigPanel::setupCustomWidget(std::string const & transform_name,
                                        std::string const & /* params_json */) {
    auto const * meta = Neuralyzer::Transforms::V2::ElementRegistry::instance().getMetadata(transform_name);
    if (!meta) {
        return;
    }

    _custom_widget = ParamWidgetRegistry::instance().createCustomWidget(
            meta->params_type, _scroll_content);

    if (_custom_widget) {
        _scroll_layout->insertWidget(0, _custom_widget);
        // Note: Custom widgets need their own signal connection mechanism.
        // This will be enhanced in Phase 4 (embeddable builder).
    }
}

void StepConfigPanel::clearCurrentWidget() {
    if (_auto_param_widget) {
        _scroll_layout->removeWidget(_auto_param_widget);
        _auto_param_widget->deleteLater();
        _auto_param_widget = nullptr;
    }
    if (_custom_widget) {
        _scroll_layout->removeWidget(_custom_widget);
        _custom_widget->deleteLater();
        _custom_widget = nullptr;
    }

    // Also remove any "no parameters" labels
    while (_scroll_layout->count() > 1) {// Keep the stretch
        auto * item = _scroll_layout->takeAt(0);
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
