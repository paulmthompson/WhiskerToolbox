#include "AnalogIntervalThreshold.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

std::shared_ptr<DigitalIntervalSeries> analogIntervalThreshold(
        AnalogTimeSeries const & input,
        AnalogIntervalThresholdParams const & params,
        ComputeContext const & ctx) {

    // Validate parameters
    if (!params.isValidDirection()) {
        std::cerr << "analogIntervalThreshold: Invalid direction parameter: " 
                  << params.getDirection() << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    if (!params.isValidMissingDataMode()) {
        std::cerr << "analogIntervalThreshold: Invalid missing_data_mode parameter: " 
                  << params.getMissingDataMode() << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & timestamps = input.getTimeSeries();
    auto const & values = input.getAnalogTimeSeries();

    if (timestamps.empty()) {
        ctx.reportProgress(100);
        return std::make_shared<DigitalIntervalSeries>();
    }

    ctx.reportProgress(10);

    auto const threshold = params.getThresholdValue();
    double const minDuration = static_cast<double>(params.getMinDuration());
    double const lockoutTime = static_cast<double>(params.getLockoutTime());
    bool const treatMissingAsZero = params.treatMissingAsZero();
    std::string const direction = params.getDirection();
    
    std::vector<Interval> intervals;

    // Variables to track interval state
    bool in_interval = false;
    int64_t interval_start = 0;
    double last_interval_end = -lockoutTime - 1.0;  // Initialize to allow first interval
    int64_t last_valid_time = 0;  // Track the last time where we know the interval state

    auto addIntervalIfValid = [&intervals, minDuration](int64_t start, int64_t end) {
        // Check if the interval meets the minimum duration requirement
        if (static_cast<double>(end - start + 1) >= minDuration) {
            intervals.push_back({start, end});
        }
    };

    ctx.reportProgress(20);

    // Lambda to check if value meets threshold criteria
    auto meetsThreshold = [&direction, threshold](float value) -> bool {
        if (direction == "positive") {
            return value > threshold;
        } else if (direction == "negative") {
            return value < threshold;
        } else if (direction == "absolute") {
            return std::abs(value) > std::abs(threshold);
        }
        return false;
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
            size_t const second_step = static_cast<size_t>(timestamps[2].getValue() - timestamps[1].getValue());
            // If steps are very different, fall back to minimum step size
            if (second_step != typical_time_step) {
                typical_time_step = std::min(typical_time_step, second_step);
            }
        }
    }

    // Process the time series
    for (size_t i = 0; i < total_samples; ++i) {
        // Check for cancellation periodically
        if (i % 100 == 0 && ctx.shouldCancel()) {
            return std::make_shared<DigitalIntervalSeries>();
        }

        if (i % 1000 == 0) {
            int const progress = 20 + static_cast<int>((i * 70) / total_samples);
            ctx.reportProgress(progress);
        }

        // Initialize last_valid_time for first sample
        if (i == 0) {
            last_valid_time = timestamps[i].getValue();
        }

        // Handle missing data gaps if treating missing as zero
        if (i > 0 && treatMissingAsZero) {
            auto const prev_time = static_cast<size_t>(timestamps[i - 1].getValue());
            auto const curr_time = static_cast<size_t>(timestamps[i].getValue());
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
                    if (static_cast<double>(gap_start) - last_interval_end >= lockoutTime) {
                        interval_start = gap_start;
                        in_interval = true;
                    }
                }

                // If we're in an interval and zeros meet threshold, update last_valid_time to end of gap
                if (in_interval && zeroMeetsThreshold) {
                    last_valid_time = static_cast<int64_t>(curr_time) - static_cast<int64_t>(typical_time_step);
                } else {
                    last_valid_time = static_cast<int64_t>(prev_time);
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
            if (static_cast<double>(timestamps[i].getValue()) - last_interval_end >= lockoutTime) {
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

    ctx.reportProgress(100);

    return std::make_shared<DigitalIntervalSeries>(intervals);
}

}  // namespace WhiskerToolbox::Transforms::V2
