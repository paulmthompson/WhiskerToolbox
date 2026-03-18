/// @file DataManagerKeys.hpp
/// @brief Free-function utilities for querying DataManager keys by DM_DataType.
///
/// Exposes `getKeysForTypes(dm, types)`, which maps a vector of `DM_DataType`
/// values to the corresponding DataManager keys. The concrete data type headers
/// are an implementation detail confined to `DataManagerKeys.cpp`; callers need
/// only `DM_DataType` and a forward declaration of `DataManager`.

#ifndef DATA_MANAGER_KEYS_HPP
#define DATA_MANAGER_KEYS_HPP

#include "DataTypeEnum/DM_DataType.hpp"

#include <string>
#include <vector>

class DataManager;

/// @brief Return all DataManager keys matching the given data types.
///
/// An empty @p types vector returns every key (equivalent to
/// `DataManager::getAllKeys()`). When multiple enum values map to the same
/// underlying C++ type (e.g. `Video` and `Images` both map to `MediaData`),
/// the result is deduplicated and sorted.
///
/// @pre @p dm must be a valid DataManager reference.
/// @param dm    The DataManager to query.
/// @param types The set of DM_DataType values to include.
/// @return Sorted, deduplicated list of matching keys.
[[nodiscard]] std::vector<std::string>
getKeysForTypes(DataManager & dm, std::vector<DM_DataType> const & types);

#endif// DATA_MANAGER_KEYS_HPP
