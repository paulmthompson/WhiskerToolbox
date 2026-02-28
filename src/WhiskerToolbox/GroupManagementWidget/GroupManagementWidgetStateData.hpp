#ifndef GROUP_MANAGEMENT_WIDGET_STATE_DATA_HPP
#define GROUP_MANAGEMENT_WIDGET_STATE_DATA_HPP

/**
 * @file GroupManagementWidgetStateData.hpp
 * @brief Serializable state data structure for GroupManagementWidget
 *
 * Separated from GroupManagementWidgetState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see GroupManagementWidgetState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <string>
#include <vector>

/**
 * @brief Serializable data structure for GroupManagementWidgetState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct GroupManagementWidgetStateData {
    int selected_group_id = -1;                 ///< Currently selected group ID (-1 = none)
    std::vector<int> expanded_groups;           ///< List of expanded group IDs (for future tree view)
    std::string instance_id;                    ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Group Manager"; ///< User-visible name
};

#endif // GROUP_MANAGEMENT_WIDGET_STATE_DATA_HPP
