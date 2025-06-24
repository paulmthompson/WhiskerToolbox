#include "MaskBrushSelectionWidget.hpp"
#include "ui_MaskBrushSelectionWidget.h"

namespace mask_widget {

MaskBrushSelectionWidget::MaskBrushSelectionWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::MaskBrushSelectionWidget) {
    ui->setupUi(this);

    // Connect slider and spinbox to each other for two-way updates
    connect(ui->brushSizeSlider, &QSlider::valueChanged, ui->brushSizeSpinBox, &QSpinBox::setValue);
    connect(ui->brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), ui->brushSizeSlider, &QSlider::setValue);

    // Connect slider (or spinbox) to internal state and emit signal
    connect(ui->brushSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        _brushSize = value;
        emit brushSizeChanged(value);
    });

    // Connect show circle checkbox
    connect(ui->showCircleCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        _hoverCircleVisible = checked;
        emit hoverCircleVisibilityChanged(checked);
    });

    // Connect allow empty mask checkbox
    connect(ui->allowEmptyMaskCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        _allowEmptyMask = checked;
        emit allowEmptyMaskChanged(checked);
    });
}

MaskBrushSelectionWidget::~MaskBrushSelectionWidget() {
    delete ui;
}

int MaskBrushSelectionWidget::getBrushSize() const {
    return _brushSize;
}

void MaskBrushSelectionWidget::setBrushSize(int size) {
    if (_brushSize != size) {
        _brushSize = size;
        ui->brushSizeSlider->setValue(size);
        // No need to emit signal here as the slider valueChanged will do that
    }
}

bool MaskBrushSelectionWidget::isHoverCircleVisible() const {
    return _hoverCircleVisible;
}

void MaskBrushSelectionWidget::setHoverCircleVisible(bool visible) {
    if (_hoverCircleVisible != visible) {
        _hoverCircleVisible = visible;
        ui->showCircleCheckbox->setChecked(visible);
        // No need to emit signal here as the checkbox toggled will do that
    }
}

bool MaskBrushSelectionWidget::isAllowEmptyMask() const {
    return _allowEmptyMask;
}

void MaskBrushSelectionWidget::setAllowEmptyMask(bool allow) {
    if (_allowEmptyMask != allow) {
        _allowEmptyMask = allow;
        ui->allowEmptyMaskCheckbox->setChecked(allow);
        // No need to emit signal here as the checkbox toggled will do that
    }
}

}// namespace mask_widget