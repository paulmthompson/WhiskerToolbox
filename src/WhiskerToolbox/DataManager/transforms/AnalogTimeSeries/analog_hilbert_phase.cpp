
#include "analog_hilbert_phase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/armadillo_wrap/analog_armadillo.hpp"

#include <armadillo>

#include <complex>

std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams) {
    auto phase_series = std::make_shared<AnalogTimeSeries>();

    // Get input data
    auto const & timestamps = analog_time_series->getTimeSeries(); // Copy because we will use these to construct output
    auto const & values = analog_time_series->getAnalogTimeSeries();
    auto output_timestamps = std::vector<size_t>(timestamps.back());
    std::iota(output_timestamps.begin(), output_timestamps.end(), 0);

    if (timestamps.empty()) {
        return phase_series;
    }

    // Convert to arma::vec for processing
    auto signal = convertAnalogTimeSeriesToMlpackArray(analog_time_series, output_timestamps);
    signal.replace(arma::datum::nan, 0.0);

    // Calculate sampling rate from timestamps
    double dt = 0;
    if (timestamps.size() > 1) {
        dt = (timestamps[timestamps.size()-1] - timestamps[0]) / (timestamps.size() - 1);
    }
    double Fs = 1.0 / dt;

    // Bandpass filter design
    // Convert Hz to normalized frequency (0 to 1, where 1 is Nyquist frequency)
    double low_norm = phaseParams.lowFrequency / (Fs/2.0);
    double high_norm = phaseParams.highFrequency / (Fs/2.0);

    // Check if frequencies are valid
    if (low_norm >= 1.0 || high_norm >= 1.0 || low_norm <= 0 || high_norm <= 0) {
        std::cerr << "Invalid cutoff frequencies for bandpass filter" << std::endl;
        //return phase_series;
    }

    // Use Armadillo's built-in filter functions
    // For simplicity, using a basic butterworth filter design
    arma::vec b, a;

    // Design bandpass filter (4th order Butterworth)
    arma::vec filtered_signal = signal.as_col(); // Placeholder for filtered signal

    // Perform FFT
    auto X = arma::fft(signal).eval();

    // Set negative frequencies to zero
    int const halfway = 1 + X.n_elem / 2;
    for (int i = halfway; i < X.n_elem; i++) {
        X(i) = std::complex<double>(0, 0);
    }

    // Compute inverse FFT to get the analytic signal
    auto analytic_signal = arma::ifft(X).eval();

    // Extract phase
    arma::vec phase(analytic_signal.n_elem);
    for (int i = 0; i < analytic_signal.n_elem; i++) {
        phase(i) = std::arg(analytic_signal(i));
    }

    // Convert back to standard vector and set in the output time series
    auto phase_vec = arma::conv_to<std::vector<float>>::from(phase);
    phase_series->setData(phase_vec, output_timestamps);

    std::cout << "Total size of phase is " << phase_vec.size() << std::endl;
    std::cout << "Max value is " << calculate_max(*phase_series.get()) << std::endl;
    std::cout << "Min value is " << calculate_min(*phase_series.get()) << std::endl;

    return phase_series;
}

std::string HilbertPhaseOperation::getName() const {
    return "Hilbert Phase";
}

std::type_index HilbertPhaseOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool HilbertPhaseOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant HilbertPhaseOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "HilbertPhaseOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    HilbertPhaseParams currentParams;

    if (transformParameters != nullptr) {
        HilbertPhaseParams const * specificParams = dynamic_cast<HilbertPhaseParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            std::cout << "Using parameters provided by UI." << std::endl;
        } else {
            std::cerr << "Warning: HilbertPhaseOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
        }
    } else {
        // No parameter object was provided (nullptr). This might be expected if the
        // operation can run with defaults or was called programmatically.
        std::cout << "HilbertPhaseOperation received null parameters. Using default parameters." << std::endl;
    }

    std::shared_ptr<AnalogTimeSeries> result = hilbert_phase(analog_raw_ptr,
                                                             currentParams);

    if (!result) {
        std::cerr << "HilbertPhaseOperation::execute: 'calculate_intervals' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "HilbertPhaseOperation executed successfully using variant input." << std::endl;
    return result;
}
