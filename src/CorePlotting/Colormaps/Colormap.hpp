#ifndef COREPLOTTING_COLORMAPS_COLORMAP_HPP
#define COREPLOTTING_COLORMAPS_COLORMAP_HPP

/**
 * @file Colormap.hpp
 * @brief Reusable colormap system for mapping scalar values to RGBA colors
 *
 * Provides a set of perceptually uniform colormaps (Inferno, Viridis, Magma,
 * Plasma) and utility colormaps (Coolwarm, Grayscale, Hot) for mapping scalar
 * values to colors. Designed for use by any widget that renders scalar fields:
 *
 * - **HeatmapWidget**: firing rate matrix → colored heatmap cells
 * - **SpectrogramWidget**: power spectrum matrix → colored spectrogram cells
 * - **LinePlotWidget / EventPlotWidget**: external feature values → line/glyph colors
 *
 * ## Usage
 *
 * @code
 * using namespace CorePlotting::Colormaps;
 *
 * // Get a colormap function
 * auto cmap = getColormap(ColormapPreset::Inferno);
 *
 * // Map a single value (input in [0,1])
 * glm::vec4 color = cmap(0.5f);
 *
 * // Map a value with explicit range
 * glm::vec4 color = mapValue(cmap, 42.0f, 0.0f, 100.0f);
 *
 * // Map an entire matrix row-major to RGBA
 * auto colors = mapMatrix(cmap, values, num_rows, num_cols, vmin, vmax);
 * @endcode
 *
 * ## Design notes
 *
 * - **No Qt dependency**: pure C++23, usable from any rendering backend.
 * - **LUT-based**: each colormap is a 256-entry lookup table with linear
 *   interpolation, giving O(1) evaluation per value.
 * - **Extensible**: add new colormaps by providing a 256-entry LUT array.
 *
 * @see HeatmapWidget — primary consumer (rate matrix → heatmap)
 * @see SpectrogramWidget — planned consumer (power matrix → spectrogram)
 */

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace CorePlotting::Colormaps {

// =============================================================================
// Types
// =============================================================================

/// A colormap is a function from [0, 1] → RGBA
using ColormapFunction = std::function<glm::vec4(float t)>;

/// Number of entries in a colormap lookup table
inline constexpr std::size_t LUT_SIZE = 256;

/// A lookup table of RGB values (alpha is always 1.0)
using ColormapLUT = std::array<glm::vec3, LUT_SIZE>;

// =============================================================================
// Presets
// =============================================================================

/**
 * @brief Available colormap presets
 *
 * Perceptually uniform sequential colormaps (Inferno, Viridis, Magma, Plasma)
 * are from the matplotlib collection and are designed for accurate perception
 * of magnitude differences. Coolwarm is a diverging colormap centered at white,
 * suitable for signed data (e.g., z-scores).
 */
enum class ColormapPreset {
    Inferno,    ///< Dark-to-bright, perceptually uniform (default for heatmaps)
    Viridis,    ///< Blue-green-yellow, perceptually uniform (default for spectrograms)
    Magma,      ///< Dark-to-pink, perceptually uniform
    Plasma,     ///< Blue-to-yellow, perceptually uniform
    Coolwarm,   ///< Diverging blue-white-red (for z-scores)
    Grayscale,  ///< Simple black-to-white
    Hot,        ///< Black-red-yellow-white (high contrast)
};

/**
 * @brief Get a human-readable name for a colormap preset
 * @param preset The colormap preset
 * @return Name string (e.g. "Inferno", "Viridis")
 */
[[nodiscard]] std::string presetName(ColormapPreset preset);

/**
 * @brief Get all available colormap presets
 * @return Vector of all preset enum values
 */
[[nodiscard]] std::vector<ColormapPreset> allPresets();

// =============================================================================
// Colormap construction
// =============================================================================

/**
 * @brief Get a ColormapFunction for the given preset
 *
 * The returned function maps t ∈ [0,1] → RGBA using LUT interpolation.
 * Values outside [0,1] are clamped.
 *
 * @param preset Colormap preset to use
 * @return Callable that maps [0,1] → glm::vec4 (RGBA, alpha=1)
 */
[[nodiscard]] ColormapFunction getColormap(ColormapPreset preset);

/**
 * @brief Create a ColormapFunction from a custom LUT
 *
 * @param lut 256-entry RGB lookup table
 * @return Callable that maps [0,1] → glm::vec4 (RGBA, alpha=1)
 */
[[nodiscard]] ColormapFunction fromLUT(ColormapLUT const & lut);

// =============================================================================
// Mapping utilities
// =============================================================================

/**
 * @brief Map a scalar value through a colormap with explicit range
 *
 * Normalises `value` to [0,1] using `(value - vmin) / (vmax - vmin)`,
 * clamps the result, and evaluates the colormap.
 *
 * @param cmap  Colormap function
 * @param value Scalar value to map
 * @param vmin  Minimum of the data range
 * @param vmax  Maximum of the data range
 * @return RGBA color
 */
[[nodiscard]] glm::vec4 mapValue(
    ColormapFunction const & cmap,
    float value,
    float vmin,
    float vmax);

/**
 * @brief Map a flat array of scalar values to RGBA colors
 *
 * @param cmap   Colormap function
 * @param values Flat array of scalar values (row-major if matrix)
 * @param vmin   Minimum of the data range
 * @param vmax   Maximum of the data range
 * @return Vector of RGBA colors, same length as `values`
 */
[[nodiscard]] std::vector<glm::vec4> mapValues(
    ColormapFunction const & cmap,
    std::span<double const> values,
    float vmin,
    float vmax);

/**
 * @brief Map a row-major matrix of scalar values to RGBA colors
 *
 * Convenience wrapper around mapValues() — the matrix is treated as a flat
 * array. The row/column parameters are provided for documentation clarity
 * and future validation.
 *
 * @param cmap     Colormap function
 * @param values   Flat row-major matrix of values (size = num_rows × num_cols)
 * @param num_rows Number of rows
 * @param num_cols Number of columns
 * @param vmin     Minimum of the data range
 * @param vmax     Maximum of the data range
 * @return Vector of RGBA colors, size = num_rows × num_cols
 */
[[nodiscard]] std::vector<glm::vec4> mapMatrix(
    ColormapFunction const & cmap,
    std::span<double const> values,
    std::size_t num_rows,
    std::size_t num_cols,
    float vmin,
    float vmax);

// =============================================================================
// LUT access (for custom rendering or preview)
// =============================================================================

/**
 * @brief Get the raw LUT for a preset
 *
 * Useful for rendering colorbar previews or building custom textures.
 *
 * @param preset Colormap preset
 * @return Reference to the static LUT data
 */
[[nodiscard]] ColormapLUT const & getLUT(ColormapPreset preset);

} // namespace CorePlotting::Colormaps

#endif // COREPLOTTING_COLORMAPS_COLORMAP_HPP
