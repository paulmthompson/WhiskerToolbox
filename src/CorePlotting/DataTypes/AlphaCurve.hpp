#ifndef COREPLOTTING_DATATYPES_ALPHACURVE_HPP
#define COREPLOTTING_DATATYPES_ALPHACURVE_HPP

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace CorePlotting {

/**
 * @brief Alpha curve types for temporal distance → opacity mapping
 *
 * Controls how opacity falls off with temporal distance from the
 * center of an onion-skin window.
 */
enum class AlphaCurve {
    Linear,       ///< Linear falloff: alpha = max - (max-min) * (d / half_width)
    Exponential,  ///< Exponential falloff: faster fade near edges
    Gaussian      ///< Gaussian falloff: smooth bell-curve fade
};

/**
 * @brief Convert AlphaCurve enum to string for serialization
 */
[[nodiscard]] inline std::string alphaCurveToString(AlphaCurve curve) {
    switch (curve) {
        case AlphaCurve::Linear: return "linear";
        case AlphaCurve::Exponential: return "exponential";
        case AlphaCurve::Gaussian: return "gaussian";
    }
    return "linear";
}

/**
 * @brief Parse AlphaCurve from string for deserialization
 * @param str String representation ("linear", "exponential", "gaussian")
 * @return Corresponding AlphaCurve enum value (defaults to Linear for unknown strings)
 */
[[nodiscard]] inline AlphaCurve alphaCurveFromString(std::string const & str) {
    if (str == "exponential") return AlphaCurve::Exponential;
    if (str == "gaussian") return AlphaCurve::Gaussian;
    return AlphaCurve::Linear;
}

/**
 * @brief Compute alpha (opacity) value based on temporal distance
 *
 * Maps an absolute temporal distance to an alpha value in [min_alpha, max_alpha].
 * At distance == 0 (center of window), returns max_alpha.
 * At distance >= half_width (edge of window), returns min_alpha.
 *
 * @param distance Absolute temporal distance from window center (non-negative)
 * @param half_width Half-width of the temporal window (positive).
 *                   Elements at exactly half_width get min_alpha.
 * @param curve Type of interpolation curve
 * @param min_alpha Minimum alpha at the window edge [0, 1]
 * @param max_alpha Maximum alpha at the window center [0, 1]
 * @return Alpha value in [min_alpha, max_alpha]
 *
 * @note If distance > half_width, returns min_alpha (clipped)
 * @note If half_width <= 0, returns max_alpha
 */
[[nodiscard]] inline float computeTemporalAlpha(
    int distance,
    int half_width,
    AlphaCurve curve = AlphaCurve::Linear,
    float min_alpha = 0.1f,
    float max_alpha = 1.0f
) {
    // Edge cases
    if (half_width <= 0) {
        return max_alpha;
    }

    int const abs_dist = std::abs(distance);
    if (abs_dist >= half_width) {
        return min_alpha;
    }

    // Normalized position [0, 1] where 0 = center, 1 = edge
    float const t = static_cast<float>(abs_dist) / static_cast<float>(half_width);
    float const alpha_range = max_alpha - min_alpha;

    float blend = 0.0f;

    switch (curve) {
        case AlphaCurve::Linear:
            // Linear: blend goes 0 → 1 as t goes 0 → 1
            blend = t;
            break;

        case AlphaCurve::Exponential:
            // Exponential: faster falloff near center, slower at edges
            // Using (e^(t*3) - 1) / (e^3 - 1) for a reasonable curve
            blend = (std::exp(t * 3.0f) - 1.0f) / (std::exp(3.0f) - 1.0f);
            break;

        case AlphaCurve::Gaussian:
            // Gaussian: slow falloff near center, accelerates toward edges
            // Using 1 - exp(-t^2 * 3) for a smooth bell curve
            blend = 1.0f - std::exp(-t * t * 3.0f);
            break;
    }

    // Map blend [0,1] → alpha [max_alpha, min_alpha]
    return max_alpha - alpha_range * blend;
}

/**
 * @brief Convenience overload using float distance (for non-integer time bases)
 */
[[nodiscard]] inline float computeTemporalAlpha(
    float distance,
    float half_width,
    AlphaCurve curve = AlphaCurve::Linear,
    float min_alpha = 0.1f,
    float max_alpha = 1.0f
) {
    if (half_width <= 0.0f) {
        return max_alpha;
    }

    float const abs_dist = std::abs(distance);
    if (abs_dist >= half_width) {
        return min_alpha;
    }

    float const t = abs_dist / half_width;
    float const alpha_range = max_alpha - min_alpha;

    float blend = 0.0f;

    switch (curve) {
        case AlphaCurve::Linear:
            blend = t;
            break;

        case AlphaCurve::Exponential:
            blend = (std::exp(t * 3.0f) - 1.0f) / (std::exp(3.0f) - 1.0f);
            break;

        case AlphaCurve::Gaussian:
            blend = 1.0f - std::exp(-t * t * 3.0f);
            break;
    }

    return max_alpha - alpha_range * blend;
}

} // namespace CorePlotting

#endif // COREPLOTTING_DATATYPES_ALPHACURVE_HPP
