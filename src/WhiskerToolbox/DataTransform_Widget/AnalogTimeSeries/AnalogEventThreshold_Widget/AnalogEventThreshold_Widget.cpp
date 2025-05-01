
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

    // Populate the members from UI elements
    // params->thresholdValue = ui->thresholdSpinBox->value();
    // params->direction = static_cast<ThresholdParams::ThresholdDirection>(ui->directionComboBox->currentData().toInt());
    // ... (add validation if needed before returning) ...

    return params;
}

