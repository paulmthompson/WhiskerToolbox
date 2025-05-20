#include "LineEraseSelectionWidget.hpp"
#include "ui_LineEraseSelectionWidget.h"

namespace line_widget {

LineEraseSelectionWidget::LineEraseSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineEraseSelectionWidget) {
    ui->setupUi(this);
    
    // Connect radius slider signal
    connect(ui->radiusSlider, &QSlider::valueChanged, this, [this](int value) {
        _eraserRadius = value;
        ui->radiusValueLabel->setText(QString::number(value));
        emit eraserRadiusChanged(value);
    });
    
    // Connect show circle checkbox
    connect(ui->showCircleCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        // Forward this signal to the parent widget through a lambda
        // We'll update MediaLine_Widget to handle this signal
        emit showCircleToggled(checked);
    });
}

LineEraseSelectionWidget::~LineEraseSelectionWidget() {
    delete ui;
}

int LineEraseSelectionWidget::getEraserRadius() const {
    return _eraserRadius;
}

void LineEraseSelectionWidget::setEraserRadius(int radius) {
    if (_eraserRadius != radius) {
        _eraserRadius = radius;
        ui->radiusSlider->setValue(radius);
        // No need to emit signal here as the slider valueChanged will do that
    }
}

} // namespace line_widget 