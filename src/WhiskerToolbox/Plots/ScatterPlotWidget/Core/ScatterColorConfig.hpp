/**
 * @file ScatterColorConfig.hpp
 * @brief Serializable configuration for external feature-based point coloring
 *
 * ScatterColorConfigData describes how scatter points should be colored by an
 * external data source. It supports two mapping modes:
 * - Continuous: float values mapped through a colormap (Viridis, Inferno, etc.)
 * - Threshold: float values binarized at a cutoff into two colors
 *
 * The struct is designed for round-trip JSON serialization via reflect-cpp.
 * Enum values are stored as strings for human-readable JSON output.
 *
 * @see CorePlotting::FeatureColor::FeatureColorSourceDescriptor
 * @see CorePlotting::FeatureColor::ColorRangeConfig
 */

#ifndef SCATTER_COLOR_CONFIG_HPP
#define SCATTER_COLOR_CONFIG_HPP

#include "CorePlotting/FeatureColor/FeatureColorSourceDescriptor.hpp"

#include <optional>
#include <string>

/**
 * @brief Serializable configuration for external feature-based point coloring
 *
 * When @c color_mode is "fixed", points use the default GlyphStyleData color.
 * When "by_feature", points are colored by resolving @c color_source against
 * the DataManager and mapping the resulting float values according to
 * @c mapping_mode.
 */
struct ScatterColorConfigData {
    std::string color_mode = "fixed";///< "fixed" or "by_feature"

    /// Feature color source (only used when color_mode == "by_feature")
    std::optional<CorePlotting::FeatureColor::FeatureColorSourceDescriptor> color_source;

    std::string mapping_mode = "continuous";///< "continuous" or "threshold"

    // -- Continuous mode settings --
    std::string colormap_preset = "Viridis";///< Colormap name (matches ColormapPreset)
    std::string color_range_mode = "auto";  ///< "auto", "manual", or "symmetric"
    float color_range_vmin = 0.0f;          ///< Manual mode min value
    float color_range_vmax = 1.0f;          ///< Manual mode max value

    // -- Threshold mode settings --
    double threshold = 0.5;             ///< Binarization threshold
    std::string above_color = "#FF4444";///< Hex color for values >= threshold
    std::string below_color = "#3388FF";///< Hex color for values < threshold
    float above_alpha = 0.9f;           ///< Alpha for above-threshold points
    float below_alpha = 0.9f;           ///< Alpha for below-threshold points
};

#endif// SCATTER_COLOR_CONFIG_HPP
