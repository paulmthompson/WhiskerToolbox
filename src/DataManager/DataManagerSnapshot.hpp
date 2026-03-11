/**
 * @file DataManagerSnapshot.hpp
 * @brief Lightweight snapshot of DataManager state for testing and comparison
 */

#ifndef DATA_MANAGER_SNAPSHOT_HPP
#define DATA_MANAGER_SNAPSHOT_HPP

#include <cstddef>
#include <map>
#include <string>

class DataManager;

/// @brief Lightweight summary of a DataManager's state.
///
/// Captures the key→type mapping, entity/element counts, and a deterministic
/// content hash per key. Designed for golden trace tests: replay a command
/// sequence, snapshot the result, and compare against an expected snapshot.
struct DataManagerSnapshot {
    /// Map of data key → type name string (e.g. "line", "points", "analog")
    std::map<std::string, std::string> key_type;

    /// Map of data key → number of entities/elements
    std::map<std::string, std::size_t> key_entity_count;

    /// Map of data key → deterministic content hash (hex string)
    std::map<std::string, std::string> key_content_hash;

    friend bool operator==(DataManagerSnapshot const &, DataManagerSnapshot const &) = default;
};

/// @brief Produce a snapshot of a DataManager's current state.
///
/// Iterates all data keys (excluding "media"), recording the type, entity count,
/// and a deterministic FNV-1a content hash for each entry.
///
/// @param dm The DataManager to snapshot (non-const because DataManager accessors are non-const)
/// @return A DataManagerSnapshot summarising the current state
DataManagerSnapshot snapshotDataManager(DataManager & dm);

#endif// DATA_MANAGER_SNAPSHOT_HPP
