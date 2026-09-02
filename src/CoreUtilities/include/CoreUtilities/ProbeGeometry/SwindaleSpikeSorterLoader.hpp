#ifndef COREUTILITIES_PROBE_GEOMETRY_SWINDALE_SPIKE_SORTER_LOADER_HPP
#define COREUTILITIES_PROBE_GEOMETRY_SWINDALE_SPIKE_SORTER_LOADER_HPP

/**
 * @file SwindaleSpikeSorterLoader.hpp
 * @brief Parser and rank adapter for the SpikeSorter electrode configuration format.
 *
 * References:
 *   Swindale & Spacek (2014). Front Syst Neurosci 8:6. doi:10.3389/fnsys.2014.00006
 *   Swindale & Spacek (2015). J Comput Neurosci 38:249-267. doi:10.1007/s10827-014-0539-z
 *   Swindale et al. (2017). J Vis Exp 55217. doi:10.3791/55217
 */

#include "coreutilities_export.h"
#include "CoreUtilities/ProbeGeometry/ChannelPosition.hpp"

#include <string>
#include <vector>

/**
 * @brief Parse a SpikeSorter electrode configuration file into a channel position list.
 *
 * Parses a whitespace/comma-separated configuration produced by Swindale SpikeSorter.
 * Columns are: row index, channel number (1-based), x position, y position.
 * The first line is treated as a header and skipped.
 * Channel numbers are converted from 1-based to 0-based.
 *
 * @param text Raw text content of the SpikeSorter configuration file (.cfg)
 * @return Vector of `ChannelPosition` entries; empty if parsing fails entirely.
 */
[[nodiscard]] std::vector<ChannelPosition> COREUTILITIES_EXPORT parseSwindaleSpikeSorterConfig(std::string const & text);

/**
 * @brief Extract group name and channel ID from a series key.
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
 * @param keys Series keys to rank (typically visible analog series keys)
 * @param configs Parsed electrode position map (from `parseSwindaleSpikeSorterConfig`)
 * @param key_one_based true when series key suffixes are 1-based (e.g. "voltage_1" = first channel)
 * @return `SortableRankMap` ready for ordering policy
 */
[[nodiscard]] COREUTILITIES_EXPORT SortableRankMap buildSwindaleSpikeSorterRanks(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs,
        bool key_one_based = true);

/**
 * @brief Order series keys by probe electrode position using a SpikeSorter config.
 *
 * @param keys Keys to order (typically visible analog series keys)
 * @param configs Parsed electrode position map
 * @param key_one_based true when series key suffixes are 1-based (e.g. "voltage_1" = first channel)
 * @return Keys sorted by group, then by Y position, then by channel ID
 */
[[nodiscard]] COREUTILITIES_EXPORT std::vector<std::string> orderKeysBySwindaleSpikeSorter(
        std::vector<std::string> const & keys,
        ChannelPositionMap const & configs,
        bool key_one_based = true);

#endif// COREUTILITIES_PROBE_GEOMETRY_SWINDALE_SPIKE_SORTER_LOADER_HPP
