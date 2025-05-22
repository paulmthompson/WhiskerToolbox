#include "Scaling_Widget.hpp"
#include "ui_Scaling_Widget.h"

Scaling_Widget::Scaling_Widget(QWidget * parent)
    : QWidget(parent), ui(new Ui::Scaling_Widget) {
    ui->setupUi(this);

    connect(ui->enable_image_scaling, &QCheckBox::clicked, this, &Scaling_Widget::_enableImageScaling);
    connect(ui->original_height_spin, &QSpinBox::valueChanged, this, &Scaling_Widget::scalingParametersChanged);
    connect(ui->original_width_spin, &QSpinBox::valueChanged, this, &Scaling_Widget::scalingParametersChanged);
    connect(ui->scaled_height_spin, &QSpinBox::valueChanged, this, &Scaling_Widget::scalingParametersChanged);
    connect(ui->scaled_width_spin, &QSpinBox::valueChanged, this, &Scaling_Widget::scalingParametersChanged);
    connect(ui->enable_image_scaling, &QCheckBox::clicked, this, &Scaling_Widget::scalingParametersChanged);


    ui->scaled_width_spin->setEnabled(false);
    ui->scaled_height_spin->setEnabled(false);
}

Scaling_Widget::~Scaling_Widget() {
    delete ui;
}

void Scaling_Widget::_enableImageScaling(bool enable) {
    if (enable) {
        ui->scaled_height_spin->setEnabled(true);
        ui->scaled_width_spin->setEnabled(true);
    } else {
        ui->scaled_height_spin->setEnabled(false);
        ui->scaled_width_spin->setEnabled(false);
    }
    emit scalingParametersChanged();
}

ImageSize Scaling_Widget::getOriginalImageSize() const {
    return {
            .width = ui->original_width_spin->value(),
            .height = ui->original_height_spin->value()
    };
}

ImageSize Scaling_Widget::getScaledImageSize() const {
    return {
            .width = ui->scaled_width_spin->value(),
            .height = ui->scaled_height_spin->value()
    };
}

bool Scaling_Widget::isScalingEnabled() const {
    return ui->enable_image_scaling->isChecked();
} 