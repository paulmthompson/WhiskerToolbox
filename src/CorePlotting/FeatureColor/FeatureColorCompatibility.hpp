/**
 * @file FeatureColorCompatibility.hpp
 * @brief Compatibility checking between a plot data source and a feature color source
 *
 * Validates that a FeatureColorSourceDescriptor can be joined with the plot's
 * data source given the source row type and point count.
 */

#ifndef COREPLOTTING_FEATURECOLOR_FEATURECOLORCOMPATIBILITY_HPP
#define COREPLOTTING_FEATURECOLOR_FEATURECOLORCOMPATIBILITY_HPP

#include "FeatureColorSourceDescriptor.hpp"
#include "PlotSourceRowType.hpp"

#include <cstddef>
#include <string>

class DataManager;

namespace CorePlotting::FeatureColor {

/**
 * @brief Result of a feature color compatibility check
 */
struct FeatureColorCompatibilityResult {
    bool compatible = false;///< true if the source can be joined with the plot data
    std::string reason;     ///< Human-readable explanation (empty if compatible)
};

/**
 * @brief Check whether a feature color source can be joined with a plot data source
 *
 * Validates:
 * - The DataManager key exists and resolves to a supported type
 *   (AnalogTimeSeries, TensorData, or DigitalIntervalSeries)
 * - For TensorData: the specified column exists
 * - The row types are compatible for the chosen join strategy
 *
 * @param dm           The DataManager to look up keys in
 * @param descriptor   Describes which data key / column provides color values
 * @param source_type  Row type of the plot data source
 * @param point_count  Number of points in the plot (used for ordinal validation)
 * @return FeatureColorCompatibilityResult with compatible=true or a reason string
 */
[[nodiscard]] FeatureColorCompatibilityResult checkFeatureColorCompatibility(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor,
        PlotSourceRowType source_type,
        std::size_t point_count);

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_FEATURECOLORCOMPATIBILITY_HPP
