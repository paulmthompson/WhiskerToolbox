
#include "analog_hilbert_phase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/armadillo_wrap/analog_armadillo.hpp"

#include <armadillo>

#include <algorithm>
#include <complex>
#include <iostream>
#include <numeric>//std::iota
#include <vector>

/**
     * @brief Represents a continuous chunk of data in the time series
     */
struct DataChunk {
    DataArrayIndex start_idx;         // Start index in original timestamps
    DataArrayIndex end_idx;           // End index in original timestamps (exclusive)
    TimeFrameIndex output_start;      // Start position in output array
    TimeFrameIndex output_end;        // End position in output array (exclusive)
    std::vector<float> values;// Values for this chunk
    std::vector<TimeFrameIndex> times;// Timestamps for this chunk

};

/**
     * @brief Detects discontinuities in the time series and splits into continuous chunks
     * @param analog_time_series Input time series
     * @param threshold Maximum gap size before considering it a discontinuity
     * @return Vector of continuous data chunks
     */
std::vector<DataChunk> detectChunks(AnalogTimeSeries const * analog_time_series, size_t threshold) {
    std::vector<DataChunk> chunks;

    auto const & timestamps = analog_time_series->getTimeSeries();
    auto const & values = analog_time_series->getAnalogTimeSeries();

    if (timestamps.empty()) {
        return chunks;
    }

    DataArrayIndex chunk_start = DataArrayIndex(0);
    TimeFrameIndex last_time = timestamps[0];

    for (DataArrayIndex i = DataArrayIndex(1); i < DataArrayIndex(timestamps.size()); ++i) {
        TimeFrameIndex current_time = timestamps[i.getValue()];
        TimeFrameIndex gap = current_time - last_time;

        // If gap is larger than threshold, end current chunk and start new one
        if (gap.getValue() > static_cast<int64_t>(threshold)) {
            
            std::vector<float> chunk_values;
            std::vector<TimeFrameIndex> chunk_times;
            chunk_values.reserve(i - chunk_start);
            chunk_times.reserve(i - chunk_start);
            for (DataArrayIndex j = chunk_start; j < i; ++j) {
                chunk_values.push_back(values[j.getValue()]);
                chunk_times.push_back(timestamps[j.getValue()]);
            }


            DataChunk chunk {.start_idx = chunk_start,
                             .end_idx = i,
                             .output_start = timestamps[chunk_start.getValue()],
                             .output_end = last_time + TimeFrameIndex(1),
                             .values = std::move(chunk_values),
                             .times = std::move(chunk_times)};

            chunks.push_back(std::move(chunk));
            chunk_start = i;
        }
        last_time = current_time;
    }

    std::vector<float> final_chunk_values;
    std::vector<TimeFrameIndex> final_chunk_times;
    final_chunk_values.reserve(timestamps.size() - chunk_start.getValue());
    final_chunk_times.reserve(timestamps.size() - chunk_start.getValue());
    for (DataArrayIndex j = chunk_start; j < DataArrayIndex(timestamps.size()); ++j) {
        final_chunk_values.push_back(values[j.getValue()]);
        final_chunk_times.push_back(timestamps[j.getValue()]);
    }

    // Add final chunk
    DataChunk final_chunk {.start_idx = chunk_start,
                           .end_idx = DataArrayIndex(timestamps.size()),
                           .output_start = timestamps[chunk_start.getValue()],
                           .output_end = timestamps.back() + TimeFrameIndex(1),
                           .values = std::move(final_chunk_values),
                           .times = std::move(final_chunk_times)};

    chunks.push_back(std::move(final_chunk));

    return chunks;
}

/**
     * @brief Processes a single continuous chunk using Hilbert transform
     * @param chunk The data chunk to process
     * @param phaseParams Parameters for the calculation
     * @return Vector of phase values for this chunk
     */
