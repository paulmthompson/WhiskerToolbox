#include "GammaWidget.hpp"
#include "ui_GammaWidget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>

GammaWidget::GammaWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::GammaWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &GammaWidget::_onActiveChanged);
    connect(ui->gamma_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &GammaWidget::_onGammaChanged);
}

GammaWidget::~GammaWidget() {
    delete ui;
}

GammaOptions GammaWidget::getOptions() const {
    GammaOptions options;
    options.gamma = ui->gamma_spinbox->value();
    return options;
}

bool GammaWidget::isActive() const {
    return ui->active_checkbox->isChecked();
}

void GammaWidget::setActive(bool active) {
    ui->active_checkbox->blockSignals(true);
    ui->active_checkbox->setChecked(active);
    ui->active_checkbox->blockSignals(false);
}

void GammaWidget::setOptions(GammaOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void GammaWidget::_onActiveChanged() {
    emit activeChanged(ui->active_checkbox->isChecked());
    _updateOptions();
}

void GammaWidget::_onGammaChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void GammaWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void GammaWidget::_blockSignalsAndSetValues(GammaOptions const& options) {
    ui->gamma_spinbox->blockSignals(true);
    ui->gamma_spinbox->setValue(options.gamma);
    ui->gamma_spinbox->blockSignals(false);
} 