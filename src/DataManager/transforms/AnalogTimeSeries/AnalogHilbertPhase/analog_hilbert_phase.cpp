#include "analog_hilbert_phase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/armadillo_wrap/analog_armadillo.hpp"
#include "utils/filter/FilterFactory.hpp"

#include <armadillo>

#include <algorithm>
#include <complex>
#include <iostream>
#include <numbers>
#include <numeric>//std::iota
#include <span>
#include <vector>

/**
     * @brief Represents a continuous chunk of data in the time series
     */
struct DataChunk {
    DataArrayIndex start_idx;         // Start index in original timestamps
    DataArrayIndex end_idx;           // End index in original timestamps (exclusive)
    TimeFrameIndex output_start;      // Start position in output array
    TimeFrameIndex output_end;        // End position in output array (exclusive)
    std::vector<float> values;        // Values for this chunk
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

            // End chunk at last valid time + 1 (exclusive)
            DataChunk chunk{.start_idx = chunk_start,
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

    // Add final chunk
    std::vector<float> final_chunk_values;
    std::vector<TimeFrameIndex> final_chunk_times;
    final_chunk_values.reserve(timestamps.size() - chunk_start.getValue());
    final_chunk_times.reserve(timestamps.size() - chunk_start.getValue());
    for (DataArrayIndex j = chunk_start; j < DataArrayIndex(timestamps.size()); ++j) {
        final_chunk_values.push_back(values[j.getValue()]);
        final_chunk_times.push_back(timestamps[j.getValue()]);
    }

    DataChunk final_chunk{.start_idx = chunk_start,
                          .end_idx = DataArrayIndex(timestamps.size()),
                          .output_start = timestamps[chunk_start.getValue()],
                          .output_end = timestamps.back() + TimeFrameIndex(1),
                          .values = std::move(final_chunk_values),
                          .times = std::move(final_chunk_times)};

    chunks.push_back(std::move(final_chunk));

    return chunks;
}

/**
 * @brief Creates a Hann window of specified length
 * @param length Length of the window
 * @return Vector containing Hann window values
 */
std::vector<float> createHannWindow(size_t length) {
    std::vector<float> window(length);
    for (size_t i = 0; i < length; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(length - 1)));
    }
    return window;
}

/**
 * @brief Splits a large chunk into smaller overlapping sub-chunks for efficient processing
 * @param chunk The data chunk to split
 * @param maxChunkSize Maximum size of each sub-chunk
 * @param overlapFraction Fraction of overlap between sub-chunks (0.0 to 0.5)
 * @return Vector of sub-chunks with overlap information
 */
struct SubChunk {
    std::vector<float> values;
    std::vector<TimeFrameIndex> times;
    size_t valid_start_idx{0};  // Index where valid (non-edge) data starts
    size_t valid_end_idx{0};    // Index where valid (non-edge) data ends (exclusive)
    TimeFrameIndex output_start{TimeFrameIndex(0)}; // Time corresponding to valid_start_idx
};

std::vector<SubChunk> splitIntoOverlappingChunks(
    std::vector<float> const & values,
    std::vector<TimeFrameIndex> const & times,
    size_t maxChunkSize,
    double overlapFraction) {
    
    std::vector<SubChunk> subchunks;
    
    if (values.empty() || maxChunkSize == 0 || values.size() <= maxChunkSize) {
        // No need to split - return single chunk with full validity
        SubChunk single;
        single.values = values;
        single.times = times;
        single.valid_start_idx = 0;
        single.valid_end_idx = values.size();
        single.output_start = times.empty() ? TimeFrameIndex(0) : times[0];
        subchunks.push_back(std::move(single));
        return subchunks;
    }
    
    // Calculate overlap size and step size
    size_t overlap_size = static_cast<size_t>(static_cast<double>(maxChunkSize) * std::clamp(overlapFraction, 0.0, 0.5));
    size_t step_size = maxChunkSize - overlap_size;
    size_t edge_discard = overlap_size / 2; // Discard this many samples from each edge
    
    for (size_t start_idx = 0; start_idx < values.size(); start_idx += step_size) {
        size_t end_idx = std::min(start_idx + maxChunkSize, values.size());
        
        // Copy data for this sub-chunk
        std::vector<float> sub_values;
        std::vector<TimeFrameIndex> sub_times;
        sub_values.reserve(end_idx - start_idx);
        sub_times.reserve(end_idx - start_idx);
        for (size_t i = start_idx; i < end_idx; ++i) {
            sub_values.push_back(values[i]);
            sub_times.push_back(times[i]);
        }
        
        // Determine valid region (exclude edges where windowing causes artifacts)
        size_t valid_start_idx = 0;
        size_t valid_end_idx = 0;
        if (start_idx == 0) {
            // First chunk: keep from beginning, discard end edge
            valid_start_idx = 0;
            valid_end_idx = (end_idx < values.size()) ? (sub_values.size() - edge_discard) : sub_values.size();
        } else if (end_idx >= values.size()) {
            // Last chunk: discard start edge, keep to end
            valid_start_idx = edge_discard;
            valid_end_idx = sub_values.size();
        } else {
            // Middle chunk: discard both edges
            valid_start_idx = edge_discard;
            valid_end_idx = sub_values.size() - edge_discard;
        }
        
        TimeFrameIndex output_start_time = sub_times[valid_start_idx];
        
        SubChunk sub;
        sub.values = std::move(sub_values);
        sub.times = std::move(sub_times);
        sub.valid_start_idx = valid_start_idx;
        sub.valid_end_idx = valid_end_idx;
        sub.output_start = output_start_time;
        
        subchunks.push_back(std::move(sub));
        
        // If we've reached the end, stop
        if (end_idx >= values.size()) {
            break;
        }
    }
    
    return subchunks;
}

