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
    enum class Direction { positive, negative, absolute };

    // Threshold value for event detection
    float threshold_value = 1.0f;

    // Direction of threshold crossing
    Direction direction = Direction::positive;

    // Lockout time (in same units as time series) to prevent multiple detections
    // Must be non-negative
    rfl::Validator<float, rfl::Minimum<0.0f>> lockout_time = 0.0f;
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
