
#include "AnalogIntervalThreshold_Widget.hpp"

#include "ui_AnalogIntervalThreshold_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/analog_interval_threshold.hpp"

#include <iostream>

AnalogIntervalThreshold_Widget::AnalogIntervalThreshold_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::AnalogIntervalThreshold_Widget)
{
    ui->setupUi(this);
}

AnalogIntervalThreshold_Widget::~AnalogIntervalThreshold_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> AnalogIntervalThreshold_Widget::getParameters() const {
    auto params = std::make_unique<IntervalThresholdParams>();

    params->thresholdValue = ui->threshold_spinbox->value();

    auto const threshold_direction = ui->direction_combobox->currentText();

    if (threshold_direction == "Positive (Rising)") {
        params->direction = IntervalThresholdParams::ThresholdDirection::POSITIVE;
    } else if (threshold_direction == "Negative (Falling)") {
        params->direction = IntervalThresholdParams::ThresholdDirection::NEGATIVE;
    } else if (threshold_direction == "Absolute (Magnitude)") {
        params->direction = IntervalThresholdParams::ThresholdDirection::ABSOLUTE;
    } else {
        std::cout << "Unknown threshold direction!" << std::endl;
    }

    params->lockoutTime = ui->lockout_spinbox->value();

    params->minDuration = ui->min_duration_spinbox->value();

    return params;
}

