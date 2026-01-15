#ifndef WHISKERTOOLBOX_V2_VALUE_RANGE_REDUCTIONS_HPP
#define WHISKERTOOLBOX_V2_VALUE_RANGE_REDUCTIONS_HPP

/**
 * @file ValueRangeReductions.hpp
 * @brief Range reductions for value-based time series (AnalogTimeSeries)
 *
 * These reductions consume a range of value elements and produce a scalar.
 * They are designed for trial-aligned analysis where each trial's analog
 * data needs to be summarized (e.g., max value, time of peak).
 *
 * ## Element Requirements
 *
 * Input elements must satisfy ValueElement concept:
 * - `time()` → returns time (TimeFrameIndex or numeric)
 * - `value()` → returns the sample value (typically float)
 *
 * ## Usage with GatherResult
 *
 * ```cpp
 * auto behavior_gather = gather(behavior_series, trials);
 *
 * // Sort trials by peak behavior value
 * auto max_values = behavior_gather.reducePipeline<TimeValuePoint, float>(
 *     TransformPipeline()
 *         .addRangeReduction("MaxValue", NoReductionParams{}));
 * ```
 *
 * @see RangeReductionTypes.hpp for concepts
 * @see AnalogTimeSeries::TimeValuePoint for the primary value element type
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <cmath>
#include <limits>
#include <span>

namespace WhiskerToolbox::Transforms::V2::RangeReductions {

// ============================================================================
// Parameter Types
// ============================================================================

/**
 * @brief Parameters for threshold crossing detection
 *
 * Used by TimeOfThresholdCross to find when a signal crosses a threshold.
 */
struct ThresholdCrossParams {
    /// Threshold value to cross
    float threshold{0.0f};

    /// Direction of crossing: true = rising (low to high), false = falling
    bool rising{true};
};

/**
 * @brief Parameters for percentile-based operations
 */
struct PercentileParams {
    /// Percentile to compute (0.0 to 100.0)
    float percentile{50.0f};
};

// ============================================================================
// Reduction Functions
// ============================================================================

/**
 * @brief Find maximum value in range
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Maximum value, or -infinity if empty
 */
template<typename Element>
[[nodiscard]] inline float maxValue(std::span<Element const> points) {
    if (points.empty()) {
        return -std::numeric_limits<float>::infinity();
    }

    float max_val = -std::numeric_limits<float>::infinity();
    for (auto const & p : points) {
        float val = static_cast<float>(p.value());
        if (val > max_val) {
            max_val = val;
        }
    }
    return max_val;
}

/**
 * @brief Find minimum value in range
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Minimum value, or +infinity if empty
 */
template<typename Element>
[[nodiscard]] inline float minValue(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::infinity();
    }

    float min_val = std::numeric_limits<float>::infinity();
    for (auto const & p : points) {
        float val = static_cast<float>(p.value());
        if (val < min_val) {
            min_val = val;
        }
    }
    return min_val;
}

/**
 * @brief Compute mean value in range
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Mean value, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float meanValue(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    double sum = 0.0;
    for (auto const & p : points) {
        sum += static_cast<double>(p.value());
    }
    return static_cast<float>(sum / static_cast<double>(points.size()));
}

/**
 * @brief Compute standard deviation of values in range
 *
 * Uses Welford's online algorithm for numerical stability.
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Population standard deviation, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float stdValue(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (points.size() == 1) {
        return 0.0f;
    }

    // Welford's algorithm for numerical stability
    double mean = 0.0;
    double M2 = 0.0;
    size_t n = 0;

    for (auto const & p : points) {
        ++n;
        double val = static_cast<double>(p.value());
        double delta = val - mean;
        mean += delta / static_cast<double>(n);
        double delta2 = val - mean;
        M2 += delta * delta2;
    }

    return static_cast<float>(std::sqrt(M2 / static_cast<double>(n)));
}

/**
 * @brief Find time at which maximum value occurs
 *
 * @tparam Element Value element type (must have time() and value() accessors)
 * @param points Range of value points
 * @return Time of maximum value, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float timeOfMax(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float max_val = -std::numeric_limits<float>::infinity();
    float max_time = std::numeric_limits<float>::quiet_NaN();

    for (auto const & p : points) {
        float val = static_cast<float>(p.value());
        if (val > max_val) {
            max_val = val;
            max_time = static_cast<float>(p.time().getValue());
        }
    }
    return max_time;
}

/**
 * @brief Find time at which minimum value occurs
 *
 * @tparam Element Value element type (must have time() and value() accessors)
 * @param points Range of value points
 * @return Time of minimum value, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float timeOfMin(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float min_val = std::numeric_limits<float>::infinity();
    float min_time = std::numeric_limits<float>::quiet_NaN();

    for (auto const & p : points) {
        float val = static_cast<float>(p.value());
        if (val < min_val) {
            min_val = val;
            min_time = static_cast<float>(p.time().getValue());
        }
    }
    return min_time;
}

/**
 * @brief Find first time when value crosses threshold
 *
 * Detects the first crossing point in the specified direction.
 * A crossing occurs when:
 * - Rising: previous value < threshold AND current value >= threshold
 * - Falling: previous value >= threshold AND current value < threshold
 *
 * @tparam Element Value element type (must have time() and value() accessors)
 * @param points Range of value points (should be sorted by time)
 * @param params Threshold and direction parameters
 * @return Time of first crossing, or NaN if no crossing found
 */
