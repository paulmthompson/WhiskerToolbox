#ifndef SCATTER_AXIS_SOURCE_HPP
#define SCATTER_AXIS_SOURCE_HPP

/**
 * @file ScatterAxisSource.hpp
 * @brief Data source descriptor for a single scatter plot axis (X or Y)
 *
 * A ScatterAxisSource identifies which DataManager key and (for TensorData)
 * which column supplies the numeric values for one axis of the scatter plot.
 * The struct is serializable (all POD / std::string / std::optional).
 *
 * @see ScatterPlotState for usage in the state tree
 * @see SourceCompatibility.hpp for validation between two axis sources
 */

#include <cstddef>
#include <optional>
#include <string>

/**
 * @brief Describes a single axis data source (X or Y) for a scatter plot
 *
 * The @c data_key references a DataManager entry, which must be either an
 * `AnalogTimeSeries` or a `TensorData`. When the key points to `TensorData`,
 * @c tensor_column_name (preferred) or @c tensor_column_index selects the
 * column to use. Both may be set; the name takes priority when resolving.
 *
 * @c time_offset shifts the lookup index by the given number of
 * `TimeFrameIndex` steps, enabling lag-plots ($x(t)$ vs $x(t-1)$).
 */
struct ScatterAxisSource {
    std::string data_key;                            ///< DataManager key (AnalogTimeSeries or TensorData)
    std::optional<std::string> tensor_column_name;   ///< Column name (only for TensorData)
    std::optional<std::size_t> tensor_column_index;  ///< Column index fallback (if no names)
    int time_offset = 0;                             ///< Temporal offset in TimeFrameIndex steps
};

#endif  // SCATTER_AXIS_SOURCE_HPP
