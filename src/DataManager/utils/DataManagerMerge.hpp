/// @file DataManagerMerge.hpp
/// @brief Type-erased overwrite-merge utilities for DataManager data objects.
///
/// Exposes `mergeOverwriteData()` for merging all entries from a source key into
/// a target key. Concrete data-type headers are confined to `DataManagerMerge.cpp`.

#ifndef DATA_MANAGER_MERGE_HPP
#define DATA_MANAGER_MERGE_HPP

#include "DataManager/DataManagerTypes.hpp"

#include <cstddef>
#include <optional>
#include <string>

class DataManager;

/// @brief Check whether @p source supports overwrite-merge into an existing object.
[[nodiscard]] bool supportsMergeOverwrite(DataTypeVariant const & source);

/// @brief Overwrite-merge all entries from a source data object into a target.
///
/// Supported types in v1: MaskData, LineData, PointData (via RaggedTimeSeries).
/// Unsupported types return @c std::nullopt with a descriptive @p error_message.
///
/// @pre @p dm must contain @p target_key and @p source_key with the same data type
/// @pre Source and target must share the same non-null @c TimeFrame object
/// @param dm DataManager containing both objects
/// @param target_key Key of the object to merge into (mutated in place)
/// @param source_key Key of the object to copy from (unchanged)
/// @param error_message Set on failure
/// @return Number of entries merged on success, or empty on failure
[[nodiscard]] std::optional<std::size_t>
mergeOverwriteData(DataManager & dm,
                   std::string const & target_key,
                   std::string const & source_key,
                   std::string & error_message);

/// @brief Overwrite-merge pipeline output into an existing DataManager object.
///
/// Same semantics as the key-to-key overload, but the source is a @c DataTypeVariant
/// that is not yet stored in @p dm (e.g. transform pipeline output). The target at
/// @p target_key is mutated in place; @p source is not modified.
///
/// @pre @p dm must contain @p target_key
/// @pre @p source and the target object must hold the same data type alternative
/// @pre Source and target must share the same non-null @c TimeFrame object
/// @param dm DataManager containing the target object
/// @param target_key Key of the object to merge into (mutated in place)
/// @param source Source data variant (not registered in @p dm)
/// @param error_message Set on failure
/// @return Number of entries merged on success, or empty on failure
[[nodiscard]] std::optional<std::size_t>
mergeOverwriteData(DataManager & dm,
                   std::string const & target_key,
                   DataTypeVariant const & source,
                   std::string & error_message);

#endif// DATA_MANAGER_MERGE_HPP
