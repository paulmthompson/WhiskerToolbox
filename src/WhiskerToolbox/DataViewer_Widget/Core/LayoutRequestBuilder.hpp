#ifndef DATAVIEWER_LAYOUT_REQUEST_BUILDER_HPP
#define DATAVIEWER_LAYOUT_REQUEST_BUILDER_HPP

/**
 * @file LayoutRequestBuilder.hpp
 * @brief Builds CorePlotting layout requests from DataViewer runtime/state inputs.
 *
 * This module centralizes lane ordering and lane override policy so rendering
 * widgets do not need to know fallback ordering details.
 */

#include "Ordering/ChannelPositionMetadata.hpp"
#include "Ordering/SwindaleSpikeSorterLoader.hpp"

#include "CorePlotting/Layout/LayoutEngine.hpp"

class DataViewerState;

namespace DataViewer {

class TimeSeriesDataStore;

/**
 * @brief Inputs required to build a layout request.
 */
struct LayoutRequestBuildContext {
    ::DataViewerState const & state;
    TimeSeriesDataStore const & data_store;
    ChannelPositionMap const & spike_sorter_configs;
    float viewport_y_min{-1.0f};
    float viewport_y_max{1.0f};
};

/**
 * @brief Build a CorePlotting layout request from current DataViewer state.
 */
[[nodiscard]] CorePlotting::LayoutRequest buildLayoutRequest(LayoutRequestBuildContext const & context);

}// namespace DataViewer

#endif// DATAVIEWER_LAYOUT_REQUEST_BUILDER_HPP
