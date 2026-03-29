/**
 * @file FeatureColorSourceDescriptor.hpp
 * @brief Serializable descriptor for an external data source used for feature-based point coloring
 *
 * Shared across ScatterPlotWidget, EventPlotWidget, and LinePlotWidget. Describes which
 * DataManager key and (optionally) which TensorData column provides the color values.
 */

#ifndef COREPLOTTING_FEATURECOLOR_FEATURECOLORSOURCEDESCRIPTOR_HPP
#define COREPLOTTING_FEATURECOLOR_FEATURECOLORSOURCEDESCRIPTOR_HPP

#include <cstddef>
#include <optional>
#include <string>

namespace CorePlotting::FeatureColor {

/**
 * @brief Describes an external data source for feature-based point coloring
 *
 * Points to a DataManager key (AnalogTimeSeries, TensorData, or DigitalIntervalSeries)
 * and, for TensorData, specifies which column to extract color values from.
 *
 * Designed for serialization via reflect-cpp.
 */
struct FeatureColorSourceDescriptor {
    std::string data_key;                          ///< DataManager key for the color source
    std::optional<std::string> tensor_column_name; ///< Column name (TensorData only)
    std::optional<std::size_t> tensor_column_index;///< Column index fallback (TensorData only)
};

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_FEATURECOLORSOURCEDESCRIPTOR_HPP
