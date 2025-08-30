#include "MaskPrincipalAxis_Widget.hpp"
#include "ui_MaskPrincipalAxis_Widget.h"

#include "DataManager/transforms/Masks/Mask_Principal_Axis/mask_principal_axis.hpp"

MaskPrincipalAxis_Widget::MaskPrincipalAxis_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::MaskPrincipalAxis_Widget) {
    ui->setupUi(this);

    // Set default to Major axis
    ui->majorAxisRadioButton->setChecked(true);
}

MaskPrincipalAxis_Widget::~MaskPrincipalAxis_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskPrincipalAxis_Widget::getParameters() const {
    auto params = std::make_unique<MaskPrincipalAxisParameters>();

    // Set axis type based on radio button selection
    if (ui->majorAxisRadioButton->isChecked()) {
        params->axis_type = PrincipalAxisType::Major;
    } else {
        params->axis_type = PrincipalAxisType::Minor;
    }

    return params;
}