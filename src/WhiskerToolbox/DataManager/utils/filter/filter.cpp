#include "filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>

// ========== FilterOptions Validation ==========

bool FilterOptions::isValid() const {
    return getValidationError().empty();
}

std::string FilterOptions::getValidationError() const {
    if (order < 1 || order > max_filter_order) {
        return "Filter order must be between 1 and " + std::to_string(max_filter_order);
    }

    if (sampling_rate_hz <= 0.0) {
        return "Sampling rate must be positive";
    }

    if (cutoff_frequency_hz <= 0.0) {
        return "Cutoff frequency must be positive";
    }

    double nyquist = sampling_rate_hz / 2.0;
    if (cutoff_frequency_hz >= nyquist) {
        return "Cutoff frequency must be less than Nyquist frequency (" +
               std::to_string(nyquist) + " Hz)";
    }

    // Additional validation for band filters
    if (response == FilterResponse::BandPass || response == FilterResponse::BandStop) {
        // For RBJ filters, we use cutoff_frequency_hz as center frequency and q_factor
        if (type == FilterType::RBJ) {
            if (cutoff_frequency_hz >= nyquist) {
                return "Center frequency must be less than Nyquist frequency";
            }
            if (q_factor <= 0.0) {
                return "Q factor must be positive for RBJ band filters";
            }
        } else {
            // For IIR filters, we need both cutoff frequencies
            if (high_cutoff_hz <= cutoff_frequency_hz) {
                return "High cutoff frequency must be greater than low cutoff frequency";
            }
            if (high_cutoff_hz >= nyquist) {
                return "High cutoff frequency must be less than Nyquist frequency";
            }
        }
    }

    // Validation for filter-specific parameters
    if (type == FilterType::ChebyshevI && passband_ripple_db <= 0.0) {
        return "Chebyshev I passband ripple must be positive";
    }

    if (type == FilterType::ChebyshevII && stopband_ripple_db <= 0.0) {
        return "Chebyshev II stopband ripple must be positive";
    }

    if (type == FilterType::RBJ && q_factor <= 0.0) {
        return "RBJ Q factor must be positive";
    }

    return "";// Valid
}

// ========== Sampling Rate Estimation ==========

double estimateSamplingRate(
        AnalogTimeSeries const * analog_time_series,
        std::optional<TimeFrameIndex> start_time,
        std::optional<TimeFrameIndex> end_time) {
    if (!analog_time_series) {
        return 0.0;
    }

    size_t num_samples = analog_time_series->getNumSamples();
    if (num_samples < 2) {
        return 0.0;
    }

    // Get time series for analysis
    auto time_indices = analog_time_series->getTimeSeries();

    // Determine analysis range
    size_t start_idx = 0;
    size_t end_idx = time_indices.size();

    if (start_time.has_value()) {
        auto start_data_idx = analog_time_series->findDataArrayIndexGreaterOrEqual(start_time.value());
        if (start_data_idx.has_value()) {
            start_idx = start_data_idx.value().getValue();
        }
    }

    if (end_time.has_value()) {
        auto end_data_idx = analog_time_series->findDataArrayIndexLessOrEqual(end_time.value());
        if (end_data_idx.has_value()) {
            end_idx = std::min(end_data_idx.value().getValue() + 1, time_indices.size());
        }
    }

    if (end_idx <= start_idx + 1) {
        return 0.0;
    }

    // Calculate time differences
    std::vector<double> time_diffs;
    time_diffs.reserve(end_idx - start_idx - 1);

    for (size_t i = start_idx + 1; i < end_idx; ++i) {
        double dt = static_cast<double>(time_indices[i].getValue() - time_indices[i - 1].getValue());
        if (dt > 0) {
            time_diffs.push_back(dt);
        }
    }

    if (time_diffs.empty()) {
        return 0.0;
    }

    // Use median time difference for robustness
    std::sort(time_diffs.begin(), time_diffs.end());
    double median_dt = time_diffs[time_diffs.size() / 2];

    // Assume time indices are in units that give sampling rate of 1/dt
    // This is a heuristic - users should specify sampling rate explicitly
    return 1.0 / median_dt;
}

