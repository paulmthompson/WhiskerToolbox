#ifndef COREUTILITIES_PROBE_GEOMETRY_CHANNEL_POSITION_HPP
#define COREUTILITIES_PROBE_GEOMETRY_CHANNEL_POSITION_HPP

/**
 * @file ChannelPosition.hpp
 * @brief Generic probe electrode position metadata and sortable-rank types.
 *
 * Defines the source-agnostic data model for electrode channel positions on a
 * multi-channel neural probe. These types are consumed by spike-sorting transforms
 * (such as SwindaleEventDetection), rank adapter layers, and viewer ordering policies.
 */

#include "coreutilities_export.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Physical position of one electrode channel on a multi-channel probe.
 *
 * Coordinates are in micrometers.
 * - x: position perpendicular to the shank (or along the probe width)
 * - y: depth along the probe shank
 */
struct ChannelPosition {
    int channel_id{0};///< Channel identifier (0-based)
    float x{0.0f};    ///< X position in micrometers
    float y{0.0f};    ///< Y position in micrometers
};

/**
 * @brief Map of probe group name to its electrode channel positions.
 *
 * Keys are group names corresponding to the group component of a series key
 * (e.g. "ephys" in "ephys_32").
 */
using ChannelPositionMap = std::unordered_map<std::string, std::vector<ChannelPosition>>;

/**
 * @brief Normalized identity parsed from a series key.
 *
 * Keys in the form "group_N" produce:
 * - group = "group"
 * - channel_id = N-1 (0-based)
 *
 * Non-conforming keys produce:
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
 * @param key Series key to parse (e.g. "ephys_32")
 * @return Normalized identity. Non-conforming keys return group=key, channel_id=nullopt.
 */
[[nodiscard]] COREUTILITIES_EXPORT NormalizedSeriesIdentity parseSeriesIdentity(std::string const & key);

/**
 * @brief Extract a 0-based channel index from a series key within a group.
 *
 * @param key Series key (e.g. "voltage_19")
 * @param group Expected group prefix (e.g. "voltage")
 * @param key_one_based true when suffixes are 1-based (e.g. "voltage_1" = first channel)
 * @return 0-based channel index, or std::nullopt when the key does not match
 */
[[nodiscard]] std::optional<int> extractChannelFromSeriesKey(
        std::string const & key,
        std::string const & group,
        bool key_one_based);

/**
 * @brief Infer whether series key suffixes in a group are 1-based.
 *
 * @param keys Series keys to inspect (typically all analog keys in a group)
 * @param group Group prefix to match (e.g. "voltage")
 * @return false (0-based) when any key has suffix 0, otherwise true (1-based)
 */
[[nodiscard]] COREUTILITIES_EXPORT bool detectKeyOneBased(
        std::vector<std::string> const & keys,
        std::string const & group);

/**
 * @brief Generic rank map consumed by ordering policy.
 *
 * Maps series key to an integer rank.
 */
using SortableRankMap = std::unordered_map<std::string, int>;

/**
 * @brief Provider contract for external sortable-rank sources.
 */
using SortableRankProvider = std::function<SortableRankMap(std::vector<std::string> const &)>;

#endif// COREUTILITIES_PROBE_GEOMETRY_CHANNEL_POSITION_HPP
