#ifndef NEURALYZER_V2_ANALOG_INTERVAL_THRESHOLD_HPP
#define NEURALYZER_V2_ANALOG_INTERVAL_THRESHOLD_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>

class AnalogTimeSeries;
class DigitalIntervalSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2 {

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
    enum class Direction {
        positive,
        negative,
        absolute
    };

    enum class MissingDataMode {
        ignore,
        treat_as_zero
    };

    /// Threshold value for interval detection
    float threshold_value = 1.0f;

    /// Direction of threshold crossing
    Direction direction = Direction::positive;

    /// Lockout time (in same units as time series) after interval ends before new one can start
    /// Must be non-negative
    rfl::Validator<float, rfl::Minimum<0.0f>> lockout_time = 0.0f;

    /// Minimum duration for an interval to be valid (in same units as time series)
    /// Must be non-negative
    rfl::Validator<float, rfl::Minimum<0.0f>> min_duration = 0.0f;

    /// How to handle missing data gaps in the time series
    MissingDataMode missing_data_mode = MissingDataMode::treat_as_zero;
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

}// namespace Neuralyzer::Transforms::V2

#endif// NEURALYZER_V2_ANALOG_INTERVAL_THRESHOLD_HPP
