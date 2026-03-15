#include "SharpenWidget.hpp"
#include "ui_SharpenWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>

SharpenWidget::SharpenWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::SharpenWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &SharpenWidget::_onActiveChanged);
    connect(ui->sigma_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SharpenWidget::_onSigmaChanged);
}

SharpenWidget::~SharpenWidget() {
    delete ui;
}

SharpenOptions SharpenWidget::getOptions() const {
    SharpenOptions options;
    options.sigma = ui->sigma_spinbox->value();
    return options;
}

bool SharpenWidget::isActive() const {
    return ui->active_checkbox->isChecked();
}

void SharpenWidget::setActive(bool active) {
    ui->active_checkbox->blockSignals(true);
    ui->active_checkbox->setChecked(active);
    ui->active_checkbox->blockSignals(false);
}

void SharpenWidget::setOptions(SharpenOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void SharpenWidget::_onActiveChanged() {
    emit activeChanged(ui->active_checkbox->isChecked());
    _updateOptions();
}

void SharpenWidget::_onSigmaChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void SharpenWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void SharpenWidget::_blockSignalsAndSetValues(SharpenOptions const& options) {
    // Block signals to prevent recursive updates
    ui->sigma_spinbox->blockSignals(true);

    // Set values
    ui->sigma_spinbox->setValue(options.sigma);

    // Unblock signals
    ui->sigma_spinbox->blockSignals(false);
} 