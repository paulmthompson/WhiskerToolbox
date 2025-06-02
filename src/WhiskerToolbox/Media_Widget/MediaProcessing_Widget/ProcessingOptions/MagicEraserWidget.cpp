#include "MagicEraserWidget.hpp"
#include "ui_MagicEraserWidget.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

MagicEraserWidget::MagicEraserWidget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::MagicEraserWidget) {
    
    ui->setupUi(this);

    // Connect UI controls to slots
    connect(ui->active_checkbox, &QCheckBox::toggled,
            this, &MagicEraserWidget::_onActiveChanged);
    connect(ui->brush_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MagicEraserWidget::_onBrushSizeChanged);
    connect(ui->filter_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MagicEraserWidget::_onMedianFilterSizeChanged);
    connect(ui->drawing_mode_button, &QPushButton::toggled,
            this, &MagicEraserWidget::_onDrawingModeChanged);
    connect(ui->clear_mask_button, &QPushButton::clicked,
            this, &MagicEraserWidget::_onClearMaskClicked);
}

MagicEraserWidget::~MagicEraserWidget() {
    delete ui;
}

MagicEraserOptions MagicEraserWidget::getOptions() const {
    MagicEraserOptions options;
    options.active = ui->active_checkbox->isChecked();
    options.brush_size = ui->brush_size_spinbox->value();
    options.median_filter_size = ui->filter_size_spinbox->value();
    options.drawing_mode = ui->drawing_mode_button->isChecked();
    return options;
}

void MagicEraserWidget::setOptions(MagicEraserOptions const& options) {
    _blockSignalsAndSetValues(options);
}

void MagicEraserWidget::_onActiveChanged() {
    _updateOptions();
}

void MagicEraserWidget::_onBrushSizeChanged() {
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void MagicEraserWidget::_onMedianFilterSizeChanged() {
    // Ensure filter size is odd
    int value = ui->filter_size_spinbox->value();
    if (value % 2 == 0) {
        ui->filter_size_spinbox->blockSignals(true);
        ui->filter_size_spinbox->setValue(value + 1);
        ui->filter_size_spinbox->blockSignals(false);
    }
    
    // Auto-enable when user changes values
    if (!ui->active_checkbox->isChecked()) {
        ui->active_checkbox->blockSignals(true);
        ui->active_checkbox->setChecked(true);
        ui->active_checkbox->blockSignals(false);
    }
    _updateOptions();
}

void MagicEraserWidget::_onDrawingModeChanged() {
    bool drawing_mode = ui->drawing_mode_button->isChecked();
    
    // Update button text
    ui->drawing_mode_button->setText(drawing_mode ? "Stop Drawing" : "Start Drawing");
    
    // Emit the drawing mode change signal
    emit drawingModeChanged(drawing_mode);
    
    _updateOptions();
}

void MagicEraserWidget::_onClearMaskClicked() {
    // Emit signal to request mask clearing
    emit clearMaskRequested();
    
    std::cout << "Magic eraser mask clear requested" << std::endl;
}

void MagicEraserWidget::_updateOptions() {
    emit optionsChanged(getOptions());
}

void MagicEraserWidget::_blockSignalsAndSetValues(MagicEraserOptions const& options) {
    // Block signals to prevent recursive updates
    ui->active_checkbox->blockSignals(true);
    ui->brush_size_spinbox->blockSignals(true);
    ui->filter_size_spinbox->blockSignals(true);
    ui->drawing_mode_button->blockSignals(true);

    // Set values
    ui->active_checkbox->setChecked(options.active);
    ui->brush_size_spinbox->setValue(options.brush_size);
    ui->filter_size_spinbox->setValue(options.median_filter_size);
    ui->drawing_mode_button->setChecked(options.drawing_mode);
    
    // Update button text based on drawing mode
    ui->drawing_mode_button->setText(options.drawing_mode ? "Stop Drawing" : "Start Drawing");

    // Unblock signals
    ui->active_checkbox->blockSignals(false);
    ui->brush_size_spinbox->blockSignals(false);
    ui->filter_size_spinbox->blockSignals(false);
    ui->drawing_mode_button->blockSignals(false);
} 
