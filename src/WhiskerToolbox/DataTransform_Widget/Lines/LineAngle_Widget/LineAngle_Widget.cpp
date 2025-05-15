#include "LineAngle_Widget.hpp"

#include "ui_LineAngle_Widget.h"

LineAngle_Widget::LineAngle_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::LineAngle_Widget)
{
    ui->setupUi(this);
    
    ui->positionSpinBox->setValue(20.0);
}

LineAngle_Widget::~LineAngle_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineAngle_Widget::getParameters() const {
    auto params = std::make_unique<LineAngleParameters>();
    
    // Convert percentage from UI (0-100) to decimal (0.0-1.0)
    params->position = static_cast<float>(ui->positionSpinBox->value()) / 100.0f;
    
    return params;
}
