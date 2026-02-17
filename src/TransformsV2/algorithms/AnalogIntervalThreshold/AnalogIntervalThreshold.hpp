#ifndef WHISKERTOOLBOX_V2_ANALOG_INTERVAL_THRESHOLD_HPP
#define WHISKERTOOLBOX_V2_ANALOG_INTERVAL_THRESHOLD_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>

class AnalogTimeSeries;
class DigitalIntervalSeries;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for analog interval threshold detection
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * 
 * This transform detects intervals in an analog time series where the signal
 * meets specified threshold criteria. Unlike event threshold (which detects
 * individual crossing points), this produces continuous intervals.
 * 
 * Example JSON:
 * ```json
 * {
 *   "threshold_value": 1.0,
 *   "direction": "positive",
 *   "lockout_time": 0.0,
 *   "min_duration": 0.0,
 *   "missing_data_mode": "treat_as_zero"
 * }
 * ```
 */
struct AnalogIntervalThresholdParams {
    /// Threshold value for interval detection
    std::optional<float> threshold_value;

    /// Direction of threshold crossing: "positive", "negative", or "absolute"
    std::optional<std::string> direction;

    /// Lockout time (in same units as time series) after interval ends before new one can start
    /// Must be non-negative
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> lockout_time;

    /// Minimum duration for an interval to be valid (in same units as time series)
    /// Must be non-negative  
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> min_duration;

    /// How to handle missing data: "ignore" or "treat_as_zero"
    std::optional<std::string> missing_data_mode;

    // Helper methods to get values with defaults
    [[nodiscard]] float getThresholdValue() const {
        return threshold_value.value_or(1.0f);
    }

    [[nodiscard]] std::string getDirection() const {
        return direction.value_or("positive");
    }

    [[nodiscard]] float getLockoutTime() const {
        return lockout_time.has_value() ? lockout_time.value().value() : 0.0f;
    }

    [[nodiscard]] float getMinDuration() const {
        return min_duration.has_value() ? min_duration.value().value() : 0.0f;
    }

    [[nodiscard]] std::string getMissingDataMode() const {
        return missing_data_mode.value_or("treat_as_zero");
    }

    /// Validate direction string
    [[nodiscard]] bool isValidDirection() const {
        auto const dir = getDirection();
        return dir == "positive" || dir == "negative" || dir == "absolute";
    }

    /// Validate missing data mode string
    [[nodiscard]] bool isValidMissingDataMode() const {
        auto const mode = getMissingDataMode();
        return mode == "ignore" || mode == "treat_as_zero";
    }

    /// Check if missing data should be treated as zero
    [[nodiscard]] bool treatMissingAsZero() const {
        return getMissingDataMode() == "treat_as_zero";
    }
};

// ============================================================================
// Transform Implementation (Container Transform)
// ============================================================================

/**
 * @brief Detect intervals in analog signal based on threshold criteria
 * 
 * This is a **container-level transform** because:
 * - Requires temporal context to detect interval start/end
 * - Needs lockout period enforcement across time points
 * - Must handle missing data gaps based on sampling rate
 * - Produces intervals that span multiple time points
 * 
 * Algorithm:
 * 1. Iterate through time series samples
 * 2. Track when signal enters/exits threshold condition
 * 3. Apply lockout time after interval ends
 * 4. Filter intervals by minimum duration requirement
 * 5. Handle missing data gaps based on mode
 * 
 * @param input Input analog time series
 * @param params Threshold parameters (value, direction, lockout, min duration, missing data mode)
 * @param ctx Compute context for progress reporting and cancellation
 * @return Shared pointer to digital interval series containing detected intervals
 */
std::shared_ptr<DigitalIntervalSeries> analogIntervalThreshold(
        AnalogTimeSeries const & input,
        AnalogIntervalThresholdParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_ANALOG_INTERVAL_THRESHOLD_HPP
