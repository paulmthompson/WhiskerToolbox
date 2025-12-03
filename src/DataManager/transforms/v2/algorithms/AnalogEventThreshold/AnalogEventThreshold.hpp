#ifndef WHISKERTOOLBOX_V2_ANALOG_EVENT_THRESHOLD_HPP
#define WHISKERTOOLBOX_V2_ANALOG_EVENT_THRESHOLD_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <vector>

class AnalogTimeSeries;
class DigitalEventSeries;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for analog event threshold detection
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * 
 * Example JSON:
 * ```json
 * {
 *   "threshold_value": 1.0,
 *   "direction": "positive",
 *   "lockout_time": 150.0
 * }
 * ```
 */
struct AnalogEventThresholdParams {
    // Threshold value for event detection
    std::optional<float> threshold_value;

    // Direction of threshold crossing: "positive", "negative", or "absolute"
    std::optional<std::string> direction;

    // Lockout time (in same units as time series) to prevent multiple detections
    // Must be non-negative
    std::optional<rfl::Validator<float, rfl::Minimum<0.0f>>> lockout_time;

    // Helper methods to get values with defaults
    float getThresholdValue() const {
        return threshold_value.value_or(1.0f);
    }

    std::string getDirection() const {
        return direction.value_or("positive");
    }

    float getLockoutTime() const {
        return lockout_time.has_value() ? lockout_time.value().value() : 0.0f;
    }

    // Validate direction string
    bool isValidDirection() const {
        auto dir = getDirection();
        return dir == "positive" || dir == "negative" || dir == "absolute";
    }
};

/**
 * @brief Detect threshold crossing events in an analog time series
 * 
 * This is a container-level transform because it has temporal dependencies:
 * the lockout period requires looking at previous samples to determine if
 * an event should be reported.
 * 
 * Container signature: AnalogTimeSeries → DigitalEventSeries
 * 
 * Algorithm:
 * 1. Iterate through time series samples
 * 2. Check if value crosses threshold (based on direction)
 * 3. If crossed and outside lockout period from last event, record event
 * 4. Report progress and check for cancellation
 * 
 * @param input Input analog time series
 * @param params Threshold parameters (value, direction, lockout)
 * @param ctx Compute context for progress/cancellation
 * @return Shared pointer to digital event series containing event times
 */
std::shared_ptr<DigitalEventSeries> analogEventThreshold(
        AnalogTimeSeries const & input,
        AnalogEventThresholdParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_ANALOG_EVENT_THRESHOLD_HPP
