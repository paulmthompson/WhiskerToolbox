#include "analog_interval_threshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>


std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams) {
    return interval_threshold(analog_time_series, thresholdParams, [](int) {});
}

std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!analog_time_series) {
        std::cerr << "interval_threshold: Input AnalogTimeSeries is null" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & timestamps = analog_time_series->getTimeSeries();
    auto const & values = analog_time_series->getAnalogTimeSeries();

    if (timestamps.empty()) {
        std::cerr << "interval_threshold: Input time series is empty" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
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
    int64_t last_valid_time = 0;                                  // Track the last time where we know the interval state

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

    // Check if zero meets threshold (for missing data handling)
    bool const zeroMeetsThreshold = meetsThreshold(0.0f);

    // Calculate typical time step to detect actual gaps (not just large regular intervals)
    size_t const total_samples = timestamps.size();
    size_t typical_time_step = 1;
    if (total_samples > 1) {
        // Use the first time difference as the expected step size
        typical_time_step = static_cast<size_t>(timestamps[1].getValue() - timestamps[0].getValue());

        // Validate this against a few more samples if available
        if (total_samples > 2) {
            size_t second_step = static_cast<size_t>(timestamps[2].getValue() - timestamps[1].getValue());
            // If steps are very different, fall back to minimum step size
            if (second_step != typical_time_step) {
                typical_time_step = std::min(typical_time_step, second_step);
            }
        }
    }

    // Process the time series
    for (size_t i = 0; i < total_samples; ++i) {
        if (progressCallback && i % 1000 == 0) {
            int const progress = 20 + static_cast<int>((i * 70) / total_samples);
            progressCallback(progress);
        }

        // Initialize last_valid_time for first sample
        if (i == 0) {
            last_valid_time = timestamps[i].getValue();
        }

        // Handle missing data gaps if treating missing as zero
        if (i > 0 && thresholdParams.missingDataMode == IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO) {
            size_t const prev_time = static_cast<size_t>(timestamps[i - 1].getValue());
            size_t const curr_time = static_cast<size_t>(timestamps[i].getValue());
            size_t const actual_step = curr_time - prev_time;

            // Check if there's a gap (more than 1.5x the typical time step)
            if (actual_step > (typical_time_step * 3 / 2)) {
                // There's a gap - handle missing data as zeros
                if (in_interval && !zeroMeetsThreshold) {
                    // We're in an interval but zeros don't meet threshold - end the interval at the gap start
                    auto const gap_start = static_cast<int64_t>(prev_time);
                    addIntervalIfValid(interval_start, gap_start);
                    last_interval_end = static_cast<double>(gap_start);
                    in_interval = false;
                } else if (!in_interval && zeroMeetsThreshold) {
                    // We're not in an interval but zeros meet threshold - start interval in the gap
                    auto const gap_start = static_cast<int64_t>(prev_time + typical_time_step);
                    if (static_cast<double>(gap_start) - last_interval_end >= thresholdParams.lockoutTime) {
                        interval_start = gap_start;
                        in_interval = true;
                    }
                }

                // If we're in an interval and zeros meet threshold, update last_valid_time to end of gap
                if (in_interval && zeroMeetsThreshold) {
                    last_valid_time = curr_time - typical_time_step;
                } else {
                    last_valid_time = prev_time;
                }
            } else {
                last_valid_time = static_cast<int64_t>(prev_time);
            }
        } else {
            if (i > 0) {
                last_valid_time = timestamps[i - 1].getValue();
            }
        }

        bool const threshold_met = meetsThreshold(values[i]);

        if (threshold_met && !in_interval) {
            // Start of a new interval
            if (static_cast<double>(timestamps[i].getValue()) - last_interval_end >= thresholdParams.lockoutTime) {
                interval_start = timestamps[i].getValue();
                in_interval = true;
            }
        } else if (!threshold_met && in_interval) {
            // End of current interval
            int64_t const interval_end = (i > 0) ? last_valid_time : interval_start;
            addIntervalIfValid(interval_start, interval_end);
            last_interval_end = static_cast<double>(interval_end);
            in_interval = false;
        }

        // Update last_valid_time to current timestamp
        last_valid_time = timestamps[i].getValue();
    }

    // Handle case where signal still meets threshold at the end
    if (in_interval) {
        addIntervalIfValid(interval_start, timestamps.back().getValue());
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return std::make_shared<DigitalIntervalSeries>(intervals);
}

///////////////////////////////////////////////////////////////////////////////

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
