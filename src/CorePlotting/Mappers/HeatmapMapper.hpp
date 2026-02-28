#ifndef COREPLOTTING_MAPPERS_HEATMAPMAPPER_HPP
#define COREPLOTTING_MAPPERS_HEATMAPMAPPER_HPP

/**
 * @file HeatmapMapper.hpp
 * @brief Converts a rows × bins grid of scalar values into a colored rectangle batch
 *
 * Provides a generic grid-to-rectangle-batch mapper suitable for any widget that
 * renders a 2D scalar field as colored cells:
 *
 * - **HeatmapWidget**: each row is a unit's firing rate profile over time bins
 * - **SpectrogramWidget**: each row is a frequency bin's power over time bins
 *
 * The mapper is agnostic to the data source — it works with `HeatmapRowData`,
 * a lightweight struct describing one row of scalar values with X-axis metadata.
 * The calling widget is responsible for converting its domain-specific data
 * (e.g., `RateProfile` from EventRateEstimation) into `HeatmapRowData`.
 *
 * ## Usage
 *
 * @code
 * using namespace CorePlotting;
 *
 * // Build row data from rate profiles (done by the widget)
 * std::vector<HeatmapRowData> rows;
 * for (auto const & profile : rate_profiles) {
 *     rows.push_back({profile.counts, profile.bin_start, profile.bin_width});
 * }
 *
 * auto batch = HeatmapMapper::toRectangleBatch(
 *     rows,
 *     Colormaps::getColormap(Colormaps::ColormapPreset::Inferno));
 * @endcode
 *
 * @see Colormaps — provides the colormap functions
 * @see HeatmapOpenGLWidget — primary consumer (firing rate heatmap)
 * @see SpectrogramOpenGLWidget — planned consumer (power spectrogram)
 */

#include "Colormaps/Colormap.hpp"
#include "SceneGraph/RenderablePrimitives.hpp"

#include <cstddef>
#include <vector>

namespace CorePlotting {

// =============================================================================
// Row data (input to mapper)
// =============================================================================

/**
 * @brief One row of scalar values for the heatmap grid
 *
 * Describes a horizontal strip of the heatmap. The widget converts its
 * domain-specific data (e.g., `RateProfile`) into this lightweight struct.
 */
struct HeatmapRowData {
    std::vector<double> values;  ///< Per-bin scalar values
    double bin_start = 0.0;      ///< X position of the first bin's left edge
    double bin_width = 1.0;      ///< Width of each bin (uniform)
};

// =============================================================================
// Color range configuration
// =============================================================================

/**
 * @brief Configuration for how the heatmap color range is determined
 */
struct HeatmapColorRange {
    enum class Mode {
        Auto,       ///< Range from data min/max (default)
        Manual,     ///< User-specified vmin/vmax
        Symmetric,  ///< Symmetric around zero (for z-scores)
    };

    Mode mode = Mode::Auto;
    float vmin = 0.0f;     ///< Manual minimum (used when mode == Manual)
    float vmax = 1.0f;     ///< Manual maximum (used when mode == Manual)
};

// =============================================================================
// Mapper
// =============================================================================

/**
 * @brief Converts a grid of scalar rows into colored rectangle batches
 */
class HeatmapMapper {
public:
    /**
     * @brief Convert row data to a colored rectangle batch
     *
     * Each `HeatmapRowData` becomes one row of the grid. The first row
     * is placed at Y=0, the second at Y=1, etc. Each bin within a row
     * becomes one rectangle cell of height 1.
     *
     * Color is determined by normalising the scalar value to [0,1] using
     * the specified color range, then evaluating the colormap function.
     *
     * When `color_range.mode == Auto`, the range is computed as:
     *   - vmin = 0 (counts are non-negative)
     *   - vmax = max value across all rows and bins
     *
     * When `color_range.mode == Symmetric`:
     *   - vmax = max(|value|)
     *   - vmin = -vmax
     *
     * @param rows        Row data (one per unit/frequency/etc.)
     * @param colormap    Colormap function mapping [0,1] → RGBA
     * @param color_range Color range configuration
     * @return Rectangle batch ready for SceneRenderer
     */
    [[nodiscard]] static RenderableRectangleBatch toRectangleBatch(
        std::vector<HeatmapRowData> const & rows,
        Colormaps::ColormapFunction const & colormap,
        HeatmapColorRange const & color_range = {});

    /**
     * @brief Build a complete RenderableScene from row data
     *
     * Convenience that creates the rectangle batch and wraps it in a
     * RenderableScene with identity view/projection matrices (the widget
     * overrides with its own projection).
     *
     * @param rows        Row data (one per unit/frequency/etc.)
     * @param colormap    Colormap function
     * @param color_range Color range configuration
     * @return Complete scene ready for uploadScene()
     */
    [[nodiscard]] static RenderableScene buildScene(
        std::vector<HeatmapRowData> const & rows,
        Colormaps::ColormapFunction const & colormap,
        HeatmapColorRange const & color_range = {});

    /**
     * @brief Compute the auto color range from row data
     *
     * Returns {0, max_value} where max_value is the maximum scalar value
     * across all rows and bins. If all values are zero, returns {0, 1}.
     *
     * @param rows Row data to scan
     * @return Pair of (vmin, vmax)
     */
    [[nodiscard]] static std::pair<float, float> computeAutoRange(
        std::vector<HeatmapRowData> const & rows);
};

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_HEATMAPMAPPER_HPP
