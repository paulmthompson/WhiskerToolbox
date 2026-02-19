#ifndef MLCORE_LABELASSEMBLER_HPP
#define MLCORE_LABELASSEMBLER_HPP

/**
 * @file LabelAssembler.hpp
 * @brief Assembles label vectors for ML workflows from various data sources
 *
 * Converts label information from interval series, time-entity groups, or
 * data-entity groups into arma::Row<size_t> label vectors aligned with
 * feature tensor rows.
 *
 * Three labeling modes are supported:
 * 1. **FromIntervals**: Binary labeling from DigitalIntervalSeries
 *    (inside interval = 1, outside = 0)
 * 2. **FromTimeEntityGroups**: Multi-class labeling from groups of TimeEntity IDs
 *    (each group maps to one class)
 * 3. **FromDataEntityGroups**: Multi-class labeling from groups of data-object entities
 *    (each group maps to one class, matched by data_key)
 *
 * All assembly functions produce an AssembledLabels struct containing the label
 * vector, class names, class count, and an unlabeled count for group-based modes.
 *
 * Feature assembly is handled by TensorDesigner / TensorColumnBuilders.
 * The LabelAssembler only concerns itself with producing aligned labels.
 *
 */

#include <armadillo>

#include <cstddef>
#include <span>
#include <string>
#include <variant>
#include <vector>

class DigitalIntervalSeries;
class EntityGroupManager;
class EntityRegistry;
class TimeFrame;
struct TimeFrameIndex;
using GroupId = std::uint64_t;

namespace MLCore {

// ============================================================================
// Configuration types
// ============================================================================

/**
 * @brief Configuration for binary labeling from a DigitalIntervalSeries
 *
 * Frames inside any interval are labeled 1 (positive_class_name),
 * frames outside all intervals are labeled 0 (negative_class_name).
 */
struct LabelFromIntervals {
    std::string positive_class_name = "Inside"; ///< Name for class 1 (inside interval)
    std::string negative_class_name = "Outside";///< Name for class 0 (outside interval)
};

/**
 * @brief Configuration for multi-class labeling from TimeEntity groups
 *
 * Each group in class_groups maps to a class label (group[0] → label 0, etc.).
 * TimeEntity entities are matched by time_key and EntityKind::TimeEntity.
 */
struct LabelFromTimeEntityGroups {
    std::vector<GroupId> class_groups;///< group[i] → label i
    std::string time_key;             ///< TimeFrame key for entity lookup
};

/**
 * @brief Configuration for multi-class labeling from data-object entity groups
 *
 * Each group in class_groups maps to a class label. Entities are matched
 * by data_key (any EntityKind except TimeEntity).
 */
struct LabelFromDataEntityGroups {
    std::string data_key;             ///< DataManager key for the data object
    std::vector<GroupId> class_groups;///< group[i] → label i
};

/**
 * @brief Configuration variant for label assembly
 *
 * Select one of the three labeling modes.
 */
using LabelAssemblyConfig = std::variant<
        LabelFromIntervals,
        LabelFromTimeEntityGroups,
        LabelFromDataEntityGroups>;

// ============================================================================
// Result type
// ============================================================================

/**
 * @brief Result of label assembly
 *
 * Contains the label vector aligned with the feature tensor rows,
 * class name metadata, and diagnostics.
 */
struct AssembledLabels {
    /**
     * @brief Label per observation (0-indexed class labels)
     *
     * For binary (FromIntervals): values are 0 or 1.
     * For multi-class: values are 0..num_classes-1.
     * Unlabeled rows (group modes only) receive label = num_classes (sentinel).
     */
    arma::Row<std::size_t> labels;

    /**
     * @brief Name for each class (size = num_classes)
     *
     * class_names[i] is the human-readable name for label value i.
     */
    std::vector<std::string> class_names;

    /**
     * @brief Number of distinct classes
     */
    std::size_t num_classes{0};

