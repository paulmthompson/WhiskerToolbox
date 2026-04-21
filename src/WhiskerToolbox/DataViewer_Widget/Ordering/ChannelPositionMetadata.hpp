#ifndef DATAVIEWER_CHANNEL_POSITION_METADATA_HPP
#define DATAVIEWER_CHANNEL_POSITION_METADATA_HPP

/**
 * @file ChannelPositionMetadata.hpp
 * @brief Generic probe electrode position metadata and sortable-rank types.
 *
 * This header defines the source-agnostic data model for electrode channel
 * positions on a multi-channel probe. These types are consumed by rank adapter
 * layers (e.g. SwindaleSpikeSorterLoader) and ordering policy (OrderingPolicyResolver)
 * without coupling either to a specific file format.
 *
 * @note `ChannelPosition` and `ChannelPositionMap` are not tied to any particular
 * spike-sorting program. Any loader that can produce electrode position data should
 * output these types and remain unaware of downstream ordering policy.
 */

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Physical position of one electrode channel on a multi-channel probe.
 *
 * Coordinates are typically in micrometers. Only the Y axis (depth along the
 * probe shank) is used for vertical stacking order; X is stored for completeness.
 */
struct ChannelPosition {
    int channel_id{0};///< Channel identifier (0-based)
    float x{0.0f};    ///< X position in micrometers (unused for vertical ordering)
    float y{0.0f};    ///< Y position in micrometers (used for ordering)
};

/**
 * @brief Map of probe group name to its electrode channel positions.
 *
 * A single recording session may have multiple probes or channel groups.
 * Keys are group names that correspond to the group component of a series key
 * (e.g., "ephys" in the key "ephys_32").
 */
using ChannelPositionMap = std::unordered_map<std::string, std::vector<ChannelPosition>>;

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
 * This utility is ingestion-agnostic and is used by ordering policy,
 * rank adapters, and future providers.
 *
 * @param key Series key to parse (e.g., "ephys_32")
 * @return Normalized identity. Non-conforming keys return group=key, channel_id=nullopt.
 */
[[nodiscard]] NormalizedSeriesIdentity parseSeriesIdentity(std::string const & key);

/**
 * @brief Generic rank map consumed by ordering policy.
 *
 * Maps series key to an integer rank. Lower values appear earlier in the
 * stacked lane layout. Produced by rank adapter functions such as
 * `buildSwindaleSpikeSorterRanks`.
 */
using SortableRankMap = std::unordered_map<std::string, int>;

/**
 * @brief Provider contract for external sortable-rank sources.
 *
 * A `SortableRankProvider` is any callable that, given the current set of
 * visible series keys, returns a `SortableRankMap`. Binding a specific
 * loader's rank-building function to this type decouples `LayoutRequestBuildContext`
 * from any particular file format.
 */
using SortableRankProvider = std::function<SortableRankMap(std::vector<std::string> const &)>;

#endif// DATAVIEWER_CHANNEL_POSITION_METADATA_HPP
