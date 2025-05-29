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
    options.active = ui->active_checkbox->isChecked();
    options.sigma = ui->sigma_spinbox->value();
    return options;
}

void SharpenWidget::setOptions(SharpenOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void SharpenWidget::_onActiveChanged() {
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
    ui->active_checkbox->blockSignals(true);
    ui->sigma_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->sigma_spinbox->setValue(options.sigma);

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->sigma_spinbox->blockSignals(false);
} 