// ========== Filter Creation Helpers ==========

namespace {

template<int Order>
class FilterVariant {
public:
    // Butterworth filters
    std::optional<Iir::Butterworth::LowPass<Order>> butterworth_lowpass;
    std::optional<Iir::Butterworth::HighPass<Order>> butterworth_highpass;
    std::optional<Iir::Butterworth::BandPass<Order>> butterworth_bandpass;
    std::optional<Iir::Butterworth::BandStop<Order>> butterworth_bandstop;

    // Chebyshev I filters
    std::optional<Iir::ChebyshevI::LowPass<Order>> chebyshev1_lowpass;
    std::optional<Iir::ChebyshevI::HighPass<Order>> chebyshev1_highpass;
    std::optional<Iir::ChebyshevI::BandPass<Order>> chebyshev1_bandpass;
    std::optional<Iir::ChebyshevI::BandStop<Order>> chebyshev1_bandstop;

    // Chebyshev II filters
    std::optional<Iir::ChebyshevII::LowPass<Order>> chebyshev2_lowpass;
    std::optional<Iir::ChebyshevII::HighPass<Order>> chebyshev2_highpass;
    std::optional<Iir::ChebyshevII::BandPass<Order>> chebyshev2_bandpass;
    std::optional<Iir::ChebyshevII::BandStop<Order>> chebyshev2_bandstop;

    void setupFilter(FilterOptions const & options) {
        try {
            switch (options.type) {
                case FilterType::Butterworth:
                    setupButterworthFilter(options);
                    break;
                case FilterType::ChebyshevI:
                    setupChebyshevIFilter(options);
                    break;
                case FilterType::ChebyshevII:
                    setupChebyshevIIFilter(options);
                    break;
                case FilterType::RBJ:
                    // RBJ filters are only 2nd order, handled separately
                    break;
            }
        } catch (std::exception const & e) {
            throw std::runtime_error("Filter setup failed: " + std::string(e.what()));
        }
    }

    float filter(float input) {
        // Apply the active filter
        if (butterworth_lowpass) return static_cast<float>(butterworth_lowpass->filter(input));
        if (butterworth_highpass) return static_cast<float>(butterworth_highpass->filter(input));
        if (butterworth_bandpass) return static_cast<float>(butterworth_bandpass->filter(input));
        if (butterworth_bandstop) return static_cast<float>(butterworth_bandstop->filter(input));

        if (chebyshev1_lowpass) return static_cast<float>(chebyshev1_lowpass->filter(input));
        if (chebyshev1_highpass) return static_cast<float>(chebyshev1_highpass->filter(input));
        if (chebyshev1_bandpass) return static_cast<float>(chebyshev1_bandpass->filter(input));
        if (chebyshev1_bandstop) return static_cast<float>(chebyshev1_bandstop->filter(input));

        if (chebyshev2_lowpass) return static_cast<float>(chebyshev2_lowpass->filter(input));
        if (chebyshev2_highpass) return static_cast<float>(chebyshev2_highpass->filter(input));
        if (chebyshev2_bandpass) return static_cast<float>(chebyshev2_bandpass->filter(input));
        if (chebyshev2_bandstop) return static_cast<float>(chebyshev2_bandstop->filter(input));

        return input;// No filter active
    }

