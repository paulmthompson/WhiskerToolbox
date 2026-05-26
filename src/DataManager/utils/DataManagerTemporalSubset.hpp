/// @file DataManagerTemporalSubset.hpp
/// @brief Type-erased temporal subset utilities for DataManager data objects.
///
/// Exposes `createTemporalSubset()` for `DataTypeVariant` and DataManager keys.
/// Concrete data-type headers are confined to `DataManagerTemporalSubset.cpp`.

#ifndef DATA_MANAGER_TEMPORAL_SUBSET_HPP
#define DATA_MANAGER_TEMPORAL_SUBSET_HPP

#include "DataManager/DataManagerTypes.hpp"

#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/interval_data.hpp"

#include <optional>
#include <string>

class DataManager;

/// @brief Create a temporal subset of a data object at a single time index.
///
/// Equivalent to `createTemporalSubset(source, TimeFrameInterval{time, time}, error_message)`.
///
/// @pre @p source must hold a non-null `shared_ptr`.
/// @param source Source data variant.
/// @param time Inclusive time index for the subset.
/// @param error_message Set on failure.
/// @return Subset variant (same alternative as @p source), or empty on failure.
[[nodiscard]] std::optional<DataTypeVariant>
createTemporalSubset(DataTypeVariant const & source,
                     TimeFrameIndex time,
                     std::string & error_message);

/// @brief Create a temporal subset of a data object over a time interval.
///
/// Interval semantics are inclusive on both ends: `[interval.start, interval.end]`.
/// Unsupported types (MediaData, TensorData in v1) return `std::nullopt`.
///
/// @pre @p source must hold a non-null `shared_ptr`.
/// @param source Source data variant.
/// @param interval Inclusive time range.
/// @param error_message Set on failure.
/// @return Subset variant (same alternative as @p source), or empty on failure.
[[nodiscard]] std::optional<DataTypeVariant>
createTemporalSubset(DataTypeVariant const & source,
                     TimeFrameInterval interval,
                     std::string & error_message);

/// @brief Create a temporal subset of a DataManager object by key.
///
/// @pre @p dm must contain @p key.
/// @param dm DataManager to query.
/// @param key Data object key.
/// @param interval Inclusive time range.
/// @param error_message Set on failure (missing key, null data, unsupported type).
/// @return Subset variant, or empty on failure.
[[nodiscard]] std::optional<DataTypeVariant>
createTemporalSubset(DataManager & dm,
                     std::string const & key,
                     TimeFrameInterval interval,
                     std::string & error_message);

#endif// DATA_MANAGER_TEMPORAL_SUBSET_HPP
