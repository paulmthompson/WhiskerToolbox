
#include "MaskArea_Widget.hpp"

#include "ui_MaskArea_Widget.h"

#include "DataManager/transforms/Masks/Mask_Area/mask_area.hpp"

MaskArea_Widget::MaskArea_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::MaskArea_Widget)
{
    ui->setupUi(this);


}

MaskArea_Widget::~MaskArea_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskArea_Widget::getParameters() const {
    auto params = std::make_unique<TransformParametersBase>();

    return params;
}
