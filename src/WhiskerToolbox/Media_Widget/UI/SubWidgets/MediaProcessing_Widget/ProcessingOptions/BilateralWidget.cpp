#include "BilateralWidget.hpp"
#include "ui_BilateralWidget.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>

BilateralWidget::BilateralWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::BilateralWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &BilateralWidget::_onActiveChanged);
    connect(ui->d_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BilateralWidget::_onDChanged);
    connect(ui->sigma_color_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BilateralWidget::_onSigmaColorChanged);
    connect(ui->sigma_space_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BilateralWidget::_onSigmaSpaceChanged);
}

BilateralWidget::~BilateralWidget() {
    delete ui;
}

BilateralOptions BilateralWidget::getOptions() const {
    BilateralOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.diameter = ui->d_spinbox->value();
    options.sigma_color = ui->sigma_color_spinbox->value();
    options.sigma_spatial = ui->sigma_space_spinbox->value();
    return options;
}

void BilateralWidget::setOptions(BilateralOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void BilateralWidget::_onActiveChanged() {
    _updateOptions();
}

void BilateralWidget::_onDChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void BilateralWidget::_onSigmaColorChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void BilateralWidget::_onSigmaSpaceChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void BilateralWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void BilateralWidget::_blockSignalsAndSetValues(BilateralOptions const& options) {
    // Block signals to prevent recursive updates
    ui->active_checkbox->blockSignals(true);
    ui->d_spinbox->blockSignals(true);
    ui->sigma_color_spinbox->blockSignals(true);
    ui->sigma_space_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->d_spinbox->setValue(options.diameter);
    ui->sigma_color_spinbox->setValue(options.sigma_color);
    ui->sigma_space_spinbox->setValue(options.sigma_spatial);

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->d_spinbox->blockSignals(false);
    ui->sigma_color_spinbox->blockSignals(false);
    ui->sigma_space_spinbox->blockSignals(false);
} 
