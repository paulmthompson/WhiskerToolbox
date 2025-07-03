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

    // First check for NaN values and remove them
    std::vector<float> clean_values;
    std::vector<TimeFrameIndex> clean_times;
    clean_values.reserve(chunk.values.size());
    clean_times.reserve(chunk.times.size());
    
    for (size_t i = 0; i < chunk.values.size(); ++i) {
        if (!std::isnan(chunk.values[i])) {
            clean_values.push_back(chunk.values[i]);
            clean_times.push_back(chunk.times[i]);
        }
    }

    // If all values were NaN, return empty vector
    if (clean_values.empty()) {
        return std::vector<float>(chunk.output_end.getValue() - chunk.output_start.getValue(), 0.0f);
    }

    // Convert to arma::vec for processing
    arma::vec signal(clean_values.size());
    std::copy(clean_values.begin(), clean_values.end(), signal.begin());

    // Calculate sampling rate from timestamps
    double dt = 1.0 / 1000.0;  // Default to 1kHz if we can't calculate
    if (clean_times.size() > 1) {
        // Find minimum time difference between consecutive samples
        double min_dt = std::numeric_limits<double>::max();
        for (size_t i = 1; i < clean_times.size(); ++i) {
            double diff = static_cast<double>(clean_times[i].getValue() - clean_times[i-1].getValue());
            if (diff > 0) {
                min_dt = std::min(min_dt, diff);
            }
        }
        if (min_dt != std::numeric_limits<double>::max()) {
            dt = min_dt / 1000.0;  // Convert from TimeFrameIndex units to seconds
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
    std::vector<float> phase_values = arma::conv_to<std::vector<float>>::from(phase);

    // Create output vector with same size as the time range
    std::vector<float> output_phase(chunk.output_end.getValue() - chunk.output_start.getValue(), 0.0f);

    // First, fill in the original points (excluding NaN values)
    for (size_t i = 0; i < clean_times.size(); ++i) {
        size_t output_idx = clean_times[i].getValue() - chunk.output_start.getValue();
        if (output_idx < output_phase.size()) {
            output_phase[output_idx] = phase_values[i];
        }
    }

    // Then interpolate small gaps
    for (size_t i = 1; i < clean_times.size(); ++i) {
        int64_t gap = clean_times[i].getValue() - clean_times[i-1].getValue();
        if (gap > 1 && static_cast<size_t>(gap) <= phaseParams.discontinuityThreshold) {
            // Linear interpolation for points in between
            float phase_start = phase_values[i-1];
            float phase_end = phase_values[i];
            
            // Handle phase wrapping
            if (phase_end - phase_start > M_PI) {
                phase_start += 2.0f * M_PI;
            } else if (phase_start - phase_end > M_PI) {
                phase_end += 2.0f * M_PI;
            }

            for (int64_t j = 1; j < gap; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(gap);
                float interpolated_phase = phase_start + t * (phase_end - phase_start);
                
                // Wrap back to [-π, π]
                while (interpolated_phase > M_PI) interpolated_phase -= 2.0f * M_PI;
                while (interpolated_phase <= -M_PI) interpolated_phase += 2.0f * M_PI;
                
                size_t output_idx = (clean_times[i-1].getValue() + j) - chunk.output_start.getValue();
                if (output_idx < output_phase.size()) {
                    output_phase[output_idx] = interpolated_phase;
                }
            }
        }
    }

    return output_phase;
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
    if (chunks.empty()) {
        std::cerr << "hilbert_phase: No valid chunks detected" << std::endl;
        return std::make_shared<AnalogTimeSeries>();
    }

    // Determine total output size based on last chunk's end
    auto const & last_chunk = chunks.back();
    size_t total_size = last_chunk.output_end.getValue();

    // Create output vectors with proper size
    std::vector<float> output_data(total_size, 0.0f);
    std::vector<TimeFrameIndex> output_times;
    output_times.reserve(total_size);
    for (size_t i = 0; i < total_size; ++i) {
        output_times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }

    // Process each chunk
    size_t total_chunks = chunks.size();
    for (size_t i = 0; i < chunks.size(); ++i) {
        auto const & chunk = chunks[i];
        
        // Process chunk
        auto chunk_phase = processChunk(chunk, phaseParams);
        
        // Copy chunk results to output
        if (!chunk_phase.empty()) {
            size_t start_idx = chunk.output_start.getValue();
            size_t end_idx = std::min(start_idx + chunk_phase.size(), output_data.size());
            std::copy(chunk_phase.begin(), 
                     chunk_phase.begin() + (end_idx - start_idx),
                     output_data.begin() + start_idx);
        }

        // Update progress
        if (progressCallback) {
            int progress = 5 + static_cast<int>((90.0f * (i + 1)) / total_chunks);
            progressCallback(progress);
        }
    }

    // Create result
    auto result = std::make_shared<AnalogTimeSeries>(std::move(output_data), std::move(output_times));

    // Copy TimeFrameV2 if present
    if (analog_time_series->hasTimeFrameV2()) {
        result->setTimeFrameV2(analog_time_series->getTimeFrameV2().value());
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return result;
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
