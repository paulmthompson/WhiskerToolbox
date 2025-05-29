#include "ContrastWidget.hpp"
#include "ui_ContrastWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>

ContrastWidget::ContrastWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ContrastWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &ContrastWidget::_onActiveChanged);
    connect(ui->alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ContrastWidget::_onAlphaChanged);
    connect(ui->beta_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ContrastWidget::_onBetaChanged);
}

ContrastWidget::~ContrastWidget() {
    delete ui;
}

ContrastOptions ContrastWidget::getOptions() const {
    ContrastOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.alpha = ui->alpha_spinbox->value();
    options.beta = ui->beta_spinbox->value();
    return options;
}

void ContrastWidget::setOptions(ContrastOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void ContrastWidget::_onActiveChanged() {
    _updateOptions();
}

void ContrastWidget::_onAlphaChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ContrastWidget::_onBetaChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ContrastWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void ContrastWidget::_blockSignalsAndSetValues(ContrastOptions const& options) {
    // Block signals to prevent recursive updates
    ui->active_checkbox->blockSignals(true);
    ui->alpha_spinbox->blockSignals(true);
    ui->beta_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->alpha_spinbox->setValue(options.alpha);
    ui->beta_spinbox->setValue(options.beta);

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->alpha_spinbox->blockSignals(false);
    ui->beta_spinbox->blockSignals(false);
} 