#include "MaskDilationWidget.hpp"
#include "ui_MaskDilationWidget.h"

#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>

MaskDilationWidget::MaskDilationWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MaskDilationWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->preview_checkbox, &QCheckBox::toggled,
            this, &MaskDilationWidget::_onPreviewChanged);
    connect(ui->grow_radio, &QRadioButton::toggled,
            this, &MaskDilationWidget::_onModeChanged);
    connect(ui->shrink_radio, &QRadioButton::toggled,
            this, &MaskDilationWidget::_onModeChanged);
    connect(ui->size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MaskDilationWidget::_onSizeChanged);
    connect(ui->apply_button, &QPushButton::clicked,
            this, &MaskDilationWidget::_onApplyClicked);
}

MaskDilationWidget::~MaskDilationWidget() {
    delete ui;
}

MaskDilationOptions MaskDilationWidget::getOptions() const {
    MaskDilationOptions options;
    options.preview = ui->preview_checkbox->isChecked();
    options.active = options.preview; // Active when preview is enabled
    options.is_grow_mode = ui->grow_radio->isChecked();
    
    // Use the same size value for both grow and shrink
    int size = ui->size_spinbox->value();
    options.grow_size = size;
    options.shrink_size = size;
    
    return options;
}

void MaskDilationWidget::setOptions(MaskDilationOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void MaskDilationWidget::_onPreviewChanged() {
    _updateOptions();
}

void MaskDilationWidget::_onSizeChanged() {
    // Auto-enable preview when user changes values
    if (!ui->preview_checkbox->isChecked()) {
        ui->preview_checkbox->blockSignals(true);
        ui->preview_checkbox->setChecked(true);
        ui->preview_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void MaskDilationWidget::_onModeChanged() {
    // Auto-enable preview when user changes mode
    if (!ui->preview_checkbox->isChecked()) {
        ui->preview_checkbox->blockSignals(true);
        ui->preview_checkbox->setChecked(true);
        ui->preview_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void MaskDilationWidget::_onApplyClicked() {
    emit applyRequested();
}

void MaskDilationWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void MaskDilationWidget::_blockSignalsAndSetValues(MaskDilationOptions const& options) {
    // Block signals to prevent recursive updates
    ui->preview_checkbox->blockSignals(true);
    ui->grow_radio->blockSignals(true);
    ui->shrink_radio->blockSignals(true);
    ui->size_spinbox->blockSignals(true);

    // Set values
    ui->preview_checkbox->setChecked(options.preview);
    ui->grow_radio->setChecked(options.is_grow_mode);
    ui->shrink_radio->setChecked(!options.is_grow_mode);
    
    // Use grow_size as the default, but if it's 1 and shrink_size is different, use shrink_size
    int size_to_use = options.is_grow_mode ? options.grow_size : options.shrink_size;
    ui->size_spinbox->setValue(size_to_use);

    // Unblock signals
    ui->preview_checkbox->blockSignals(false);
    ui->grow_radio->blockSignals(false);
    ui->shrink_radio->blockSignals(false);
    ui->size_spinbox->blockSignals(false);
} 