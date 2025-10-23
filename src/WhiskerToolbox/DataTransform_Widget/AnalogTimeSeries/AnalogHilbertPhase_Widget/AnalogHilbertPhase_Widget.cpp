#include "AnalogHilbertPhase_Widget.hpp"

#include "ui_AnalogHilbertPhase_Widget.h"

#include "DataManager/transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"

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
    
    // Get output type from combo box
    if (ui->output_type_combobox->currentText() == "Phase") {
        params->outputType = HilbertPhaseParams::OutputType::Phase;
    } else {
        params->outputType = HilbertPhaseParams::OutputType::Amplitude;
    }
    
    params->discontinuityThreshold = static_cast<size_t>(ui->discontinuity_threshold_spinbox->value());
    
    // Windowed processing parameters
    params->maxChunkSize = static_cast<size_t>(ui->max_chunk_size_spinbox->value());
    params->overlapFraction = ui->overlap_fraction_spinbox->value();
    params->useWindowing = ui->use_windowing_checkbox->isChecked();
    
    // Bandpass filter parameters
    params->applyBandpassFilter = ui->apply_bandpass_filter_checkbox->isChecked();
    params->filterLowFreq = ui->filter_low_freq_spinbox->value();
    params->filterHighFreq = ui->filter_high_freq_spinbox->value();
    params->filterOrder = ui->filter_order_spinbox->value();
    params->samplingRate = ui->sampling_rate_spinbox->value();

    return params;
}
