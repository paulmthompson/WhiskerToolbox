#include "MaskDrawSelectionWidget.hpp"
#include "ui_MaskDrawSelectionWidget.h"

namespace mask_widget {

MaskDrawSelectionWidget::MaskDrawSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MaskDrawSelectionWidget) {
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
}

MaskDrawSelectionWidget::~MaskDrawSelectionWidget() {
    delete ui;
}

int MaskDrawSelectionWidget::getBrushSize() const {
    return _brushSize;
}

void MaskDrawSelectionWidget::setBrushSize(int size) {
    if (_brushSize != size) {
        _brushSize = size;
        ui->brushSizeSlider->setValue(size);
        // No need to emit signal here as the slider valueChanged will do that
    }
}

bool MaskDrawSelectionWidget::isHoverCircleVisible() const {
    return _hoverCircleVisible;
}

void MaskDrawSelectionWidget::setHoverCircleVisible(bool visible) {
    if (_hoverCircleVisible != visible) {
        _hoverCircleVisible = visible;
        ui->showCircleCheckbox->setChecked(visible);
        // No need to emit signal here as the checkbox toggled will do that
    }
}

} // namespace mask_widget 