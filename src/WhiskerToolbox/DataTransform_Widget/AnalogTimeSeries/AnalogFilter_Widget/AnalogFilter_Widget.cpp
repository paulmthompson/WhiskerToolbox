#include "AnalogFilter_Widget.hpp"
#include "ui_AnalogFilter_Widget.h"
#include "DataManager/transforms/AnalogTimeSeries/AnalogFilter/analog_filter.hpp"

#include <QMessageBox>

AnalogFilter_Widget::AnalogFilter_Widget(QWidget * parent) :
    TransformParameter_Widget(parent),
    ui(std::make_unique<Ui::AnalogFilter_Widget>()) {
    ui->setupUi(this);
    _setupConnections();
    _updateVisibleParameters();
}

AnalogFilter_Widget::~AnalogFilter_Widget() = default;

void AnalogFilter_Widget::_setupConnections() {
    connect(ui->filter_type_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogFilter_Widget::_onFilterTypeChanged);
    connect(ui->response_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalogFilter_Widget::_onResponseChanged);

    // Connect all parameter changes to validation
    connect(ui->sampling_rate_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
    connect(ui->cutoff_frequency_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
    connect(ui->high_cutoff_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
    connect(ui->order_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
    connect(ui->q_factor_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
    connect(ui->ripple_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalogFilter_Widget::_validateParameters);
}

void AnalogFilter_Widget::_onFilterTypeChanged(int index) {
    _updateVisibleParameters();
    _validateParameters();
}

void AnalogFilter_Widget::_onResponseChanged(int index) {
    _updateVisibleParameters();
    _validateParameters();
}

void AnalogFilter_Widget::_updateVisibleParameters() {
    // Get current filter type and response
    auto filter_type = ui->filter_type_combobox->currentText().toStdString();
    auto response = ui->response_combobox->currentText().toStdString();

    // Show/hide parameters based on filter type
    bool const is_rbj = (filter_type == "RBJ");
    bool const is_chebyshev = (filter_type.find("Chebyshev") != std::string::npos);
    bool const is_band_filter = (response == "Band Pass" || response == "Band Stop (Notch)");

    // Order is not applicable for RBJ filters (always 2nd order)
    ui->order_label->setVisible(!is_rbj);
    ui->order_spinbox->setVisible(!is_rbj);

    // Q factor only for RBJ filters
    ui->q_factor_label->setVisible(is_rbj);
    ui->q_factor_spinbox->setVisible(is_rbj);

    // Ripple only for Chebyshev filters
    ui->ripple_label->setVisible(is_chebyshev);
    ui->ripple_spinbox->setVisible(is_chebyshev);

    // High cutoff only for band filters
    ui->high_cutoff_label->setVisible(is_band_filter);
    ui->high_cutoff_spinbox->setVisible(is_band_filter);

    // Update labels based on response type
    if (is_band_filter) {
        ui->cutoff_label->setText("Low Cutoff (Hz):");
    } else {
        ui->cutoff_label->setText("Cutoff Frequency (Hz):");
    }
}

void AnalogFilter_Widget::_validateParameters() {
    bool valid = true;
    QString error_message;

    double const sampling_rate = ui->sampling_rate_spinbox->value();
    double const cutoff = ui->cutoff_frequency_spinbox->value();
    double const high_cutoff = ui->high_cutoff_spinbox->value();
    bool const is_band_filter = ui->response_combobox->currentText().contains("Band");

    // Validate sampling rate
    if (sampling_rate <= 0.0) {
        valid = false;
        error_message = "Sampling rate must be positive";
    }

    // Validate cutoff frequencies against Nyquist
    double const nyquist = sampling_rate / 2.0;
    if (cutoff >= nyquist) {
        valid = false;
        error_message = QString("Cutoff frequency must be less than Nyquist frequency (%1 Hz)").arg(nyquist);
    }

    // For band filters, validate high cutoff
    if (is_band_filter && ui->high_cutoff_spinbox->isVisible()) {
        if (high_cutoff <= cutoff) {
            valid = false;
            error_message = "High cutoff must be greater than low cutoff";
        }
        if (high_cutoff >= nyquist) {
            valid = false;
            error_message = QString("High cutoff must be less than Nyquist frequency (%1 Hz)").arg(nyquist);
        }
    }

    // Show error message if invalid
    if (!valid) {
        QMessageBox::warning(this, "Invalid Parameters", error_message);
    }
}

std::unique_ptr<TransformParametersBase> AnalogFilter_Widget::getParameters() const {
    auto params = std::make_unique<AnalogFilterParams>();

    params->sampling_rate = ui->sampling_rate_spinbox->value();
    params->cutoff_frequency = ui->cutoff_frequency_spinbox->value();
    params->cutoff_frequency2 = ui->high_cutoff_spinbox->value();
    params->order = ui->order_spinbox->value();
    params->ripple = ui->ripple_spinbox->value();
    params->zero_phase = ui->zero_phase_checkbox->isChecked();

    QString const response_str = ui->response_combobox->currentText();
    if (response_str == "Low Pass") {
        params->filter_type = AnalogFilterParams::FilterType::Lowpass;
    } else if (response_str == "High Pass") {
        params->filter_type = AnalogFilterParams::FilterType::Highpass;
    } else if (response_str == "Band Pass") {
        params->filter_type = AnalogFilterParams::FilterType::Bandpass;
    } else if (response_str == "Band Stop (Notch)") {
        params->filter_type = AnalogFilterParams::FilterType::Bandstop;
    }

    return params;
}
