#ifndef MLCORE_TIMEENTITYUTILS_HPP
#define MLCORE_TIMEENTITYUTILS_HPP

/**
 * @file TimeEntityUtils.hpp
 * @brief Utility functions for bulk TimeEntity registration and conversions
 *
 * Provides higher-level helpers that bridge the Entity system and data types:
 * - Bulk registration of TimeFrameIndex values as TimeEntity IDs
 * - Bidirectional conversion between DigitalIntervalSeries and TimeEntity groups
 *
 * These keep DigitalIntervalSeries as the compact representation for contiguous
 * time regions, while entity groups provide per-frame labeling for ML workflows.
 *
 * @see ml_library_roadmap.md §3.3
 */

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

// Forward declarations
class DataManager;
class DigitalIntervalSeries;
class EntityGroupManager;
class EntityRegistry;
class TimeKey;
using GroupId = std::uint64_t;

namespace MLCore {

// ============================================================================
// Bulk registration
// ============================================================================

/**
 * @brief Register a set of arbitrary TimeFrameIndex values as TimeEntities.
 *
 * Each frame in @p frames is registered (or retrieved if already present)
 * via DataManager::ensureTimeEntityId(). The returned vector is parallel
 * to @p frames — result[i] is the EntityId for frames[i].
 *
 * @param dm           DataManager (must have a valid EntityRegistry)
 * @param time_key_str The TimeFrame key string (clock identity)
 * @param frames       Span of TimeFrameIndex values to register
 * @return             Vector of EntityIds, one per input frame (same order)
 */
std::vector<EntityId> registerTimeEntities(
    DataManager & dm,
    std::string const & time_key_str,
    std::span<TimeFrameIndex const> frames);

// ============================================================================
// Interval ↔ TimeEntity conversions
// ============================================================================

/**
 * @brief Expand a DigitalIntervalSeries into individual TimeEntity IDs.
 *
 * For each interval [start, end] in @p intervals, registers every integer
 * index in [start, end] as a TimeEntity. Returns the full set of EntityIds
 * across all intervals, sorted by TimeFrameIndex value.
 *
 * Duplicate registrations (overlapping intervals) are handled idempotently
 * by the underlying EntityRegistry.
 *
 * @param dm             DataManager with a valid EntityRegistry
 * @param intervals      The interval series to expand
 * @param time_key_str   The TimeFrame key string (clock identity)
 * @return               Vector of EntityIds, sorted ascending by time value
 *
 * @note The resulting vector may contain fewer elements than the sum of
 *       interval lengths if intervals overlap.
 */
std::vector<EntityId> intervalsToTimeEntities(
    DataManager & dm,
    DigitalIntervalSeries const & intervals,
    std::string const & time_key_str);

/**
 * @brief Convert a group of TimeEntity IDs into a DigitalIntervalSeries.
 *
 * Retrieves all TimeEntity members of @p group_id whose data_key matches
 * @p time_key_str, sorts by TimeFrameIndex value, and merges adjacent
 * frames (difference == 1) into contiguous intervals.
 *
 * Non-TimeEntity members and TimeEntities from other clocks are silently
 * skipped.
 *
 * @param dm             DataManager with valid EntityRegistry and EntityGroupManager
 * @param group_id       The group to read entities from
 * @param time_key_str   The TimeFrame key string to filter by
 * @return               Shared pointer to a DigitalIntervalSeries containing
 *                       the merged intervals, or nullptr if the group is empty
 *                       or has no matching TimeEntities
 */
std::shared_ptr<DigitalIntervalSeries> timeEntitiesToIntervals(
    DataManager & dm,
    GroupId group_id,
    std::string const & time_key_str);

} // namespace MLCore

#endif // MLCORE_TIMEENTITYUTILS_HPP