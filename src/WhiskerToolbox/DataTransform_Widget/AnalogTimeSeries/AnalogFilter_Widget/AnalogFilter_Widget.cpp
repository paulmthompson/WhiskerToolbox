#include "AnalogFilter_Widget.hpp"
#include "ui_AnalogFilter_Widget.h"
#include "DataManager/transforms/AnalogTimeSeries/analog_filter.hpp"

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
    bool is_rbj = (filter_type == "RBJ");
    bool is_chebyshev = (filter_type.find("Chebyshev") != std::string::npos);
    bool is_band_filter = (response == "Band Pass" || response == "Band Stop (Notch)");

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

    double cutoff = ui->cutoff_frequency_spinbox->value();
    double high_cutoff = ui->high_cutoff_spinbox->value();
    bool is_band_filter = ui->response_combobox->currentText().contains("Band");

    // Basic validation
    if (cutoff <= 0.0) {
        valid = false;
        error_message = "Cutoff frequency must be positive";
    }

    // For band filters, validate high cutoff
    if (is_band_filter && ui->high_cutoff_spinbox->isVisible()) {
        if (high_cutoff <= cutoff) {
            valid = false;
            error_message = "High cutoff must be greater than low cutoff";
        }
    }

    // Show error message if invalid
    if (!valid) {
        QMessageBox::warning(this, "Invalid Parameters", error_message);
    }
}

std::unique_ptr<TransformParametersBase> AnalogFilter_Widget::getParameters() const {
    auto params = std::make_unique<AnalogFilterParams>();

    // Set filter type
    QString type_str = ui->filter_type_combobox->currentText();
    if (type_str == "Butterworth") {
        params->filter_options.type = FilterType::Butterworth;
    } else if (type_str == "Chebyshev I") {
        params->filter_options.type = FilterType::ChebyshevI;
    } else if (type_str == "Chebyshev II") {
        params->filter_options.type = FilterType::ChebyshevII;
    } else if (type_str == "RBJ") {
        params->filter_options.type = FilterType::RBJ;
    }

    // Set response type
    QString response_str = ui->response_combobox->currentText();
    if (response_str == "Low Pass") {
        params->filter_options.response = FilterResponse::LowPass;
    } else if (response_str == "High Pass") {
        params->filter_options.response = FilterResponse::HighPass;
    } else if (response_str == "Band Pass") {
        params->filter_options.response = FilterResponse::BandPass;
    } else if (response_str == "Band Stop (Notch)") {
        params->filter_options.response = FilterResponse::BandStop;
    }

    // Set frequencies and other parameters
    params->filter_options.cutoff_frequency_hz = ui->cutoff_frequency_spinbox->value();
    params->filter_options.high_cutoff_hz = ui->high_cutoff_spinbox->value();
    params->filter_options.order = ui->order_spinbox->value();
    params->filter_options.q_factor = ui->q_factor_spinbox->value();
    params->filter_options.passband_ripple_db = ui->ripple_spinbox->value();
    params->filter_options.zero_phase = ui->zero_phase_checkbox->isChecked();

    return params;
} 