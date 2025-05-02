
#include "AnalogEventThreshold_Widget.hpp"

#include "ui_AnalogEventThreshold_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/analog_event_threshold.hpp"

AnalogEventThreshold_Widget::AnalogEventThreshold_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::AnalogEventThreshold_Widget)
{
    ui->setupUi(this);


}

AnalogEventThreshold_Widget::~AnalogEventThreshold_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> AnalogEventThreshold_Widget::getParameters() const {
    auto params = std::make_unique<ThresholdParams>();

    params->thresholdValue = ui->threshold_spinbox->value();

    auto const threshold_direction = ui->direction_combobox->currentText();

    if (threshold_direction == "Positive (Rising)") {
        params->direction = ThresholdParams::ThresholdDirection::POSITIVE;
    } else if (threshold_direction == "Negative (Falling)") {
        params->direction = ThresholdParams::ThresholdDirection::NEGATIVE;
    } else if (threshold_direction == "Absolute (Magnitude)") {
        params->direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
    } else {
        std::cout << "Unknown threshold direction!" << std::endl;
    }

    return params;
}

