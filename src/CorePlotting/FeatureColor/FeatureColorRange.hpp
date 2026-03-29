/**
 * @file FeatureColorRange.hpp
 * @brief Compute effective vmin/vmax from resolved feature color values
 *
 * Supports Auto (data-driven), Manual (user-specified), and Symmetric modes
 * for determining the color range used by ContinuousMapping.
 */

#ifndef COREPLOTTING_FEATURECOLOR_FEATURECOLORRANGE_HPP
#define COREPLOTTING_FEATURECOLOR_FEATURECOLORRANGE_HPP

#include <optional>
#include <span>
#include <utility>

namespace CorePlotting::FeatureColor {

/**
 * @brief Color range mode
 */
enum class ColorRangeMode {
    Auto,    ///< vmin/vmax from data min/max
    Manual,  ///< User-specified vmin/vmax
    Symmetric///< Symmetric around zero: max(|min|, |max|) → [-v, +v]
};

/**
 * @brief Configuration for color range determination
 */
struct ColorRangeConfig {
    ColorRangeMode mode = ColorRangeMode::Auto;
    float manual_vmin = 0.0f;///< Used only in Manual mode
    float manual_vmax = 1.0f;///< Used only in Manual mode
};

/**
 * @brief Compute the effective vmin/vmax for a set of resolved float values
 *
 * @param resolved_values Per-point float values (nullopt entries are skipped)
 * @param config          Range mode and manual bounds
 * @return {vmin, vmax} pair, or nullopt if no valid values exist and mode is Auto/Symmetric
 */
[[nodiscard]] std::optional<std::pair<float, float>> computeEffectiveColorRange(
        std::span<std::optional<float> const> resolved_values,
        ColorRangeConfig const & config);

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_FEATURECOLORRANGE_HPP
