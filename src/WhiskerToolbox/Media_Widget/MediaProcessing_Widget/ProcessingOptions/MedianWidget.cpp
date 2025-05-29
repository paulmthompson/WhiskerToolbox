#include "MedianWidget.hpp"
#include "ui_MedianWidget.h"

#include <QCheckBox>
#include <QSpinBox>

MedianWidget::MedianWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MedianWidget) {
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &MedianWidget::_onActiveChanged);
    connect(ui->kernel_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MedianWidget::_onKernelSizeChanged);
}

MedianWidget::~MedianWidget() {
    delete ui;
}

MedianOptions MedianWidget::getOptions() const {
    MedianOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.kernel_size = ui->kernel_size_spinbox->value();
    return options;
}

void MedianWidget::setOptions(MedianOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void MedianWidget::_onActiveChanged() {
    _updateOptions();
}

void MedianWidget::_onKernelSizeChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void MedianWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void MedianWidget::_blockSignalsAndSetValues(MedianOptions const& options) {
    // Block signals to prevent triggering optionsChanged during programmatic updates
    ui->active_checkbox->blockSignals(true);
    ui->kernel_size_spinbox->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->kernel_size_spinbox->setValue(options.kernel_size);

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->kernel_size_spinbox->blockSignals(false);

    // Emit signal after all values are set
    emit optionsChanged(getOptions());
} 