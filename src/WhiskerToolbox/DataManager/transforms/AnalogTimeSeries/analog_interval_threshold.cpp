#include "analog_interval_threshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/interval_data.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
/**
     * @brief Internal implementation of interval threshold detection with optional progress reporting.
     * @param analog_time_series Input time series data
     * @param thresholdParams Parameters for the detection
     * @param progressCallback Optional progress callback (can be nullptr)
     * @return Shared pointer to the resulting interval series
     */
std::shared_ptr<DigitalIntervalSeries> interval_threshold_impl(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams,
        ProgressCallback progressCallback = nullptr) {

    auto interval_series = std::make_shared<DigitalIntervalSeries>();

    // Input validation
    if (!analog_time_series) {
        std::cerr << "interval_threshold: Input AnalogTimeSeries is null" << std::endl;
        return interval_series;
    }

    auto const & timestamps = analog_time_series->getTimeSeries();
    auto const & values = analog_time_series->getAnalogTimeSeries();

    if (timestamps.empty()) {
        std::cerr << "interval_threshold: Input time series is empty" << std::endl;
        return interval_series;
    }

    if (progressCallback) {
        progressCallback(10);
    }

    auto const threshold = static_cast<float>(thresholdParams.thresholdValue);
    double const minDuration = thresholdParams.minDuration;
    std::vector<Interval> intervals;

    // Variables to track interval state
    bool in_interval = false;
    int64_t interval_start = 0;
    double last_interval_end = -thresholdParams.lockoutTime - 1.0;// Initialize to allow first interval

    auto addIntervalIfValid = [&intervals, minDuration](int64_t start, int64_t end) {
        // Check if the interval meets the minimum duration requirement
        if (static_cast<double>(end - start + 1) >= minDuration) {
            intervals.push_back({start, end});
        }
    };

    if (progressCallback) {
        progressCallback(20);
    }

    // Lambda to check if value meets threshold criteria
    auto meetsThreshold = [&thresholdParams, threshold](float value) -> bool {
        switch (thresholdParams.direction) {
            case IntervalThresholdParams::ThresholdDirection::POSITIVE:
                return value > threshold;
            case IntervalThresholdParams::ThresholdDirection::NEGATIVE:
                return value < threshold;
            case IntervalThresholdParams::ThresholdDirection::ABSOLUTE:
                return std::abs(value) > threshold;
            default:
                return false;
        }
    };

    // Process the time series
    size_t const total_samples = timestamps.size();
    for (size_t i = 0; i < total_samples; ++i) {
        if (progressCallback && i % 1000 == 0) {
            int const progress = 20 + static_cast<int>((i * 70) / total_samples);
            progressCallback(progress);
        }

        bool const threshold_met = meetsThreshold(values[i]);

        if (threshold_met && !in_interval) {
            // Start of a new interval
            if (static_cast<double>(timestamps[i] - last_interval_end) >= thresholdParams.lockoutTime) {
                interval_start = static_cast<int64_t>(timestamps[i]);
                in_interval = true;
            }
        } else if (!threshold_met && in_interval) {
            // End of current interval
            int64_t const interval_end = (i > 0) ? static_cast<int64_t>(timestamps[i - 1]) : interval_start;
            addIntervalIfValid(interval_start, interval_end);
            last_interval_end = static_cast<double>(interval_end);
            in_interval = false;
        }
    }

    // Handle case where signal still meets threshold at the end
    if (in_interval) {
        addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps.back()));
    }

    if (progressCallback) {
        progressCallback(90);
    }

    interval_series->setData(intervals);

    if (progressCallback) {
        progressCallback(100);
    }

    return interval_series;
}
}// namespace

std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams) {
    return interval_threshold_impl(analog_time_series, thresholdParams);
}

std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams,
        ProgressCallback progressCallback) {
    return interval_threshold_impl(analog_time_series, thresholdParams, progressCallback);
}

std::string IntervalThresholdOperation::getName() const {
    return "Threshold Interval Detection";
}

std::type_index IntervalThresholdOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool IntervalThresholdOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant IntervalThresholdOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant IntervalThresholdOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "IntervalThresholdOperation::execute: Invalid input data variant" << std::endl;
        return {};
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    IntervalThresholdParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams =
                dynamic_cast<IntervalThresholdParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "IntervalThresholdOperation::execute: Incompatible parameter type, using defaults" << std::endl;
        }
    }

    std::shared_ptr<DigitalIntervalSeries> result = interval_threshold(
            analog_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "IntervalThresholdOperation::execute: Interval detection failed" << std::endl;
        return {};
    }

    return result;
}
