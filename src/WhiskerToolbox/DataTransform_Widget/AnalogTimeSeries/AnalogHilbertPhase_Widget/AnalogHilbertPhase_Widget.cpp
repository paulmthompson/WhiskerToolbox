
#include "AnalogHilbertPhase_Widget.hpp"

#include "ui_AnalogHilbertPhase_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/analog_hilbert_phase.hpp"

AnalogHilbertPhase_Widget::AnalogHilbertPhase_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::AnalogHilbertPhase_Widget)
{
    ui->setupUi(this);
}

AnalogHilbertPhase_Widget::~AnalogHilbertPhase_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> AnalogHilbertPhase_Widget::getParameters() const {
    auto params = std::make_unique<HilbertPhaseParams>();

    return params;
}
