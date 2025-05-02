
#include "analog_interval_threshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams) {
    auto interval_series = std::make_shared<DigitalIntervalSeries>();

    float const threshold = thresholdParams.thresholdValue;
    double const minDuration = thresholdParams.minDuration;

    std::vector<Interval> intervals;

    auto const & timestamps = analog_time_series->getTimeSeries();
    auto const & values = analog_time_series->getAnalogTimeSeries();

    if (timestamps.empty()) {
        return interval_series;
    }

    // Variables to track interval state
    bool in_interval = false;
    int64_t interval_start = 0;
    double last_interval_end = -thresholdParams.lockoutTime - 1.0;// Initialize to allow first interval

    auto addIntervalIfValid = [&intervals, minDuration](int64_t start, int64_t end) {
        // Check if the interval meets the minimum duration requirement
        if ((end - start + 1) >= minDuration) {
            intervals.push_back({start, end});
        }
    };

    if (thresholdParams.direction == IntervalThresholdParams::ThresholdDirection::POSITIVE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (values[i] > threshold && !in_interval) {
                // Start of a new interval
                if (timestamps[i] - last_interval_end >= thresholdParams.lockoutTime) {
                    interval_start = static_cast<int64_t>(timestamps[i]);
                    in_interval = true;
                }
            } else if (values[i] <= threshold && in_interval) {
                // End of current interval
                addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps[i - 1]));
                last_interval_end = timestamps[i - 1];
                in_interval = false;
            }
        }
        // Handle case where signal is still above threshold at the end
        if (in_interval) {
            addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps.back()));
        }
    } else if (thresholdParams.direction == IntervalThresholdParams::ThresholdDirection::NEGATIVE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (values[i] < threshold && !in_interval) {
                // Start of a new interval
                if (timestamps[i] - last_interval_end >= thresholdParams.lockoutTime) {
                    interval_start = static_cast<int64_t>(timestamps[i]);
                    in_interval = true;
                }
            } else if (values[i] >= threshold && in_interval) {
                // End of current interval
                addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps[i-1]));
                last_interval_end = timestamps[i-1];
                in_interval = false;
            }
        }
        // Handle case where signal is still below threshold at the end
        if (in_interval) {
            addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps.back()));
        }
    } else if (thresholdParams.direction == IntervalThresholdParams::ThresholdDirection::ABSOLUTE) {
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (std::abs(values[i]) > threshold && !in_interval) {
                // Start of a new interval
                if (timestamps[i] - last_interval_end >= thresholdParams.lockoutTime) {
                    interval_start = static_cast<int64_t>(timestamps[i]);
                    in_interval = true;
                }
            } else if (std::abs(values[i]) <= threshold && in_interval) {
                // End of current interval
                addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps[i-1]));
                last_interval_end = timestamps[i-1];
                in_interval = false;
            }
        }
        // Handle case where signal is still above threshold at the end
        if (in_interval) {
            addIntervalIfValid(interval_start, static_cast<int64_t>(timestamps.back()));
        }
    } else {
        std::cerr << "Unknown threshold direction!" << std::endl;
    }

    interval_series->setData(intervals);

    return interval_series;
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

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant IntervalThresholdOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "IntervalThresholdOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    IntervalThresholdParams currentParams;

    if (transformParameters != nullptr) {
        IntervalThresholdParams const * specificParams = dynamic_cast<IntervalThresholdParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            std::cout << "Using parameters provided by UI." << std::endl;
        } else {
            std::cerr << "Warning: IntervalThresholdOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
        }
    } else {
        // No parameter object was provided (nullptr). This might be expected if the
        // operation can run with defaults or was called programmatically.
        std::cout << "ThresholdOperation received null parameters. Using default parameters." << std::endl;
        // Fall through to use the default 'currentParams'
    }

    std::shared_ptr<DigitalIntervalSeries> result_ts = interval_threshold(analog_raw_ptr,
                                                                          currentParams);

    if (!result_ts) {
        std::cerr << "IntervalThresholdOperation::execute: 'calculate_intervals' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "IntervalThresholdOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
