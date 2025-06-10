#include "GroupIntervals_Widget.hpp"

#include "ui_GroupIntervals_Widget.h"

#include "DataManager/transforms/DigitalIntervalSeries/digital_interval_group.hpp"

#include <iostream>

GroupIntervals_Widget::GroupIntervals_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::GroupIntervals_Widget) {
    ui->setupUi(this);
}

GroupIntervals_Widget::~GroupIntervals_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> GroupIntervals_Widget::getParameters() const {
    auto params = std::make_unique<GroupParams>();

    params->maxSpacing = ui->max_spacing_spinbox->value();

    return params;
}