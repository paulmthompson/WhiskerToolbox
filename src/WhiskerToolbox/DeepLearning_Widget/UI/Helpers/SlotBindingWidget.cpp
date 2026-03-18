/// @file SlotBindingWidget.cpp
/// @brief Implementation of composite data-source + AutoParamWidget.

#include "SlotBindingWidget.hpp"

#include "DataSourceComboHelper.hpp"

#include "AutoParamWidget/AutoParamWidget.hpp"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

static constexpr auto NONE_TEXT = "(None)";

namespace dl::widget {

SlotBindingWidget::SlotBindingWidget(
        DataSourceComboHelper * combo_helper,
        QWidget * parent)
    : QWidget(parent),
      _combo_helper(combo_helper) {

    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // ── Data source combo section ──────────────────────────────────────────

    _source_label = new QLabel(tr("Data Source"), this);
    _source_combo = new QComboBox(this);
    _source_combo->addItem(tr(NONE_TEXT));

    layout->addWidget(_source_label);
    layout->addWidget(_source_combo);

    // Hide data source section if no helper provided
    if (!_combo_helper) {
        _source_label->setVisible(false);
        _source_combo->setVisible(false);
    }

    // ── AutoParamWidget ────────────────────────────────────────────────────

    _param_widget = new AutoParamWidget(this);
    layout->addWidget(_param_widget);

    layout->addStretch();

    // ── Signal wiring ──────────────────────────────────────────────────────

    connect(_source_combo, &QComboBox::currentTextChanged,
            this, [this](QString const & text) {
                std::string const key = (text == tr(NONE_TEXT))
                                                ? std::string{}
                                                : text.toStdString();
                emit dataSourceChanged(key);
                emit bindingChanged();
            });

    connect(_param_widget, &AutoParamWidget::parametersChanged,
            this, [this]() {
                emit parametersChanged();
                emit bindingChanged();
            });
}

SlotBindingWidget::~SlotBindingWidget() {
    if (_combo_helper && _source_combo) {
        _combo_helper->untrack(_source_combo);
    }
}

// ── Data Source Combo ──────────────────────────────────────────────────────

void SlotBindingWidget::setDataSourceLabel(QString const & label) {
    _source_label->setText(label);
}

void SlotBindingWidget::setDataSourceTypes(std::vector<DM_DataType> const & types) {
    if (!_combo_helper) return;

    _combo_helper->populateCombo(_source_combo, types);
    _combo_helper->track(_source_combo, types);
}

std::string SlotBindingWidget::selectedDataSource() const {
    QString const text = _source_combo->currentText();
    if (text == tr(NONE_TEXT) || text.isEmpty()) {
        return {};
    }
    return text.toStdString();
}

void SlotBindingWidget::setSelectedDataSource(std::string const & key) {
    if (key.empty()) {
        _source_combo->setCurrentIndex(0);// "(None)"
        return;
    }
    int const idx = _source_combo->findText(QString::fromStdString(key));
    if (idx >= 0) {
        _source_combo->setCurrentIndex(idx);
    } else {
        _source_combo->setCurrentIndex(0);
    }
}

void SlotBindingWidget::setDataSourceVisible(bool visible) {
    _source_label->setVisible(visible);
    _source_combo->setVisible(visible);
}

QComboBox * SlotBindingWidget::dataSourceCombo() const {
    return _source_combo;
}

// ── AutoParamWidget ───────────────────────────────────────────────────────

void SlotBindingWidget::setParameterSchema(
        WhiskerToolbox::Transforms::V2::ParameterSchema const & schema) {
    _param_widget->setSchema(schema);
}

std::string SlotBindingWidget::parametersJson() const {
    return _param_widget->toJson();
}

bool SlotBindingWidget::setParametersJson(std::string const & json) {
    return _param_widget->fromJson(json);
}

void SlotBindingWidget::clearParameters() {
    _param_widget->clear();
}

AutoParamWidget * SlotBindingWidget::paramWidget() const {
    return _param_widget;
}

void SlotBindingWidget::setPostEditHook(AutoParamWidget::PostEditHook hook) {
    _param_widget->setPostEditHook(std::move(hook));
}

}// namespace dl::widget
