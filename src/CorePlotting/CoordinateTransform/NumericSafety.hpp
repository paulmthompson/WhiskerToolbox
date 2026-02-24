#ifndef COREPLOTTING_COORDINATETRANSFORM_NUMERICSAFETY_HPP
#define COREPLOTTING_COORDINATETRANSFORM_NUMERICSAFETY_HPP

/**
 * @file NumericSafety.hpp
 * @brief Numeric safety helpers for plot coordinate arithmetic.
 *
 * Provides overflow-safe arithmetic for coordinate computations where
 * subtraction of extreme (but finite) doubles can produce infinity.
 * yyjson (used by reflect-cpp) cannot serialize infinity/NaN, and
 * constructing std::string from the resulting null pointer crashes.
 *
 * Discovered by Phase 2 fuzz testing of LinePlotState / PSTHState.
 */

#include <algorithm>
#include <cmath>
#include <limits>

namespace CorePlotting {

/**
 * @brief Compute (max_val - min_val) without producing infinity.
 *
 * If subtracting two finite doubles overflows to ±infinity, returns
 * std::numeric_limits<double>::max() instead.  If either input is
 * already non-finite (NaN / inf), returns 0.0 as a safe fallback.
 *
 * @param min_val Lower bound
 * @param max_val Upper bound
 * @return The range, clamped to the finite double range
 */
[[nodiscard]] inline double safe_range(double min_val, double max_val)
{
    if (!std::isfinite(min_val) || !std::isfinite(max_val)) {
        return 0.0;
    }
    double const range = max_val - min_val;
    if (!std::isfinite(range)) {
        return std::numeric_limits<double>::max();
    }
    return range;
}

/**
 * @brief Clamp a value to the finite double range.
 *
 * NaN maps to 0.0.  ±infinity maps to ±DBL_MAX.
 *
 * @param v Input value
 * @return Finite value
 */
[[nodiscard]] inline double clamp_finite(double v)
{
    if (std::isnan(v)) {
        return 0.0;
    }
    return std::clamp(v,
                      std::numeric_limits<double>::lowest(),
                      std::numeric_limits<double>::max());
}

} // namespace CorePlotting

#endif // COREPLOTTING_COORDINATETRANSFORM_NUMERICSAFETY_HPP
