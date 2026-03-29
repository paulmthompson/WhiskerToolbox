/**
 * @file PlotSourceRowType.hpp
 * @brief Row-type classification for plot data sources
 *
 * Shared enum used by the FeatureColor resolution and compatibility logic.
 * Generalizes ScatterSourceRowType for use across multiple plot widgets.
 */

#ifndef COREPLOTTING_FEATURECOLOR_PLOTSOURCEROWTYPE_HPP
#define COREPLOTTING_FEATURECOLOR_PLOTSOURCEROWTYPE_HPP

namespace CorePlotting::FeatureColor {

/**
 * @brief Classification of a plot data source's row semantics
 *
 * Determines how per-point time indices are joined with a color source.
 */
enum class PlotSourceRowType {
    AnalogTimeSeries,    ///< Source is an AnalogTimeSeries (always TimeFrameIndex)
    TensorTimeFrameIndex,///< Source is a TensorData with RowType::TimeFrameIndex
    TensorInterval,      ///< Source is a TensorData with RowType::Interval
    TensorOrdinal,       ///< Source is a TensorData with RowType::Ordinal
    Unknown              ///< Key not found or data type not supported
};

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_PLOTSOURCEROWTYPE_HPP