std::vector<float> processChunk(DataChunk const & chunk, HilbertPhaseParams const & phaseParams) {
    if (chunk.values.empty()) {
        return {};
    }

    std::cout << "Processing chunk with " << chunk.values.size() << " values" << std::endl;

    // Create continuous output timestamps for this chunk
    std::vector<TimeFrameIndex> output_timestamps;
    for (TimeFrameIndex i = chunk.output_start; i < chunk.output_end; ++i) {
        output_timestamps.push_back(i);
    }

    // Convert to arma::vec for processing
    arma::vec signal(chunk.values.size());
    std::copy(chunk.values.begin(), chunk.values.end(), signal.begin());

    // Calculate sampling rate from timestamps
    double dt = 1.0;// Default to 1 if we can't calculate
    if (chunk.times.size() > 1) {
        dt = static_cast<double>(chunk.times.back().getValue() - chunk.times[0].getValue()) /
             static_cast<double>(chunk.times.size() - 1);
        if (dt <= 0) {
            dt = 1.0;// Fallback to default
        }
    }
    double const Fs = 1.0 / dt;

    // Validate frequency parameters (but continue processing regardless)
    double const nyquist = Fs / 2.0;
    if (phaseParams.lowFrequency <= 0 || phaseParams.highFrequency <= 0 ||
        phaseParams.lowFrequency >= nyquist || phaseParams.highFrequency >= nyquist ||
        phaseParams.lowFrequency >= phaseParams.highFrequency) {
        // Log warning but continue processing
        std::cerr << "hilbert_phase: Invalid frequency parameters for chunk. "
                  << "Low: " << phaseParams.lowFrequency
                  << ", High: " << phaseParams.highFrequency
                  << ", Nyquist: " << nyquist << std::endl;
    }

    // Perform FFT
    arma::cx_vec X = arma::fft(signal).eval();

    // Create analytic signal by zeroing negative frequencies
    arma::uword const n = X.n_elem;
    arma::uword const halfway = (n + 1) / 2;

    // Zero out negative frequencies
    for (arma::uword i = halfway; i < n; ++i) {
        X(i) = std::complex<double>(0.0, 0.0);
    }

    // Double the positive frequencies (except DC and Nyquist if present)
    for (arma::uword i = 1; i < halfway; ++i) {
        X(i) *= 2.0;
    }

    // Compute inverse FFT to get the analytic signal
    arma::cx_vec const analytic_signal = arma::ifft(X).eval();

    // Extract phase using vectorized operation
    arma::vec const phase = arma::arg(analytic_signal);

    // Convert back to standard vector
    return arma::conv_to<std::vector<float>>::from(phase);
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams) {
    return hilbert_phase(analog_time_series, phaseParams, [](int) {});
}

std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!analog_time_series) {
        std::cerr << "hilbert_phase: Input AnalogTimeSeries is null" << std::endl;
        return std::make_shared<AnalogTimeSeries>();
    }

    auto const & timestamps = analog_time_series->getTimeSeries();
    if (timestamps.empty()) {
        std::cerr << "hilbert_phase: Input time series is empty" << std::endl;
        return std::make_shared<AnalogTimeSeries>();
    }

    if (progressCallback) {
        progressCallback(5);
    }

    // Detect discontinuous chunks
    auto chunks = detectChunks(analog_time_series, phaseParams.discontinuityThreshold);

    if (progressCallback) {
        progressCallback(10);
    }

    // Create output timestamps (continuous from 0 to last timestamp)
    std::vector<TimeFrameIndex> output_timestamps;
    for (TimeFrameIndex i = TimeFrameIndex(0); i <= timestamps.back(); ++i) {
        output_timestamps.push_back(i);
    }

    // Initialize output phase vector with zeros
    std::vector<float> phase_output(output_timestamps.size(), 0.0f);

    if (progressCallback) {
        progressCallback(15);
    }

    // Process each chunk separately
    size_t chunks_processed = 0;
    for (auto const & chunk: chunks) {
        if (progressCallback) {
            int progress = 15 + static_cast<int>((chunks_processed * 70) / chunks.size());
            progressCallback(progress);
        }

        auto chunk_phase = processChunk(chunk, phaseParams);

        // Copy chunk results to appropriate positions in output
        if (!chunk_phase.empty() && chunk.output_start < TimeFrameIndex(phase_output.size())) {
            size_t copy_size = std::min(chunk_phase.size(),
                                        static_cast<size_t>(phase_output.size() - chunk.output_start.getValue()));
            std::copy(chunk_phase.begin(),
                      chunk_phase.begin() + copy_size,
                      phase_output.begin() + chunk.output_start.getValue());
        }

        ++chunks_processed;
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return std::make_shared<AnalogTimeSeries>(phase_output, output_timestamps);
}

///////////////////////////////////////////////////////////////////////////////

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
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant HilbertPhaseOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant HilbertPhaseOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "HilbertPhaseOperation::execute: Invalid input data variant" << std::endl;
        return {};
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    HilbertPhaseParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams =
                dynamic_cast<HilbertPhaseParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "HilbertPhaseOperation::execute: Incompatible parameter type, using defaults" << std::endl;
        }
    }

    std::shared_ptr<AnalogTimeSeries> result = hilbert_phase(
            analog_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "HilbertPhaseOperation::execute: Phase calculation failed" << std::endl;
        return {};
    }

    return result;
}
