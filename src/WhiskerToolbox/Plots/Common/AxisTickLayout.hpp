#ifndef AXIS_TICK_LAYOUT_HPP
#define AXIS_TICK_LAYOUT_HPP

/**
 * @file AxisTickLayout.hpp
 * @brief Shared tick interval and position computation for axis/ruler widgets
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace Neuralyzer::Plots {

/**
 * @brief Tick spacing mode for axis widgets
 */
enum class AxisTickMode {
    Auto,  ///< Nice-number intervals based on visible range
    Fixed  ///< Fixed interval in coordinate units
};

/**
 * @brief Visual display mode for axis widgets
 */
enum class AxisDisplayMode {
    Plot,  ///< Standard plot axis with extent labels
    Ruler  ///< Slim Inkscape-style ruler without extent labels
};

/**
 * @brief Configuration for axis tick generation
 */
struct AxisTickConfig {
    AxisTickMode mode = AxisTickMode::Auto;
    int target_tick_count = 7;     ///< Auto mode: desired number of major tick intervals
    double fixed_interval = 100.0; ///< Fixed mode: spacing between ticks
    bool show_minor_ticks = true;  ///< When false, only major ticks and zero are drawn
};

/**
 * @brief Compute a human-friendly tick interval for a given range
 * @param range Total axis range (max - min)
 * @param target_tick_count Desired approximate number of tick intervals
 * @return Tick interval rounded to 1, 2, 5, or 10 × 10^n
 * @pre range > 0
 * @pre target_tick_count > 0
 */
[[nodiscard]] inline double computeNiceTickInterval(double range, int target_tick_count) {
    assert(range > 0.0);
    assert(target_tick_count > 0);

    double const target_ticks = static_cast<double>(target_tick_count);
    double const raw_interval = range / target_ticks;

    double const magnitude = std::pow(10.0, std::floor(std::log10(raw_interval)));
    double const normalized = raw_interval / magnitude;

    double nice = 10.0;
    if (normalized < 1.5) {
        nice = 1.0;
    } else if (normalized < 3.5) {
        nice = 2.0;
    } else if (normalized < 7.5) {
        nice = 5.0;
    }

    return nice * magnitude;
}

/**
 * @brief Resolve the tick interval from configuration and visible range
 * @param range Total axis range (max - min)
 * @param config Tick configuration
 * @return Tick interval in coordinate units
 * @pre range > 0
 */
[[nodiscard]] inline double computeTickInterval(double range, AxisTickConfig const & config) {
    if (config.mode == AxisTickMode::Fixed) {
        return std::max(config.fixed_interval, 1e-9);
    }
    return computeNiceTickInterval(range, config.target_tick_count);
}

/**
 * @brief Generate tick positions covering [min, max] at the configured interval
 * @param min Minimum visible coordinate
 * @param max Maximum visible coordinate
 * @param config Tick configuration
 * @return Sorted tick positions in ascending order
 * @pre max > min
 */
[[nodiscard]] inline std::vector<double> computeTickPositions(
        double min,
        double max,
        AxisTickConfig const & config) {
    assert(max > min);

    double const range = max - min;
    double const tick_interval = computeTickInterval(range, config);
    double const first_tick = std::ceil(min / tick_interval) * tick_interval;

    std::vector<double> positions;
    for (double v = first_tick; v <= max; v += tick_interval) {
        positions.push_back(v);
    }
    return positions;
}

/**
 * @brief Determine whether a tick value is a major tick
 * @param value Tick coordinate value
 * @param tick_interval Spacing between ticks
 * @return true if the tick should be drawn as major (including zero)
 */
[[nodiscard]] inline bool isMajorTick(double value, double tick_interval) {
    bool const is_zero = std::abs(value) < tick_interval * 0.01;
    bool const is_major = std::fmod(std::abs(value), tick_interval * 5.0) < tick_interval * 0.01;
    return is_major || is_zero;
}

/**
 * @brief Determine whether a tick should be drawn given minor-tick visibility
 * @param value Tick coordinate value
 * @param tick_interval Spacing between ticks
 * @param show_minor_ticks Whether minor ticks are enabled
 * @return true if the tick should be painted
 */
[[nodiscard]] inline bool shouldDrawTick(double value, double tick_interval, bool show_minor_ticks) {
    return show_minor_ticks || isMajorTick(value, tick_interval);
}

} // namespace Neuralyzer::Plots

#endif// AXIS_TICK_LAYOUT_HPP
