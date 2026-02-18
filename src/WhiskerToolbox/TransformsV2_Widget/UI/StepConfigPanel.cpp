#include "StepConfigPanel.hpp"
#include "AutoParamWidget.hpp"
#include "ParamWidgetRegistry.hpp"

#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/ParameterSchema.hpp"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace WhiskerToolbox::Transforms::V2;

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
                                      std::string const & params_json) {
    clearCurrentWidget();

    _current_transform_name = transform_name;

    // Look up metadata for display
    auto const * meta = ElementRegistry::instance().getMetadata(transform_name);
    if (meta) {
        _header_label->setText(QString::fromStdString(meta->name));
        _header_label->setVisible(true);

        if (!meta->description.empty()) {
            _description_label->setText(QString::fromStdString(meta->description));
            _description_label->setVisible(true);
        } else {
            _description_label->setVisible(false);
        }
    } else {
        _header_label->setText(QString::fromStdString(transform_name));
        _header_label->setVisible(true);
        _description_label->setVisible(false);
    }

    // Check for custom widget override first
    if (meta && meta->params_type != typeid(void) && meta->params_type != typeid(NoParams)) {
        auto & widget_registry = ParamWidgetRegistry::instance();
        if (widget_registry.hasCustomWidget(meta->params_type)) {
            setupCustomWidget(transform_name, params_json);
            return;
        }
    }

    // Fall back to auto-generated parameter widget
    setupAutoParamWidget(transform_name, params_json);
}

void StepConfigPanel::clearConfig() {
    clearCurrentWidget();
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

void StepConfigPanel::setupAutoParamWidget(std::string const & transform_name,
                                            std::string const & params_json) {
    auto const * schema = ElementRegistry::instance().getParameterSchema(transform_name);
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
    auto const * meta = ElementRegistry::instance().getMetadata(transform_name);
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
    while (_scroll_layout->count() > 1) { // Keep the stretch
        auto * item = _scroll_layout->takeAt(0);
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
