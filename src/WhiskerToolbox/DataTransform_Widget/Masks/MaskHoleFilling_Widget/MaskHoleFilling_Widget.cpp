#include "MaskHoleFilling_Widget.hpp"
#include "ui_MaskHoleFilling_Widget.h"

MaskHoleFilling_Widget::MaskHoleFilling_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::MaskHoleFilling_Widget)
{
    ui->setupUi(this);
}

MaskHoleFilling_Widget::~MaskHoleFilling_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskHoleFilling_Widget::getParameters() const
{
    // Hole filling has no user-configurable parameters
    return std::make_unique<MaskHoleFillingParameters>();
} 