/**
 * @brief Creates a bandpass filter based on parameters
 * @param params Hilbert phase parameters containing filter settings
 * @return Unique pointer to the filter, or nullptr if filtering is disabled
 */
std::unique_ptr<IFilter> createBandpassFilter(HilbertPhaseParams const & params) {
    if (!params.applyBandpassFilter) {
        return nullptr;
    }
    
    // Create zero-phase Butterworth bandpass filter with runtime order dispatch
    switch (params.filterOrder) {
        case 1: return FilterFactory::createButterworthBandpass<1>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 2: return FilterFactory::createButterworthBandpass<2>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 3: return FilterFactory::createButterworthBandpass<3>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 4: return FilterFactory::createButterworthBandpass<4>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 5: return FilterFactory::createButterworthBandpass<5>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 6: return FilterFactory::createButterworthBandpass<6>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 7: return FilterFactory::createButterworthBandpass<7>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        case 8: return FilterFactory::createButterworthBandpass<8>(
            params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
        default:
            std::cerr << "Warning: Invalid filter order " << params.filterOrder 
                      << ", defaulting to order 4" << std::endl;
            return FilterFactory::createButterworthBandpass<4>(
                params.filterLowFreq, params.filterHighFreq, params.samplingRate, true);
    }
}

/**
 * @brief Applies Hilbert transform to a signal vector
 * @param signal Input signal
 * @param outputType Whether to extract phase or amplitude
 * @param applyWindow Whether to apply Hann window
 * @param filter Optional bandpass filter to apply before Hilbert transform
 * @return Processed output (phase or amplitude)
 */
std::vector<float> applyHilbertTransform(
    std::vector<float> const & signal,
    HilbertPhaseParams::OutputType outputType,
    bool applyWindow,
    IFilter * filter = nullptr) {
    
    if (signal.empty()) {
        return {};
    }
    
    // Convert to arma::vec
    arma::vec arma_signal(signal.size());
    std::copy(signal.begin(), signal.end(), arma_signal.begin());
    
    // Apply bandpass filter first if provided
    if (filter != nullptr) {
        std::vector<float> filter_buffer(signal.begin(), signal.end());
        filter->process(std::span<float>(filter_buffer.data(), filter_buffer.size()));
        std::copy(filter_buffer.begin(), filter_buffer.end(), arma_signal.begin());
    }
    
    // Apply window if requested
    if (applyWindow && signal.size() > 1) {
        auto window = createHannWindow(signal.size());
        for (size_t i = 0; i < signal.size(); ++i) {
            arma_signal(i) *= static_cast<double>(window[i]);
        }
    }
    
    // Perform FFT
    arma::cx_vec X = arma::fft(arma_signal).eval();
    
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
    
    // Extract either phase or amplitude
    arma::vec output_values;
    if (outputType == HilbertPhaseParams::OutputType::Phase) {
        output_values = arma::arg(analytic_signal);
    } else {
        output_values = arma::abs(analytic_signal);
    }
    
    // Convert back to standard vector
    return arma::conv_to<std::vector<float>>::from(output_values);
}

/**
     * @brief Processes a single continuous chunk using Hilbert transform
     * @param chunk The data chunk to process
     * @param phaseParams Parameters for the calculation
     * @return Vector of phase or amplitude values for this chunk depending on outputType
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
        return std::vector<float>(static_cast<size_t>(chunk.output_end.getValue() - chunk.output_start.getValue()), 0.0f);
    }

    // Check if we need to split into smaller chunks for efficiency
    bool useWindowedProcessing = phaseParams.maxChunkSize > 0 && clean_values.size() > phaseParams.maxChunkSize;
    
    // Create bandpass filter if requested (shared across all sub-chunks)
    auto filter = createBandpassFilter(phaseParams);
    
    std::vector<float> result_values;
    
    if (useWindowedProcessing) {
        // Split into overlapping sub-chunks
        std::cout << "  Using windowed processing with " << phaseParams.maxChunkSize 
                  << " samples per window, " << (phaseParams.overlapFraction * 100.0) << "% overlap" << std::endl;
        
        if (filter) {
            std::cout << "  Applying " << filter->getName() << " bandpass filter" << std::endl;
        }
        
        auto subchunks = splitIntoOverlappingChunks(clean_values, clean_times, 
                                                     phaseParams.maxChunkSize, 
                                                     phaseParams.overlapFraction);
        
        std::cout << "  Split into " << subchunks.size() << " sub-chunks" << std::endl;
        
        // Process each sub-chunk and collect valid results
        std::vector<float> all_results;
        std::vector<TimeFrameIndex> all_result_times;
        
        for (auto const & subchunk : subchunks) {
            // Reset filter state for each sub-chunk (zero-phase filter handles this internally)
            if (filter) {
                filter->reset();
            }
            
            // Apply Hilbert transform to this sub-chunk
            auto sub_result = applyHilbertTransform(subchunk.values, 
                                                    phaseParams.outputType, 
                                                    phaseParams.useWindowing,
                                                    filter.get());
            
            // Extract only the valid portion (excluding edges)
            for (size_t i = subchunk.valid_start_idx; i < subchunk.valid_end_idx; ++i) {
                all_results.push_back(sub_result[i]);
                all_result_times.push_back(subchunk.times[i]);
            }
        }
        
        result_values = std::move(all_results);
        clean_times = std::move(all_result_times);
        
    } else {
        // Process entire chunk at once (original behavior for small chunks)
        if (filter) {
            std::cout << "  Applying " << filter->getName() << " bandpass filter" << std::endl;
            filter->reset();
        }
        result_values = applyHilbertTransform(clean_values, phaseParams.outputType, false, filter.get());
    }

    // Now result_values and clean_times contain the processed data
    // Create output vector for this chunk and handle interpolation for small gaps

    // Create output vector for this chunk only
    auto chunk_size = static_cast<size_t>(chunk.output_end.getValue() - chunk.output_start.getValue());
    std::vector<float> output_data(chunk_size, 0.0f);

    // Fill in the original points (excluding NaN values)
    for (size_t i = 0; i < clean_times.size(); ++i) {
        auto output_idx = static_cast<size_t>(clean_times[i].getValue() - chunk.output_start.getValue());
        if (output_idx < output_data.size()) {
            output_data[output_idx] = result_values[i];
        }
    }

    // Interpolate small gaps within this chunk only
    for (size_t i = 1; i < clean_times.size(); ++i) {
        int64_t gap = clean_times[i].getValue() - clean_times[i - 1].getValue();
        if (gap > 1 && static_cast<size_t>(gap) <= phaseParams.discontinuityThreshold) {
            // Linear interpolation for points in between
            float value_start = result_values[i - 1];
            float value_end = result_values[i];

            // Handle phase wrapping only for phase output
            if (phaseParams.outputType == HilbertPhaseParams::OutputType::Phase) {
                if (value_end - value_start > static_cast<float>(std::numbers::pi)) {
                    value_start += 2.0f * static_cast<float>(std::numbers::pi);
                } else if (value_start - value_end > static_cast<float>(std::numbers::pi)) {
                    value_end += 2.0f * static_cast<float>(std::numbers::pi);
                }
            }

            for (int64_t j = 1; j < gap; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(gap);
                float interpolated_value = value_start + t * (value_end - value_start);

                // Wrap back to [-π, π] only for phase output
                if (phaseParams.outputType == HilbertPhaseParams::OutputType::Phase) {
                    while (interpolated_value > static_cast<float>(std::numbers::pi)) interpolated_value -= 2.0f * static_cast<float>(std::numbers::pi);
                    while (interpolated_value <= -static_cast<float>(std::numbers::pi)) interpolated_value += 2.0f * static_cast<float>(std::numbers::pi);
                }

                auto output_idx = static_cast<size_t>((clean_times[i - 1].getValue() + j) - chunk.output_start.getValue());
                if (output_idx < output_data.size()) {
                    output_data[output_idx] = interpolated_value;
                }
            }
        }
    }

    return output_data;
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
    auto total_size = static_cast<size_t>(last_chunk.output_end.getValue());

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
            auto start_idx = static_cast<size_t>(chunk.output_start.getValue());
            size_t end_idx = std::min(start_idx + chunk_phase.size(), output_data.size());
            std::copy(chunk_phase.begin(),
                      chunk_phase.begin() + static_cast<long int>(end_idx - start_idx),
                      output_data.begin() + static_cast<long int>(start_idx));
        }

        // Update progress
        if (progressCallback) {
            int progress = 5 + static_cast<int>((90.0f * static_cast<float>(i + 1)) / static_cast<float>(total_chunks));
            progressCallback(progress);
        }
    }

    // Create result
    auto result = std::make_shared<AnalogTimeSeries>(std::move(output_data), std::move(output_times));

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

std::unique_ptr<TransformParametersBase> HilbertPhaseOperation::getDefaultParameters() const {
    return std::make_unique<HilbertPhaseParams>();
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
