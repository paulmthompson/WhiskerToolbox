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

    // Ensure odd-only stepping and minimum of 3
    ui->kernel_size_spinbox->setSingleStep(2);
    if (ui->kernel_size_spinbox->value() % 2 == 0)
        ui->kernel_size_spinbox->setValue(ui->kernel_size_spinbox->value() + 1);
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

void MedianWidget::setKernelConstraints(bool is_8bit_grayscale) {
    if (_is_8bit_grayscale == is_8bit_grayscale) {
        // Still enforce odd/clamp to current limits
        _enforceOddAndClamp();
        return;
    }

    _is_8bit_grayscale = is_8bit_grayscale;

    // Update tooltip to reflect constraint
    QString tip = "Size of the median filter kernel. Must be odd and >= 3.";
    if (!_is_8bit_grayscale) {
        tip += " For non-8-bit grayscale images, the maximum allowed size is 5 (OpenCV).";
    } else {
        tip += " Larger values produce stronger smoothing.";
    }
    ui->kernel_size_spinbox->setToolTip(tip);

    // Adjust max according to constraint
    // Allow up to 21 for 8-bit grayscale as per current UI defaults
    int max_allowed = _is_8bit_grayscale ? 21 : 5;

    // Block signals during range and value adjustment
    ui->kernel_size_spinbox->blockSignals(true);
    ui->kernel_size_spinbox->setRange(3, max_allowed);
    // Ensure current value respects the new limits and odd requirement
    _enforceOddAndClamp();
    ui->kernel_size_spinbox->blockSignals(false);

    // Emit updated options after constraints changed
    emit optionsChanged(getOptions());
}

void MedianWidget::_onActiveChanged() {
    _updateOptions();
}

void MedianWidget::_onKernelSizeChanged() {
    // Enforce odd and clamped value when user edits
    _enforceOddAndClamp();

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

    // Enforce constraints after setting programmatically
    _enforceOddAndClamp();

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->kernel_size_spinbox->blockSignals(false);

    // Emit signal after all values are set
    emit optionsChanged(getOptions());
}

void MedianWidget::_enforceOddAndClamp() {
    int v = ui->kernel_size_spinbox->value();
    if (v < 3) v = 3;
    if ((v % 2) == 0) v += 1; // bump to next odd

    int maxv = ui->kernel_size_spinbox->maximum();
    if (v > maxv) {
        // Ensure we pick an odd value <= max
        v = (maxv % 2) ? maxv : (maxv - 1);
        if (v < 3) v = 3; // final guard
    }

    if (v != ui->kernel_size_spinbox->value()) {
        bool blocked = ui->kernel_size_spinbox->blockSignals(true);
        ui->kernel_size_spinbox->setValue(v);
        ui->kernel_size_spinbox->blockSignals(blocked);
    }
}
