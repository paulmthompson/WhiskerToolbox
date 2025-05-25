#include "MaskSkeletonize_Widget.hpp"
#include "ui_MaskSkeletonize_Widget.h"

MaskSkeletonize_Widget::MaskSkeletonize_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::MaskSkeletonize_Widget)
{
    ui->setupUi(this);
    
    // No additional setup needed since skeletonization requires no parameters
}

MaskSkeletonize_Widget::~MaskSkeletonize_Widget()
{
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskSkeletonize_Widget::getParameters() const
{
    // Return default parameters since skeletonization doesn't require user input
    return std::make_unique<MaskSkeletonizeParameters>();
} 