    /**
     * @brief Number of rows that did not match any class group
     *
     * Always 0 for FromIntervals mode (every row is either inside or outside).
     * For group-based modes, this counts rows whose time value was not found
     * in any of the configured class groups.
     */
    std::size_t unlabeled_count{0};
};

// ============================================================================
// Assembly functions — low level (operate on data objects directly)
// ============================================================================

/**
 * @brief Assemble binary labels from a DigitalIntervalSeries
 *
 * For each row_time, checks if it falls inside any interval in the series.
 * Inside → label 1, outside → label 0. Every row receives a label.
 *
 * @param intervals The interval series defining the positive class
 * @param source_time_frame The TimeFrame that row_times are expressed in.
 *        This is typically the feature tensor's TimeFrame from RowDescriptor.
 * @param row_times The time indices of each feature tensor row
 * @param config Configuration with class names (defaults: "Outside", "Inside")
 * @return Binary labels aligned with row_times (unlabeled_count always 0)
 *
 * @pre row_times must not be empty
 * @throws std::invalid_argument if row_times is empty
 */
[[nodiscard]] AssembledLabels assembleLabelsFromIntervals(
        DigitalIntervalSeries const & intervals,
        TimeFrame const & source_time_frame,
        std::span<TimeFrameIndex const> row_times,
        LabelFromIntervals const & config = {});

/**
 * @brief Assemble multi-class labels from TimeEntity groups
 *
 * Retrieves entities from each group, looks up their EntityDescriptor in the
 * registry, and builds a mapping from TimeFrameIndex value → class label.
 * Only entities matching the configured time_key and EntityKind::TimeEntity
 * are considered.
 *
 * Rows whose time value is not found in any group receive a sentinel label
 * equal to num_classes and are counted as unlabeled.
 *
 * @param groups The entity group manager
 * @param registry The entity registry (for descriptor lookup)
 * @param row_times The time indices of each feature tensor row
 * @param config Configuration with group IDs and time key
 * @return Multi-class labels aligned with row_times
 *
 * @pre config.class_groups must not be empty
 * @pre row_times must not be empty
 * @throws std::invalid_argument if class_groups or row_times is empty
 */
[[nodiscard]] AssembledLabels assembleLabelsFromTimeEntityGroups(
        EntityGroupManager const & groups,
        EntityRegistry const & registry,
        std::span<TimeFrameIndex const> row_times,
        LabelFromTimeEntityGroups const & config);

/**
 * @brief Assemble multi-class labels from data-object entity groups
 *
 * Similar to assembleLabelsFromTimeEntityGroups but matches entities by
 * data_key rather than time_key. Accepts entities of any EntityKind
 * (LineEntity, PointEntity, EventEntity, etc.) as long as data_key matches.
 *
 * @param groups The entity group manager
 * @param registry The entity registry (for descriptor lookup)
 * @param row_times The time indices of each feature tensor row
 * @param config Configuration with data key and group IDs
 * @return Multi-class labels aligned with row_times
 *
 * @pre config.class_groups must not be empty
 * @pre row_times must not be empty
 * @throws std::invalid_argument if class_groups or row_times is empty
 */
[[nodiscard]] AssembledLabels assembleLabelsFromDataEntityGroups(
        EntityGroupManager const & groups,
        EntityRegistry const & registry,
        std::span<TimeFrameIndex const> row_times,
        LabelFromDataEntityGroups const & config);

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Retrieve human-readable class names from groups
 *
 * For each group ID, returns the group name from EntityGroupManager.
 * If a group doesn't exist, returns "Class_<id>" as fallback.
 *
 * @param groups The entity group manager
 * @param class_group_ids The group IDs to get names for
 * @return Vector of class names in the same order as class_group_ids
 */
[[nodiscard]] std::vector<std::string> getClassNamesFromGroups(
        EntityGroupManager const & groups,
        std::vector<GroupId> const & class_group_ids);

/**
 * @brief Count how many row_times fall inside intervals
 *
 * Utility for UI display — counts how many feature rows would be
 * labeled positive in a binary interval-based labeling scenario.
 *
 * @param intervals The interval series
 * @param source_time_frame The TimeFrame that row_times are expressed in
 * @param row_times The time indices to check
 * @return Number of row_times that fall inside at least one interval
 */
[[nodiscard]] std::size_t countRowsInsideIntervals(
        DigitalIntervalSeries const & intervals,
        TimeFrame const & source_time_frame,
        std::span<TimeFrameIndex const> row_times);

}// namespace MLCore

#endif// MLCORE_LABELASSEMBLER_HPP