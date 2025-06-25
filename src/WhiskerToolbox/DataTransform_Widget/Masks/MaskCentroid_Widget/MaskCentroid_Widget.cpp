#include "MaskCentroid_Widget.hpp"
#include "ui_MaskCentroid_Widget.h"

MaskCentroid_Widget::MaskCentroid_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::MaskCentroid_Widget) {
    ui->setupUi(this);

    // No additional setup needed since centroid calculation requires no parameters
}

MaskCentroid_Widget::~MaskCentroid_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> MaskCentroid_Widget::getParameters() const {
    // Return default parameters since centroid calculation doesn't require user input
    return std::make_unique<MaskCentroidParameters>();
}