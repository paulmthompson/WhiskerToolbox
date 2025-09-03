#include "MaskConnectedComponent_Widget.hpp"
#include "ui_MaskConnectedComponent_Widget.h"

#include "DataManager/transforms/Masks/Mask_Connected_Component/mask_connected_component.hpp"

MaskConnectedComponent_Widget::MaskConnectedComponent_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::MaskConnectedComponent_Widget)
{
    ui->setupUi(this);
    
    // Set default threshold value
    ui->thresholdSpinBox->setValue(10);
    ui->thresholdSpinBox->setMinimum(1);
    ui->thresholdSpinBox->setMaximum(10000);
}

MaskConnectedComponent_Widget::~MaskConnectedComponent_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskConnectedComponent_Widget::getParameters() const
{
    auto params = std::make_unique<MaskConnectedComponentParameters>();
    params->threshold = ui->thresholdSpinBox->value();
    return params;
} 