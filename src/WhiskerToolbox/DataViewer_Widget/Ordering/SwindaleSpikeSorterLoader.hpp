#ifndef DATAVIEWER_SWINDALE_SPIKE_SORTER_LOADER_HPP
#define DATAVIEWER_SWINDALE_SPIKE_SORTER_LOADER_HPP

/**
 * @file SwindaleSpikeSorterLoader.hpp
 * @brief Parser and rank adapter for the SpikeSorter electrode configuration format.
 *
 * SpikeSorter is a spike-sorting program developed by Swindale & Spacek (2017):
 *   Swindale NV, Spacek MA. "SpikeSorter: Sorting large sets of waveforms from
 *   tetrode recordings." eNeuro. 2014. PMID: 28287541.
 *
 * This module provides two independent layers:
 *
 * **Parser layer** (`parseSwindaleSpikeSorterConfig`):
 *   Converts the raw SpikeSorter `.config` text into a `ChannelPositionMap`.
 *   This layer has no knowledge of series keys or ordering policy.
 *
 * **Rank adapter layer** (`buildSwindaleSpikeSorterRanks`, `orderKeysBySwindaleSpikeSorter`):
 *   Consumes a `ChannelPositionMap` together with a set of series keys and
 *   produces a `SortableRankMap` for downstream use by `OrderingPolicyResolver`.
 *   This layer has no knowledge of the original file format.
 *
 * @section FileFormat Configuration File Format
 *
 * The expected file format is a whitespace-separated text file with columns:
 * - Row index (ignored)
 * - Channel number (1-based, converted to 0-based internally)
 * - X position (micrometers, typically)
 * - Y position (micrometers, typically)
 *
 * The first line is treated as a header and skipped.
 *
 * Example:
 * @code
 * electrode row chan x y
 * 0 1 16.0 0.0
 * 1 2 48.0 0.0
 * 2 3 0.0 20.0
 * @endcode
 */

#include "Ordering/ChannelPositionMetadata.hpp"

#include <string>
#include <vector>

/**
 * @brief Parse a SpikeSorter electrode configuration file into a channel position list.
 *
 * Parses a whitespace-separated configuration produced by the Swindale SpikeSorter
 * program. Columns are: row index, channel number (1-based), x position, y position.
 * The first line is treated as a header and skipped.
 * Channel numbers are converted from 1-based to 0-based.
 *
 * @param text Raw text content of the SpikeSorter configuration file
 * @return Vector of `ChannelPosition` entries; empty if parsing fails entirely.
 */
[[nodiscard]] std::vector<ChannelPosition> parseSwindaleSpikeSorterConfig(std::string const & text);

/**
 * @brief Extract group name and channel ID from a series key.
 *
 * Thin wrapper around `parseSeriesIdentity` for compatibility with callers
 * that require output parameters rather than a struct return.
 *
 * @param key Series key to parse (e.g., "ephys_32")
 * @param[out] group Group name portion (e.g., "ephys")
 * @param[out] channel_id Channel ID (0-based)
 * @return true if a numeric channel suffix was parsed; false otherwise.
 */
[[nodiscard]] bool extractGroupAndChannelFromKey(
        std::string const & key,
        std::string & group,
        int & channel_id);

/**
 * @brief Build a `SortableRankMap` from a `ChannelPositionMap` for a set of series keys.
 *
 * Assigns integer ranks to each key based on:
 * 1. Group name (lexicographic)
 * 2. Y electrode position within the group (ascending)
 * 3. Channel ID (ascending, used when Y positions are equal or group has no config)
 * 4. Key string (lexicographic tie-break)
 *
 * Keys whose group is absent from `configs` receive the same Y rank (0.0) and
 * fall through to channel-ID / key ordering.
 *
 * @param keys Series keys to rank (typically visible analog series keys)
 * @param configs Parsed electrode position map (from `parseSwindaleSpikeSorterConfig`)
 * @return `SortableRankMap` ready for consumption by `OrderingPolicyResolver`
 */
[[nodiscard]] SortableRankMap buildSwindaleSpikeSorterRanks(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs);

/**
 * @brief Order series keys by probe electrode position using a SpikeSorter config.
 *
 * Convenience wrapper around `buildSwindaleSpikeSorterRanks` that returns keys
 * in sorted order rather than a rank map.
 *
 * @param keys Keys to order (typically visible analog series keys)
 * @param configs Parsed electrode position map
 * @return Keys sorted by group, then by Y position, then by channel ID
 */
[[nodiscard]] std::vector<std::string> orderKeysBySwindaleSpikeSorter(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs);

#endif// DATAVIEWER_SWINDALE_SPIKE_SORTER_LOADER_HPP
