#ifndef INDEX_GROUPING_HPP
#define INDEX_GROUPING_HPP

#include "Entity/EntityGroupManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

/**
 * @brief Templated function to group data by vector index
 * 
 * This function operates on time-series data structures that store vectors of entries
 * (e.g., LineEntry, PointEntry, MaskEntry) at each timestamp. It creates groups based
 * on the position of elements within the vectors.
 * 
 * The algorithm:
 * 1. Finds the maximum number of elements at any single timestamp
 * 2. Creates that many groups (group 0, group 1, ..., group N-1)
 * 3. Assigns all elements at vector index 0 to group 0, index 1 to group 1, etc.
 * 
 * This is useful for:
 * - Organizing tracked features by their detection order
 * - Maintaining consistent identity across frames when detection order is stable
 * - Grouping whiskers, body parts, or other tracked entities by their index
 * 
 * @tparam DataMap The type of the data map (e.g., std::map<TimeFrameIndex, std::vector<LineEntry>>)
 * @tparam Entry The entry type (e.g., LineEntry, PointEntry, MaskEntry)
 * @param data_map Reference to the data map containing vectors of entries
 * @param group_manager The EntityGroupManager to add groups to
 * @param group_name_prefix Prefix for created group names (e.g., "Whisker", "Point")
 * @param group_description_template Template for group descriptions (can include {} for index)
 * @return Number of groups created
 */
template<typename DataMap, typename Entry>
inline std::size_t groupByIndex(DataMap const & data_map,
                                EntityGroupManager * group_manager,
                                std::string const & group_name_prefix,
                                std::string const & group_description_template = "Group {} - elements at vector index {}") {
    if (!group_manager) {
        return 0;
    }

    // Step 1: Find the maximum number of elements at any timestamp
    std::size_t max_elements = 0;
    for (auto const & [time, entries]: data_map) {
        max_elements = std::max(max_elements, entries.size());
    }

    if (max_elements == 0) {
        return 0;// No data to group
    }

    // Step 2: Create groups for each index position
    std::vector<GroupId> group_ids;
    group_ids.reserve(max_elements);

    for (std::size_t i = 0; i < max_elements; ++i) {
        std::string const group_name = group_name_prefix + " " + std::to_string(i);
        std::string description = group_description_template;
        
        // Replace {} placeholders with the index
        if (size_t pos = description.find("{}"); pos != std::string::npos) {
            description.replace(pos, 2, std::to_string(i));
            // Replace second occurrence if it exists
            if (size_t pos2 = description.find("{}"); pos2 != std::string::npos) {
                description.replace(pos2, 2, std::to_string(i));
            }
        }

        GroupId const group_id = group_manager->createGroup(group_name, description);
        group_ids.push_back(group_id);
    }

    // Step 3: Assign entities to groups based on their vector index
    for (auto const & [time, entries]: data_map) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            EntityId entity_id = entries[i].entity_id;
            if (entity_id != 0) {// Skip invalid entities
                group_manager->addEntityToGroup(group_ids[i], entity_id);
            }
        }
    }

    // Notify observers of group changes
    group_manager->notifyGroupsChanged();

    return max_elements;
}

#endif// INDEX_GROUPING_HPP
