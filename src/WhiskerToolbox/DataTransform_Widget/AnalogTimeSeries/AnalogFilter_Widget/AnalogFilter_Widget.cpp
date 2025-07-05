#include "AnalogFilter_Widget.hpp"
#include "ui_AnalogFilter_Widget.h"
#include "DataManager/transforms/AnalogTimeSeries/analog_filter.hpp"
#include "DataManager/utils/filter/FilterFactory.hpp"
#include "DataManager/utils/filter/IFilter.hpp"

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

    double sampling_rate = ui->sampling_rate_spinbox->value();
    double cutoff = ui->cutoff_frequency_spinbox->value();
    double high_cutoff = ui->high_cutoff_spinbox->value();
    bool is_band_filter = ui->response_combobox->currentText().contains("Band");

    // Validate sampling rate
    if (sampling_rate <= 0.0) {
        valid = false;
        error_message = "Sampling rate must be positive";
    }

    // Validate cutoff frequencies against Nyquist
    double nyquist = sampling_rate / 2.0;
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
    // Get current UI values
    double sampling_rate = ui->sampling_rate_spinbox->value();
    double cutoff_freq = ui->cutoff_frequency_spinbox->value();
    double high_cutoff = ui->high_cutoff_spinbox->value();
    int order = ui->order_spinbox->value();
    double q_factor = ui->q_factor_spinbox->value();
    double ripple_db = ui->ripple_spinbox->value();
    bool zero_phase = ui->zero_phase_checkbox->isChecked();
    
    QString type_str = ui->filter_type_combobox->currentText();
    QString response_str = ui->response_combobox->currentText();
    
    std::unique_ptr<IFilter> filter;
        
    if (type_str == "Butterworth") {
            if (response_str == "Low Pass") {
                filter = createButterworthLowpassByOrder(order, cutoff_freq, sampling_rate, zero_phase);
            } else if (response_str == "High Pass") {
                filter = createButterworthHighpassByOrder(order, cutoff_freq, sampling_rate, zero_phase);
            } else if (response_str == "Band Pass") {
                filter = createButterworthBandpassByOrder(order, cutoff_freq, high_cutoff, sampling_rate, zero_phase);
            } else if (response_str == "Band Stop (Notch)") {
                filter = createButterworthBandstopByOrder(order, cutoff_freq, high_cutoff, sampling_rate, zero_phase);
            }
        } else if (type_str == "Chebyshev I") {
            if (response_str == "Low Pass") {
                filter = createChebyshevILowpassByOrder(order, cutoff_freq, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "High Pass") {
                filter = createChebyshevIHighpassByOrder(order, cutoff_freq, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "Band Pass") {
                filter = createChebyshevIBandpassByOrder(order, cutoff_freq, high_cutoff, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "Band Stop (Notch)") {
                filter = createChebyshevIBandstopByOrder(order, cutoff_freq, high_cutoff, sampling_rate, ripple_db, zero_phase);
            }
        } else if (type_str == "Chebyshev II") {
            if (response_str == "Low Pass") {
                filter = createChebyshevIILowpassByOrder(order, cutoff_freq, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "High Pass") {
                filter = createChebyshevIIHighpassByOrder(order, cutoff_freq, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "Band Pass") {
                filter = createChebyshevIIBandpassByOrder(order, cutoff_freq, high_cutoff, sampling_rate, ripple_db, zero_phase);
            } else if (response_str == "Band Stop (Notch)") {
                filter = createChebyshevIIBandstopByOrder(order, cutoff_freq, high_cutoff, sampling_rate, ripple_db, zero_phase);
            }
        } else if (type_str == "RBJ") {
            if (response_str == "Low Pass") {
                filter = FilterFactory::createRBJLowpass(cutoff_freq, sampling_rate, q_factor, zero_phase);
            } else if (response_str == "High Pass") {
                filter = FilterFactory::createRBJHighpass(cutoff_freq, sampling_rate, q_factor, zero_phase);
            } else if (response_str == "Band Pass") {
                // For RBJ bandpass, use cutoff_freq as center frequency
                filter = FilterFactory::createRBJBandpass(cutoff_freq, sampling_rate, q_factor, zero_phase);
            } else if (response_str == "Band Stop (Notch)") {
                filter = FilterFactory::createRBJBandstop(cutoff_freq, sampling_rate, q_factor, zero_phase);
            }
        }
        
    if (filter) {
         // Create parameters with the filter instance
        return std::make_unique<AnalogFilterParams>(
            AnalogFilterParams::withFilter(std::shared_ptr<IFilter>(std::move(filter))));
    } else {
        // If no filter was created, return empty parameters
        return std::make_unique<AnalogFilterParams>();
    }
}

// Helper functions to create filters with runtime order selection
std::unique_ptr<IFilter> AnalogFilter_Widget::createButterworthLowpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createButterworthLowpass<1>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 2: return FilterFactory::createButterworthLowpass<2>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 3: return FilterFactory::createButterworthLowpass<3>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 4: return FilterFactory::createButterworthLowpass<4>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 5: return FilterFactory::createButterworthLowpass<5>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 6: return FilterFactory::createButterworthLowpass<6>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 7: return FilterFactory::createButterworthLowpass<7>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 8: return FilterFactory::createButterworthLowpass<8>(cutoff_hz, sampling_rate_hz, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createButterworthHighpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createButterworthHighpass<1>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 2: return FilterFactory::createButterworthHighpass<2>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 3: return FilterFactory::createButterworthHighpass<3>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 4: return FilterFactory::createButterworthHighpass<4>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 5: return FilterFactory::createButterworthHighpass<5>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 6: return FilterFactory::createButterworthHighpass<6>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 7: return FilterFactory::createButterworthHighpass<7>(cutoff_hz, sampling_rate_hz, zero_phase);
        case 8: return FilterFactory::createButterworthHighpass<8>(cutoff_hz, sampling_rate_hz, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createButterworthBandpassByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createButterworthBandpass<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 2: return FilterFactory::createButterworthBandpass<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 3: return FilterFactory::createButterworthBandpass<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 4: return FilterFactory::createButterworthBandpass<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 5: return FilterFactory::createButterworthBandpass<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 6: return FilterFactory::createButterworthBandpass<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 7: return FilterFactory::createButterworthBandpass<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 8: return FilterFactory::createButterworthBandpass<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createButterworthBandstopByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createButterworthBandstop<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 2: return FilterFactory::createButterworthBandstop<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 3: return FilterFactory::createButterworthBandstop<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 4: return FilterFactory::createButterworthBandstop<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 5: return FilterFactory::createButterworthBandstop<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 6: return FilterFactory::createButterworthBandstop<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 7: return FilterFactory::createButterworthBandstop<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        case 8: return FilterFactory::createButterworthBandstop<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

// Similar helper functions for Chebyshev I filters
std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevILowpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevILowpass<1>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevILowpass<2>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevILowpass<3>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevILowpass<4>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevILowpass<5>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevILowpass<6>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevILowpass<7>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevILowpass<8>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIHighpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIHighpass<1>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIHighpass<2>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIHighpass<3>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIHighpass<4>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIHighpass<5>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIHighpass<6>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIHighpass<7>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIHighpass<8>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIBandpassByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIBandpass<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIBandpass<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIBandpass<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIBandpass<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIBandpass<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIBandpass<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIBandpass<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIBandpass<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIBandstopByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIBandstop<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIBandstop<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIBandstop<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIBandstop<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIBandstop<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIBandstop<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIBandstop<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIBandstop<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

// Similar helper functions for Chebyshev II filters
std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIILowpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIILowpass<1>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIILowpass<2>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIILowpass<3>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIILowpass<4>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIILowpass<5>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIILowpass<6>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIILowpass<7>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIILowpass<8>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIIHighpassByOrder(
        int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIIHighpass<1>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIIHighpass<2>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIIHighpass<3>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIIHighpass<4>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIIHighpass<5>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIIHighpass<6>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIIHighpass<7>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIIHighpass<8>(cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIIBandpassByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIIBandpass<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIIBandpass<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIIBandpass<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIIBandpass<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIIBandpass<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIIBandpass<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIIBandpass<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIIBandpass<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}

std::unique_ptr<IFilter> AnalogFilter_Widget::createChebyshevIIBandstopByOrder(
        int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const {
    switch (order) {
        case 1: return FilterFactory::createChebyshevIIBandstop<1>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 2: return FilterFactory::createChebyshevIIBandstop<2>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 3: return FilterFactory::createChebyshevIIBandstop<3>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 4: return FilterFactory::createChebyshevIIBandstop<4>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 5: return FilterFactory::createChebyshevIIBandstop<5>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 6: return FilterFactory::createChebyshevIIBandstop<6>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 7: return FilterFactory::createChebyshevIIBandstop<7>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        case 8: return FilterFactory::createChebyshevIIBandstop<8>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
        default: throw std::invalid_argument("Unsupported filter order: " + std::to_string(order));
    }
}