    void reset() {
        if (butterworth_lowpass) butterworth_lowpass->reset();
        if (butterworth_highpass) butterworth_highpass->reset();
        if (butterworth_bandpass) butterworth_bandpass->reset();
        if (butterworth_bandstop) butterworth_bandstop->reset();

        if (chebyshev1_lowpass) chebyshev1_lowpass->reset();
        if (chebyshev1_highpass) chebyshev1_highpass->reset();
        if (chebyshev1_bandpass) chebyshev1_bandpass->reset();
        if (chebyshev1_bandstop) chebyshev1_bandstop->reset();

        if (chebyshev2_lowpass) chebyshev2_lowpass->reset();
        if (chebyshev2_highpass) chebyshev2_highpass->reset();
        if (chebyshev2_bandpass) chebyshev2_bandpass->reset();
        if (chebyshev2_bandstop) chebyshev2_bandstop->reset();
    }

private:
    void setupButterworthFilter(FilterOptions const & options) {
        switch (options.response) {
            case FilterResponse::LowPass:
                butterworth_lowpass.emplace();
                butterworth_lowpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz);
                break;
            case FilterResponse::HighPass:
                butterworth_highpass.emplace();
                butterworth_highpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz);
                break;
            case FilterResponse::BandPass: {
                butterworth_bandpass.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs
                double center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth = options.high_cutoff_hz - options.cutoff_frequency_hz;
                butterworth_bandpass->setup(options.order, options.sampling_rate_hz, center_freq, bandwidth);
                break;
            }
            case FilterResponse::BandStop: {
                butterworth_bandstop.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs  
                double center_freq_stop = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth_stop = options.high_cutoff_hz - options.cutoff_frequency_hz;
                // Note: BandStop might not have the 4-parameter setup method, using 3-parameter version
                butterworth_bandstop->setup(options.sampling_rate_hz, center_freq_stop, bandwidth_stop);
                break;
            }
            default:
                throw std::runtime_error("Unsupported Butterworth filter response type");
        }
    }

    void setupChebyshevIFilter(FilterOptions const & options) {
        switch (options.response) {
            case FilterResponse::LowPass:
                chebyshev1_lowpass.emplace();
                chebyshev1_lowpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz, options.passband_ripple_db);
                break;
            case FilterResponse::HighPass:
                chebyshev1_highpass.emplace();
                chebyshev1_highpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz, options.passband_ripple_db);
                break;
            case FilterResponse::BandPass: {
                chebyshev1_bandpass.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs
                double center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth = options.high_cutoff_hz - options.cutoff_frequency_hz;
                chebyshev1_bandpass->setup(options.order, options.sampling_rate_hz, center_freq, bandwidth, options.passband_ripple_db);
                break;
            }
            case FilterResponse::BandStop: {
                chebyshev1_bandstop.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs
                double center_freq_stop = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth_stop = options.high_cutoff_hz - options.cutoff_frequency_hz;
                // Assume similar interface to BandPass
                chebyshev1_bandstop->setup(options.order, options.sampling_rate_hz, center_freq_stop, bandwidth_stop, options.passband_ripple_db);
                break;
            }
            default:
                throw std::runtime_error("Unsupported Chebyshev I filter response type");
        }
    }

    void setupChebyshevIIFilter(FilterOptions const & options) {
        switch (options.response) {
            case FilterResponse::LowPass:
                chebyshev2_lowpass.emplace();
                chebyshev2_lowpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz, options.stopband_ripple_db);
                break;
            case FilterResponse::HighPass:
                chebyshev2_highpass.emplace();
                chebyshev2_highpass->setup(options.order, options.sampling_rate_hz, options.cutoff_frequency_hz, options.stopband_ripple_db);
                break;
            case FilterResponse::BandPass: {
                chebyshev2_bandpass.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs
                double center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth = options.high_cutoff_hz - options.cutoff_frequency_hz;
                chebyshev2_bandpass->setup(options.order, options.sampling_rate_hz, center_freq, bandwidth, options.stopband_ripple_db);
                break;
            }
            case FilterResponse::BandStop: {
                chebyshev2_bandstop.emplace();
                // Calculate center frequency and bandwidth from low and high cutoffs
                double center_freq_stop = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                double bandwidth_stop = options.high_cutoff_hz - options.cutoff_frequency_hz;
                chebyshev2_bandstop->setup(options.order, options.sampling_rate_hz, center_freq_stop, bandwidth_stop, options.stopband_ripple_db);
                break;
            }
            default:
                throw std::runtime_error("Unsupported Chebyshev II filter response type");
        }
    }
};

