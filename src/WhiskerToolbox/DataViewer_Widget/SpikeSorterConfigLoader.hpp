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

#endif // SPIKESORTER_CONFIG_LOADER_HPP
