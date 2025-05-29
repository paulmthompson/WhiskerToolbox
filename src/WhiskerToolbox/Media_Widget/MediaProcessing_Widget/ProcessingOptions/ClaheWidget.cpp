#include "ClaheWidget.hpp"
#include "ui_ClaheWidget.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>

ClaheWidget::ClaheWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::ClaheWidget) {
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &ClaheWidget::_onActiveChanged);
    connect(ui->clip_limit_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ClaheWidget::_onClipLimitChanged);
    connect(ui->grid_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ClaheWidget::_onGridSizeChanged);
}

ClaheWidget::~ClaheWidget() {
    delete ui;
}

ClaheOptions ClaheWidget::getOptions() const {
    ClaheOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.clip_limit = ui->clip_limit_spinbox->value();
    options.grid_size = ui->grid_size_spinbox->value();
    return options;
}

void ClaheWidget::setOptions(ClaheOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void ClaheWidget::_onActiveChanged() {
    _updateOptions();
}

void ClaheWidget::_onClipLimitChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ClaheWidget::_onGridSizeChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void ClaheWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void ClaheWidget::_blockSignalsAndSetValues(ClaheOptions const& options) {
    // Block signals to prevent triggering optionsChanged during programmatic updates
    ui->active_checkbox->blockSignals(true);
    ui->clip_limit_spinbox->blockSignals(true);
    ui->grid_size_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->clip_limit_spinbox->setValue(options.clip_limit);
    ui->grid_size_spinbox->setValue(options.grid_size);

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->clip_limit_spinbox->blockSignals(false);
    ui->grid_size_spinbox->blockSignals(false);

    // Emit signal after all values are set
    emit optionsChanged(getOptions());
} 