// RBJ Filter (always 2nd order)
class RBJFilter {
private:
    std::optional<Iir::RBJ::LowPass> lowpass;
    std::optional<Iir::RBJ::HighPass> highpass;
    std::optional<Iir::RBJ::BandPass2> bandpass;
    std::optional<Iir::RBJ::BandStop> bandstop;

public:
    void setupFilter(FilterOptions const & options) {
        try {
            switch (options.response) {
                case FilterResponse::LowPass:
                    lowpass.emplace();
                    lowpass->setup(options.sampling_rate_hz, options.cutoff_frequency_hz, options.q_factor);
                    break;
                case FilterResponse::HighPass:
                    highpass.emplace();
                    highpass->setup(options.sampling_rate_hz, options.cutoff_frequency_hz, options.q_factor);
                    break;
                case FilterResponse::BandPass: {
                    bandpass.emplace();
                    if (options.high_cutoff_hz > options.cutoff_frequency_hz) {
                        // Calculate center frequency and bandwidth in octaves from low and high cutoffs
                        double center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                        double bandwidth_octaves = std::log2(options.high_cutoff_hz / options.cutoff_frequency_hz);
                        bandpass->setup(options.sampling_rate_hz, center_freq, bandwidth_octaves);
                    } else {
                        // Use center frequency and Q factor
                        // Convert Q factor to bandwidth in octaves: BW ≈ 1.44 / Q
                        double center_freq = options.cutoff_frequency_hz;
                        double bandwidth_octaves = 1.44 / options.q_factor;
                        bandpass->setup(options.sampling_rate_hz, center_freq, bandwidth_octaves);
                    }
                    break;
                }
                case FilterResponse::BandStop: {
                    bandstop.emplace();
                    if (options.high_cutoff_hz > options.cutoff_frequency_hz) {
                        // Calculate center frequency and bandwidth in octaves from low and high cutoffs
                        double center_freq = (options.cutoff_frequency_hz + options.high_cutoff_hz) / 2.0;
                        double bandwidth_octaves = std::log2(options.high_cutoff_hz / options.cutoff_frequency_hz);
                        bandstop->setup(options.sampling_rate_hz, center_freq, bandwidth_octaves);
                    } else {
                        // Notch filter: use center frequency and Q factor
                        // Convert Q factor to bandwidth in octaves: BW ≈ 1.44 / Q
                        double center_freq = options.cutoff_frequency_hz;
                        double bandwidth_octaves = 1.44 / options.q_factor;
                        bandstop->setup(options.sampling_rate_hz, center_freq, bandwidth_octaves);
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported RBJ filter response type");
            }
        } catch (std::exception const & e) {
            throw std::runtime_error("RBJ filter setup failed: " + std::string(e.what()));
        }
    }

    float filter(float input) {
        if (lowpass) return static_cast<float>(lowpass->filter(input));
        if (highpass) return static_cast<float>(highpass->filter(input));
        if (bandpass) return static_cast<float>(bandpass->filter(input));
        if (bandstop) return static_cast<float>(bandstop->filter(input));
        return input;
    }

    void reset() {
        if (lowpass) lowpass->reset();
        if (highpass) highpass->reset();
        if (bandpass) bandpass->reset();
        if (bandstop) bandstop->reset();
    }
};

// Generic filter interface
class IIRFilter {
private:
    FilterVariant<1> filter1;
    FilterVariant<2> filter2;
    FilterVariant<3> filter3;
    FilterVariant<4> filter4;
    FilterVariant<5> filter5;
    FilterVariant<6> filter6;
    FilterVariant<7> filter7;
    FilterVariant<8> filter8;
    RBJFilter rbj_filter;
    int active_order = 0;
    FilterType active_type = FilterType::Butterworth;

public:
    void setupFilter(FilterOptions const & options) {
        active_order = options.order;
        active_type = options.type;

        if (options.type == FilterType::RBJ) {
            rbj_filter.setupFilter(options);
            return;
        }

        // Setup the appropriate order filter
        switch (options.order) {
            case 1:
                filter1.setupFilter(options);
                break;
            case 2:
                filter2.setupFilter(options);
                break;
            case 3:
                filter3.setupFilter(options);
                break;
            case 4:
                filter4.setupFilter(options);
                break;
            case 5:
                filter5.setupFilter(options);
                break;
            case 6:
                filter6.setupFilter(options);
                break;
            case 7:
                filter7.setupFilter(options);
                break;
            case 8:
                filter8.setupFilter(options);
                break;
            default:
                throw std::runtime_error("Unsupported filter order: " + std::to_string(options.order));
        }
    }

    float filter(float input) {
        if (active_type == FilterType::RBJ) {
            return rbj_filter.filter(input);
        }

        switch (active_order) {
            case 1:
                return filter1.filter(input);
            case 2:
                return filter2.filter(input);
            case 3:
                return filter3.filter(input);
            case 4:
                return filter4.filter(input);
            case 5:
                return filter5.filter(input);
            case 6:
                return filter6.filter(input);
            case 7:
                return filter7.filter(input);
            case 8:
                return filter8.filter(input);
            default:
                return input;
        }
    }

    void reset() {
        if (active_type == FilterType::RBJ) {
            rbj_filter.reset();
            return;
        }

        switch (active_order) {
            case 1:
                filter1.reset();
                break;
            case 2:
                filter2.reset();
                break;
            case 3:
                filter3.reset();
                break;
            case 4:
                filter4.reset();
                break;
            case 5:
                filter5.reset();
                break;
            case 6:
                filter6.reset();
                break;
            case 7:
                filter7.reset();
                break;
            case 8:
                filter8.reset();
                break;
        }
    }
};

// Data interpolation for irregular sampling
std::vector<float> interpolateData(
        std::vector<float> const & data,
        std::vector<TimeFrameIndex> const & time_indices,
        InterpolationMethod method) {
    if (method == InterpolationMethod::None || data.size() != time_indices.size()) {
        return data;
    }

    // For now, return original data.
    // TODO: Implement interpolation for irregular sampling
    return data;
}

// Process data with potential gaps
struct DataSegment {
    std::vector<float> data;
    std::vector<TimeFrameIndex> time_indices;
    size_t start_idx;
    size_t end_idx;
};

std::vector<DataSegment> segmentData(
        std::vector<float> const & data,
        std::vector<TimeFrameIndex> const & time_indices,
        size_t max_gap_samples) {
    std::vector<DataSegment> segments;

    if (data.empty() || data.size() != time_indices.size()) {
        return segments;
    }

    size_t segment_start = 0;

    for (size_t i = 1; i < time_indices.size(); ++i) {
        int64_t gap = time_indices[i].getValue() - time_indices[i - 1].getValue();

        if (static_cast<size_t>(gap) > max_gap_samples) {
            // End current segment
            DataSegment segment;
            segment.start_idx = segment_start;
            segment.end_idx = i;
            segment.data.assign(data.begin() + segment_start, data.begin() + i);
            segment.time_indices.assign(time_indices.begin() + segment_start, time_indices.begin() + i);
            segments.push_back(segment);

            segment_start = i;
        }
    }

    // Add final segment
    if (segment_start < data.size()) {
        DataSegment segment;
        segment.start_idx = segment_start;
        segment.end_idx = data.size();
        segment.data.assign(data.begin() + segment_start, data.end());
        segment.time_indices.assign(time_indices.begin() + segment_start, time_indices.end());
        segments.push_back(segment);
    }

    return segments;
}

}// anonymous namespace

// ========== Main Filtering Functions ==========

FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time,
        FilterOptions const & options) {
    FilterResult result;

    // Validate inputs
    if (!analog_time_series) {
        result.error_message = "Input AnalogTimeSeries is null";
        return result;
    }

    if (!options.isValid()) {
        result.error_message = "Invalid filter options: " + options.getValidationError();
        return result;
    }

    try {
        // Extract data from the specified time range
        auto data_span = analog_time_series->getDataInTimeFrameIndexRange(start_time, end_time);
        auto time_value_range = analog_time_series->getTimeValueRangeInTimeFrameIndexRange(start_time, end_time);

        if (data_span.empty()) {
            result.error_message = "No data found in specified time range";
            return result;
        }

        // Convert span to vector for processing
        std::vector<float> input_data(data_span.begin(), data_span.end());
        std::vector<TimeFrameIndex> input_times;
        input_times.reserve(time_value_range.size());

        for (auto const & point: time_value_range) {
            input_times.push_back(point.time_frame_index);
        }

        // Handle irregular sampling if requested
        if (options.interpolation != InterpolationMethod::None) {
            input_data = interpolateData(input_data, input_times, options.interpolation);
        }

        // Segment data for gaps
        auto segments = segmentData(input_data, input_times, options.max_gap_samples);

        std::vector<float> filtered_data;
        std::vector<TimeFrameIndex> filtered_times;

        for (auto & segment: segments) {
            if (segment.data.size() < 2) {
                // Skip segments that are too small
                continue;
            }

            // Create and setup filter
            IIRFilter filter;
            filter.setupFilter(options);

            std::vector<float> segment_output;
            segment_output.reserve(segment.data.size());

            if (options.zero_phase) {
                // Forward pass
                std::vector<float> forward_output;
                forward_output.reserve(segment.data.size());

                filter.reset();
                for (float sample: segment.data) {
                    forward_output.push_back(filter.filter(sample));
                }

                // Backward pass
                filter.reset();
                segment_output.resize(segment.data.size());

                for (int i = static_cast<int>(forward_output.size()) - 1; i >= 0; --i) {
                    segment_output[i] = filter.filter(forward_output[i]);
                }

                // Reverse the result to correct time order
                std::reverse(segment_output.begin(), segment_output.end());
            } else {
                // Single forward pass
                filter.reset();
                for (float sample: segment.data) {
                    segment_output.push_back(filter.filter(sample));
                }
            }

            // Append segment results
            filtered_data.insert(filtered_data.end(), segment_output.begin(), segment_output.end());
            filtered_times.insert(filtered_times.end(), segment.time_indices.begin(), segment.time_indices.end());

            result.samples_processed += segment_output.size();
        }

        result.segments_processed = segments.size();

        if (filtered_data.empty()) {
            result.error_message = "No data could be processed";
            return result;
        }

        // Create new AnalogTimeSeries with filtered data
        result.filtered_data = std::make_shared<AnalogTimeSeries>(
                std::move(filtered_data),
                std::move(filtered_times));

        result.success = true;

    } catch (std::exception const & e) {
        result.error_message = "Filtering failed: " + std::string(e.what());
    }

    return result;
}

