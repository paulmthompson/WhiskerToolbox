#include "AnalogHilbertPhase_Widget.hpp"

#include "ui_AnalogHilbertPhase_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"

AnalogHilbertPhase_Widget::AnalogHilbertPhase_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::AnalogHilbertPhase_Widget)
{
    ui->setupUi(this);
    
    // Set up connections for parameter validation
    connect(ui->low_frequency_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogHilbertPhase_Widget::_validateFrequencyParameters);
    connect(ui->high_frequency_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogHilbertPhase_Widget::_validateFrequencyParameters);
    connect(ui->discontinuity_threshold_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AnalogHilbertPhase_Widget::_validateParameters);
    
    // Initial validation
    _validateFrequencyParameters();
}

AnalogHilbertPhase_Widget::~AnalogHilbertPhase_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> AnalogHilbertPhase_Widget::getParameters() const {
    auto params = std::make_unique<HilbertPhaseParams>();
    
    params->lowFrequency = ui->low_frequency_spinbox->value();
    params->highFrequency = ui->high_frequency_spinbox->value();
    params->discontinuityThreshold = static_cast<size_t>(ui->discontinuity_threshold_spinbox->value());

    return params;
}

void AnalogHilbertPhase_Widget::_validateFrequencyParameters() {
    double lowFreq = ui->low_frequency_spinbox->value();
    double highFreq = ui->high_frequency_spinbox->value();
    
    // Update the note label based on parameter validity
    if (lowFreq >= highFreq) {
        ui->note_label->setText(
            "Warning: Low frequency must be less than high frequency.\n"
            "Current implementation processes the full signal without filtering."
        );
        ui->note_label->setStyleSheet("color: rgb(200, 100, 100); font-style: italic;");
    } else if (lowFreq <= 0 || highFreq <= 0) {
        ui->note_label->setText(
            "Warning: Frequencies must be positive.\n"
            "Current implementation processes the full signal without filtering."
        );
        ui->note_label->setStyleSheet("color: rgb(200, 100, 100); font-style: italic;");
    } else {
        ui->note_label->setText(
            "Note: Frequency parameters are for reference only.\n"
            "Current implementation processes the full signal without filtering."
        );
        ui->note_label->setStyleSheet("color: rgb(100, 100, 100); font-style: italic;");
    }
}

void AnalogHilbertPhase_Widget::_validateParameters() {
    // This can be extended to validate discontinuity threshold if needed
    _validateFrequencyParameters();
}
