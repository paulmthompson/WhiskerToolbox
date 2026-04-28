/**
 * @file FeatureColorMapping.hpp
 * @brief Mapping strategies for converting resolved float values to per-point colors
 *
 * Defines two mapping modes:
 * - ContinuousMapping: float → colormap(t) with vmin/vmax normalization
 * - ThresholdMapping: float >= threshold → above_color, else below_color
 *
 * These are combined into a FeatureColorMapping variant consumed by applyFeatureColorsToScene().
 */

#ifndef COREPLOTTING_FEATURECOLOR_FEATURECOLORMAPPING_HPP
#define COREPLOTTING_FEATURECOLOR_FEATURECOLORMAPPING_HPP

#include "CorePlotting/Colormaps/Colormap.hpp"

#include <glm/glm.hpp>

#include <variant>

namespace CorePlotting::FeatureColor {

/**
 * @brief Continuous colormap from float values to colors with vmin/vmax normalization
 *
 * Values are clamped to [vmin, vmax] then mapped through a colormap preset.
 */
struct ContinuousMapping {
    Colormaps::ColormapPreset preset = Colormaps::ColormapPreset::Viridis;
    float vmin = 0.0f;
    float vmax = 1.0f;
};

/**
 * @brief Threshold (binary) mapping from float values to colors
 *
 * Values are compared against a threshold, if float >= threshold then above_color, else below_color.
 *
 * Useful for:
 * - DigitalIntervalSeries (already 0/1, threshold=0.5)
 * - Continuous data with a meaningful cutoff (e.g., "highlight velocity > 20 cm/s")
 * - TensorData columns with binary labels (0/1 classifier output)
 */
struct ThresholdMapping {
    float threshold = 0.5f;
    glm::vec4 above_color{1.0f, 0.27f, 0.27f, 0.9f};///< Red-ish default
    glm::vec4 below_color{0.2f, 0.53f, 1.0f, 0.9f}; ///< Blue-ish default
};

/**
 * @brief Variant holding one of the supported float→color mapping strategies
 */
using FeatureColorMapping = std::variant<ContinuousMapping, ThresholdMapping>;

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_FEATURECOLORMAPPING_HPP