template<typename Element>
[[nodiscard]] inline float timeOfThresholdCross(
        std::span<Element const> points,
        ThresholdCrossParams const & params) {
    if (points.size() < 2) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    for (size_t i = 1; i < points.size(); ++i) {
        float prev_val = static_cast<float>(points[i - 1].value());
        float curr_val = static_cast<float>(points[i].value());

        bool crossed = false;
        if (params.rising) {
            crossed = (prev_val < params.threshold) && (curr_val >= params.threshold);
        } else {
            crossed = (prev_val >= params.threshold) && (curr_val < params.threshold);
        }

        if (crossed) {
            return static_cast<float>(points[i].time().getValue());
        }
    }

    return std::numeric_limits<float>::quiet_NaN();
}

/**
 * @brief Compute sum of all values in range
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Sum of values, or 0 if empty
 */
template<typename Element>
[[nodiscard]] inline float sumValue(std::span<Element const> points) {
    double sum = 0.0;
    for (auto const & p : points) {
        sum += static_cast<double>(p.value());
    }
    return static_cast<float>(sum);
}

/**
 * @brief Compute value range (max - min)
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @return Range of values, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float valueRange(std::span<Element const> points) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float min_val = std::numeric_limits<float>::infinity();
    float max_val = -std::numeric_limits<float>::infinity();

    for (auto const & p : points) {
        float val = static_cast<float>(p.value());
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    return max_val - min_val;
}

/**
 * @brief Compute area under curve (trapezoidal integration)
 *
 * Approximates the integral using the trapezoidal rule.
 * Assumes time units are consistent with the desired area units.
 *
 * @tparam Element Value element type (must have time() and value() accessors)
 * @param points Range of value points (should be sorted by time)
 * @return Area under curve, or 0 if fewer than 2 points
 */
template<typename Element>
[[nodiscard]] inline float areaUnderCurve(std::span<Element const> points) {
    if (points.size() < 2) {
        return 0.0f;
    }

    double area = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        double t1 = static_cast<double>(points[i - 1].time().getValue());
        double t2 = static_cast<double>(points[i].time().getValue());
        double v1 = static_cast<double>(points[i - 1].value());
        double v2 = static_cast<double>(points[i].value());

        // Trapezoidal rule: (v1 + v2) / 2 * (t2 - t1)
        area += (v1 + v2) * 0.5 * (t2 - t1);
    }

    return static_cast<float>(area);
}

/**
 * @brief Count samples above a threshold
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @param params Threshold parameters (only threshold is used, not direction)
 * @return Number of samples with value > threshold
 */
template<typename Element>
[[nodiscard]] inline int countAboveThreshold(
        std::span<Element const> points,
        ThresholdCrossParams const & params) {
    int count = 0;
    for (auto const & p : points) {
        if (static_cast<float>(p.value()) > params.threshold) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Compute fraction of samples above a threshold
 *
 * @tparam Element Value element type (must have value() accessor)
 * @param points Range of value points
 * @param params Threshold parameters
 * @return Fraction (0.0 to 1.0) of samples above threshold, or NaN if empty
 */
template<typename Element>
[[nodiscard]] inline float fractionAboveThreshold(
        std::span<Element const> points,
        ThresholdCrossParams const & params) {
    if (points.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    int count = countAboveThreshold(points, params);
    return static_cast<float>(count) / static_cast<float>(points.size());
}

}// namespace WhiskerToolbox::Transforms::V2::RangeReductions

#endif// WHISKERTOOLBOX_V2_VALUE_RANGE_REDUCTIONS_HPP
