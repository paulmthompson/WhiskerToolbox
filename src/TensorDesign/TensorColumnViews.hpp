#ifndef TENSOR_COLUMN_VIEWS_HPP
#define TENSOR_COLUMN_VIEWS_HPP

/**
 * @file TensorColumnViews.hpp
 * @brief Qt-free utilities bridging TensorData and AnalogTimeSeries in DataManager.
 *
 * Provides three directions of tensor↔DataManager interaction:
 * - **View**: TensorData → zero-copy AnalogTimeSeries column views
 * - **Pack**: AnalogTimeSeries channels → dense TensorData
 * - **Discover**: find related AnalogTimeSeries keys sharing a common prefix
 */

#include <cstddef>
#include <string>
#include <vector>

class DataManager;

namespace Neuralyzer::TensorDesign {

/**
 * @brief A group of AnalogTimeSeries keys sharing a common prefix.
 */
struct AnalogKeyGroup {
    std::string prefix;           ///< Shared prefix (e.g. "voltage")
    std::vector<std::string> keys;///< Sorted list of full keys
};

/**
 * @brief Create AnalogTimeSeries column views from a TensorData in DataManager.
 *
 * For each selected column (or all columns if @p columns is empty), creates a
 * zero-copy AnalogTimeSeries view backed by TensorColumnAnalogStorage and
 * inserts it into the DataManager under "{prefix}/{column_name}".
 *
 * @param dm DataManager containing the tensor
 * @param tensor_key DataManager key of the source TensorData
 * @param prefix Prefix for generated AnalogTimeSeries keys
 * @param columns 0-based column indices to extract (empty = all columns)
 * @return Number of views successfully created
 *
 * @pre dm must contain a TensorData at @p tensor_key
 * @pre The TensorData must be 2D
 * @pre All column indices must be in [0, numColumns())
 */
[[nodiscard]] std::size_t createTensorColumnViews(
        DataManager & dm,
        std::string const & tensor_key,
        std::string const & prefix,
        std::vector<int> const & columns);

/**
 * @brief Discover groups of AnalogTimeSeries keys sharing a common prefix.
 *
 * Scans all AnalogTimeSeries keys in the DataManager, extracts the prefix
 * before the last '_' character, and groups keys by that prefix. Only groups
 * with 2 or more keys are returned.
 *
 * @param dm DataManager to scan for AnalogTimeSeries keys
 * @return Vector of key groups, sorted by prefix
 */
[[nodiscard]] std::vector<AnalogKeyGroup> discoverAnalogKeyGroups(DataManager & dm);

/**
 * @brief Populate an existing TensorData from AnalogTimeSeries channels.
 *
 * Gathers the specified AnalogTimeSeries channels, packs them into a 2D
 * TensorData via analogToTensor(), and replaces the data at @p tensor_key.
 * The TimeKey is inherited from the first analog channel.
 *
 * @param dm DataManager containing the tensor and analog data
 * @param tensor_key Key of the TensorData to populate
 * @param analog_keys Ordered list of AnalogTimeSeries keys to pack as columns
 * @return true if the tensor was successfully populated
 *
 * @pre dm must contain a TensorData at @p tensor_key
 * @pre All analog_keys must exist and contain AnalogTimeSeries
 * @pre All channels must share the same TimeFrame and sample count
 */
[[nodiscard]] bool populateTensorFromAnalogKeys(
        DataManager & dm,
        std::string const & tensor_key,
        std::vector<std::string> const & analog_keys);

}// namespace Neuralyzer::TensorDesign

#endif// TENSOR_COLUMN_VIEWS_HPP