FilterResult filterAnalogTimeSeries(
        AnalogTimeSeries const * analog_time_series,
        FilterOptions const & options) {
    if (!analog_time_series) {
        FilterResult result;
        result.error_message = "Input AnalogTimeSeries is null";
        return result;
    }

    // Get the full time range
    auto time_series = analog_time_series->getTimeSeries();
    if (time_series.empty()) {
        FilterResult result;
        result.error_message = "AnalogTimeSeries contains no data";
        return result;
    }

    TimeFrameIndex start_time = time_series.front();
    TimeFrameIndex end_time = time_series.back();

    return filterAnalogTimeSeries(analog_time_series, start_time, end_time, options);
}

// ========== Filter Defaults ==========

namespace FilterDefaults {

FilterOptions lowpass(double cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::LowPass;
    options.cutoff_frequency_hz = cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions highpass(double cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::HighPass;
    options.cutoff_frequency_hz = cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions bandpass(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, int order) {
    FilterOptions options;
    options.type = FilterType::Butterworth;
    options.response = FilterResponse::BandPass;
    options.cutoff_frequency_hz = low_cutoff_hz;
    options.high_cutoff_hz = high_cutoff_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.order = order;
    return options;
}

FilterOptions notch(double center_freq_hz, double sampling_rate_hz, double q_factor) {
    FilterOptions options;
    options.type = FilterType::RBJ;
    options.response = FilterResponse::BandStop;
    options.cutoff_frequency_hz = center_freq_hz;
    options.sampling_rate_hz = sampling_rate_hz;
    options.q_factor = q_factor;
    options.order = 2;  // RBJ filters are always 2nd order
    return options;
}

}// namespace FilterDefaults