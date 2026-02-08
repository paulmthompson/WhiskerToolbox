#ifndef COREPLOTTING_DATATYPES_HISTOGRAMDATA_HPP
#define COREPLOTTING_DATATYPES_HISTOGRAMDATA_HPP

/**
 * @file HistogramData.hpp
 * @brief API-agnostic histogram data representation
 *
 * Describes a set of uniformly-spaced bins for histogram visualization.
 * Used by both PSTH and ACF widgets (and any future histogram-based plots).
 *
 * The bin edges run from  bin_start  to  bin_start + num_bins * bin_width.
 * The center of bin i is  bin_start + (i + 0.5) * bin_width.
 */

#include <glm/glm.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace CorePlotting {

/**
 * @brief Uniform-bin histogram ready for visualization
 *
 * All fields are in *data* space (the widget's coordinate system).
 * The mapper converts this into renderable geometry.
 */
struct HistogramData {
    double bin_start{0.0};          ///< Left edge of the first bin (x-axis)
    double bin_width{1.0};          ///< Width of each bin (x-axis units)
    std::vector<double> counts;     ///< Per-bin values (counts, rates, etc.)

    // ----- convenience accessors -----

    [[nodiscard]] std::size_t numBins() const { return counts.size(); }

    /** @brief Left edge of bin i */
    [[nodiscard]] double binLeft(std::size_t i) const {
        return bin_start + static_cast<double>(i) * bin_width;
    }

    /** @brief Center of bin i */
    [[nodiscard]] double binCenter(std::size_t i) const {
        return bin_start + (static_cast<double>(i) + 0.5) * bin_width;
    }

    /** @brief Right edge of bin i */
    [[nodiscard]] double binRight(std::size_t i) const {
        return bin_start + static_cast<double>(i + 1) * bin_width;
    }

    /** @brief Right edge of the last bin */
    [[nodiscard]] double binEnd() const {
        return bin_start + static_cast<double>(counts.size()) * bin_width;
    }

    /** @brief Maximum bin value (0 if empty) */
    [[nodiscard]] double maxCount() const {
        double m = 0.0;
        for (auto c : counts) {
            if (c > m) m = c;
        }
        return m;
    }
};

/**
 * @brief Rendering mode for histogram visualization
 */
enum class HistogramDisplayMode {
    Bar,   ///< Filled rectangles (one per bin)
    Line   ///< Polyline connecting bin centers
};

} // namespace CorePlotting

#endif // COREPLOTTING_DATATYPES_HISTOGRAMDATA_HPP
