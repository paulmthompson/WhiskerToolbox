
#include "AnalogIntervalThreshold_Widget.hpp"

#include "ui_AnalogIntervalThreshold_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.hpp"

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

    auto const missing_data_mode = ui->missing_data_combobox->currentText();
    if (missing_data_mode == "Treat as Zero (Default)") {
        params->missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;
    } else if (missing_data_mode == "Ignore Missing Points") {
        params->missingDataMode = IntervalThresholdParams::MissingDataMode::IGNORE;
    } else {
        std::cout << "Unknown missing data mode, using default (Treat as Zero)!" << std::endl;
        params->missingDataMode = IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO;
    }

    return params;
}

