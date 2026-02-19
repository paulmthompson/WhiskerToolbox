/**
 * @file LabelAssembler.cpp
 * @brief Implementation of label assembly from intervals, time-entity groups, and data-entity groups
 */

#include "LabelAssembler.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace MLCore {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/**
 * @brief Build class names from group descriptors
 *
 * Falls back to "Class_<group_id>" if a group doesn't exist.
 */
std::vector<std::string> buildClassNames(
    EntityGroupManager const & groups,
    std::vector<GroupId> const & class_groups)
{
    std::vector<std::string> names;
    names.reserve(class_groups.size());
    for (auto gid : class_groups) {
        auto desc = groups.getGroupDescriptor(gid);
        names.push_back(desc ? desc->name : "Class_" + std::to_string(gid));
    }
    return names;
}

/**
 * @brief Build a mapping from time_value (int64_t) → class label (size_t)
 *
 * Iterates all entities in each class group, looks up their descriptors,
 * and maps time_value → label index. Only entities matching the given
 * data_key are included. An optional kind filter can further restrict matches.
 *
 * If an entity's time_value appears in multiple groups, the first group wins
 * (lower class index takes priority via try_emplace).
 *
 * @param groups Entity group manager
 * @param registry Entity registry for descriptor lookup
 * @param class_groups Ordered vector of group IDs (index = class label)
 * @param match_key Key to match against EntityDescriptor::data_key
 * @param require_kind If set, only entities of this kind are included
 * @return Map from time_value → class label
 */
std::unordered_map<std::int64_t, std::size_t> buildTimeLabelMap(
    EntityGroupManager const & groups,
    EntityRegistry const & registry,
    std::vector<GroupId> const & class_groups,
    std::string const & match_key,
    std::optional<EntityKind> require_kind)
{
    std::unordered_map<std::int64_t, std::size_t> time_to_label;

    for (std::size_t label = 0; label < class_groups.size(); ++label) {
        auto entity_ids = groups.getEntitiesInGroup(class_groups[label]);
        for (auto eid : entity_ids) {
            auto desc = registry.get(eid);
            if (!desc) continue;
            if (desc->data_key != match_key) continue;
            if (require_kind.has_value() && desc->kind != *require_kind) continue;

            // First assignment wins — lower class index has priority
            time_to_label.try_emplace(desc->time_value, label);
        }
    }

    return time_to_label;
}

/**
 * @brief Produce labels from a prebuilt time→label map
 *
 * Assigns label num_classes (sentinel) to rows not found in the map.
 */
AssembledLabels labelsFromTimeLabelMap(
    std::unordered_map<std::int64_t, std::size_t> const & time_to_label,
    std::vector<std::string> class_names,
    std::size_t num_classes,
    std::span<TimeFrameIndex const> row_times)
{
    AssembledLabels result;
    result.num_classes = num_classes;
    result.class_names = std::move(class_names);
    result.labels.set_size(row_times.size());
    result.unlabeled_count = 0;

    for (std::size_t i = 0; i < row_times.size(); ++i) {
        auto it = time_to_label.find(row_times[i].getValue());
        if (it != time_to_label.end()) {
            result.labels(i) = it->second;
        } else {
            result.labels(i) = num_classes;  // sentinel for unlabeled
            ++result.unlabeled_count;
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// assembleLabelsFromIntervals
// ============================================================================

AssembledLabels assembleLabelsFromIntervals(
    DigitalIntervalSeries const & intervals,
    TimeFrame const & source_time_frame,
    std::span<TimeFrameIndex const> row_times,
    LabelFromIntervals const & config)
{
    if (row_times.empty()) {
        throw std::invalid_argument(
            "row_times must not be empty for label assembly");
    }

    AssembledLabels result;
    result.num_classes = 2;
    result.class_names = {config.negative_class_name, config.positive_class_name};
    result.labels.set_size(row_times.size());
    result.unlabeled_count = 0;  // binary mode: every row gets a label

    for (std::size_t i = 0; i < row_times.size(); ++i) {
        bool inside = intervals.hasIntervalAtTime(row_times[i], source_time_frame);
        result.labels(i) = inside ? 1 : 0;
    }

    return result;
}

// ============================================================================
// assembleLabelsFromTimeEntityGroups
// ============================================================================

AssembledLabels assembleLabelsFromTimeEntityGroups(
    EntityGroupManager const & groups,
    EntityRegistry const & registry,
    std::span<TimeFrameIndex const> row_times,
    LabelFromTimeEntityGroups const & config)
{
    if (row_times.empty()) {
        throw std::invalid_argument(
            "row_times must not be empty for label assembly");
    }
    if (config.class_groups.empty()) {
        throw std::invalid_argument(
            "class_groups must not be empty for label assembly");
    }

    auto class_names = buildClassNames(groups, config.class_groups);

    auto time_to_label = buildTimeLabelMap(
        groups, registry, config.class_groups,
        config.time_key,
        EntityKind::TimeEntity);

    return labelsFromTimeLabelMap(
        time_to_label, std::move(class_names),
        config.class_groups.size(), row_times);
}

// ============================================================================
// assembleLabelsFromDataEntityGroups
// ============================================================================

AssembledLabels assembleLabelsFromDataEntityGroups(
    EntityGroupManager const & groups,
    EntityRegistry const & registry,
    std::span<TimeFrameIndex const> row_times,
    LabelFromDataEntityGroups const & config)
{
    if (row_times.empty()) {
        throw std::invalid_argument(
            "row_times must not be empty for label assembly");
    }
    if (config.class_groups.empty()) {
        throw std::invalid_argument(
            "class_groups must not be empty for label assembly");
    }

    auto class_names = buildClassNames(groups, config.class_groups);

    // For data entity groups, match on data_key with any entity kind
    auto time_to_label = buildTimeLabelMap(
        groups, registry, config.class_groups,
        config.data_key,
        std::nullopt); // no kind filter

    return labelsFromTimeLabelMap(
        time_to_label, std::move(class_names),
        config.class_groups.size(), row_times);
}

// ============================================================================
// Utility functions
// ============================================================================

std::vector<std::string> getClassNamesFromGroups(
    EntityGroupManager const & groups,
    std::vector<GroupId> const & class_group_ids)
{
    return buildClassNames(groups, class_group_ids);
}

std::size_t countRowsInsideIntervals(
    DigitalIntervalSeries const & intervals,
    TimeFrame const & source_time_frame,
    std::span<TimeFrameIndex const> row_times)
{
    std::size_t count = 0;
    for (auto const & t : row_times) {
        if (intervals.hasIntervalAtTime(t, source_time_frame)) {
            ++count;
        }
    }
    return count;
}

} // namespace MLCore