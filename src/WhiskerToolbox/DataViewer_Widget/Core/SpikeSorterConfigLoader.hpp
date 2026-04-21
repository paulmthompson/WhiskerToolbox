#ifndef SPIKESORTER_CONFIG_LOADER_HPP
#define SPIKESORTER_CONFIG_LOADER_HPP

/**
 * @file SpikeSorterConfigLoader.hpp
 * @brief Utilities for loading and applying spike sorter configuration files
 * 
 * Spike sorter software (like Kilosort, etc.) provides electrode position information
 * that can be used to order analog channels by their physical position on the probe.
 * This module provides parsing and ordering utilities for these configurations.
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

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Channel position for spike sorter configuration
 * 
 * Used to specify custom ordering of analog series based on physical
 * electrode positions from spike sorting software.
 */
struct ChannelPosition {
    int channel_id{0};///< Channel identifier (0-based)
    float x{0.0f};    ///< X position (unused for vertical stacking)
    float y{0.0f};    ///< Y position (used for ordering)
};

/**
 * @brief Normalized identity parsed from a DataViewer series key.
 *
 * Keys in the expected form "group_N" produce:
 * - group = "group"
 * - channel_id = N-1 (0-based)
 *
 * Keys that do not match this pattern are still representable:
 * - group = full key
 * - channel_id = std::nullopt
 */
struct NormalizedSeriesIdentity {
    std::string group;
    std::optional<int> channel_id;
};

/**
 * @brief Parse a series key into a normalized group/channel identity.
 *
 * This utility is ingestion-agnostic and can be used by ordering policy,
 * spike-sorter rank adapters, and future providers.
 *
 * @param key Series key to parse (e.g., "ephys_32")
 * @return Normalized identity. Non-conforming keys return group=key, channel_id=nullopt.
 */
[[nodiscard]] NormalizedSeriesIdentity parseSeriesIdentity(std::string const & key);

/**
 * @brief Parse spike sorter configuration from text content
 * 
 * Parses a whitespace-separated configuration file with columns:
 * row, channel, x, y. The first line is treated as a header and skipped.
 * Channel numbers are converted from 1-based to 0-based.
 * 
 * @param text Raw text content of the configuration file
 * @return Vector of ChannelPosition entries, empty if parsing fails
 */
[[nodiscard]] std::vector<ChannelPosition> parseSpikeSorterConfig(std::string const & text);

/**
 * @brief Extract group name and channel ID from a series key
 * 
 * Parses keys in the format "groupname_N" where N is the channel number.
 * Used for spike sorter configuration ordering.
 * 
 * @param key Series key to parse (e.g., "ephys_32")
 * @param[out] group Group name portion (e.g., "ephys")
 * @param[out] channel_id Channel ID (0-based, parsed from 1-based in key)
 * @return true if parsing succeeded, false otherwise
 */
[[nodiscard]] bool extractGroupAndChannelFromKey(
        std::string const & key,
        std::string & group,
        int & channel_id);

/**
 * @brief Configuration map type for spike sorter configurations
 * 
 * Maps group_name -> vector of channel positions for that group.
 */
using SpikeSorterConfigMap = std::unordered_map<std::string, std::vector<ChannelPosition>>;

/**
 * @brief Generic sortable-rank map consumed by ordering policy.
 *
 * Lower rank values appear earlier in ordering.
 */
using SortableRankMap = std::unordered_map<std::string, int>;

/**
 * @brief Provider contract for external sortable-rank sources.
 */
using SortableRankProvider = std::function<SortableRankMap(std::vector<std::string> const &)>;

/**
 * @brief Convert spike-sorter channel-position metadata into sortable ranks.
 *
 * This adapter layer is independent from parser usage and can be swapped with
 * any other rank provider that returns SortableRankMap.
 *
 * @param keys Keys to rank (e.g., analog series keys)
 * @param configs Parsed spike-sorter config map
 * @return key->rank map (lower rank = earlier ordering)
 */
[[nodiscard]] SortableRankMap buildSpikeSorterSortableRanks(
        std::vector<std::string> const & keys,
        SpikeSorterConfigMap const & configs);

/**
 * @brief Order series keys according to spike sorter configuration
 * 
 * Returns series keys sorted by group name, then by Y position within groups
 * that have spike sorter configuration. Series without configuration are
 * sorted by channel ID.
 * 
 * @param keys Keys to order (e.g., analog series keys)
 * @param configs Spike sorter configuration map
 * @return Ordered vector of keys
 */
[[nodiscard]] std::vector<std::string> orderKeysBySpikeSorterConfig(
        std::vector<std::string> const & keys,
        SpikeSorterConfigMap const & configs);

#endif// SPIKESORTER_CONFIG_LOADER_HPP
