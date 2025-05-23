#include "LineResample_Widget.hpp"
#include "ui_LineResample_Widget.h" // Generated from .ui file

#include "DataManager/transforms/Lines/line_resample.hpp"

LineResample_Widget::LineResample_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::LineResample_Widget)
{
    ui->setupUi(this);
    // Default value is set in the .ui file and LineResampleParameters struct
    // ui->targetSpacingSpinBox->setValue(5.0); // Matches LineResampleParameters default
}

LineResample_Widget::~LineResample_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineResample_Widget::getParameters() const
{
    auto params = std::make_unique<LineResampleParameters>();
    params->target_spacing = static_cast<float>(ui->targetSpacingSpinBox->value());
    return params;
} 
