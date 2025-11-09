#ifndef ENTITYGROUPMANAGER_HPP
#define ENTITYGROUPMANAGER_HPP

#include "Entity/EntityTypes.hpp"

#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DataManager/Observer/Observer_Data.hpp"

/**
 * @brief Unique identifier for user-defined groups of entities.
 */
using GroupId = std::uint64_t;

/**
 * @brief Descriptor for a user-defined group containing metadata.
 */
struct GroupDescriptor {
    GroupId id;
    std::string name;
    std::string description;
    std::size_t entity_count;
};

/**
 * @brief Manages user-defined groups of EntityIds for cross-data linking and visualization.
 * 
 * @details This class provides fast lookup and manipulation of entity groups to support
 * linking data across different formats (e.g., from TableView rows back to raw LineData).
 * Optimized for hundreds of thousands of entities with O(1) group operations.
 */
class EntityGroupManager {
public:
    /**
     * @brief Create a new empty group with the given name.
     * @param name User-friendly name for the group
     * @param description Optional description for the group
     * @return GroupId of the created group
     */
    [[nodiscard]] GroupId createGroup(std::string const & name, std::string const & description = "");

    /**
     * @brief Delete a group and all its entities.
     * @param group_id The group to delete
     * @return True if the group existed and was deleted, false otherwise
     */
    bool deleteGroup(GroupId group_id);

    /**
     * @brief Check if a group exists.
     * @param group_id The group to check
     * @return True if the group exists, false otherwise
     */
    [[nodiscard]] bool hasGroup(GroupId group_id) const;

    /**
     * @brief Get information about a group.
     * @param group_id The group to query
     * @return GroupDescriptor if the group exists, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<GroupDescriptor> getGroupDescriptor(GroupId group_id) const;

    /**
     * @brief Update the name and/or description of a group.
     * @param group_id The group to update
     * @param name New name for the group
     * @param description New description for the group
     * @return True if the group existed and was updated, false otherwise
     */
    bool updateGroup(GroupId group_id, std::string const & name, std::string const & description = "");

    /**
     * @brief Get all group IDs.
     * @return Vector of all existing group IDs
     */
    [[nodiscard]] std::vector<GroupId> getAllGroupIds() const;

    /**
     * @brief Get all group descriptors.
     * @return Vector of all group descriptors
     */
    [[nodiscard]] std::vector<GroupDescriptor> getAllGroupDescriptors() const;

    // ========== Entity Management ==========

    /**
     * @brief Add a single entity to a group.
     * @param group_id The group to add to
     * @param entity_id The entity to add
     * @return True if the entity was added (group exists and entity wasn't already in group), false otherwise
     */
    bool addEntityToGroup(GroupId group_id, EntityId entity_id);

    /**
     * @brief Add multiple entities to a group.
     * @param group_id The group to add to
     * @param entity_ids The entities to add
     * @return Number of entities successfully added (excludes duplicates and invalid group)
     */
    std::size_t addEntitiesToGroup(GroupId group_id, std::ranges::range auto const & entity_ids) {
        auto group_it = m_group_entities.find(group_id);
        if (group_it == m_group_entities.end()) {
            return 0;
        }
    
        auto & group_set = group_it->second;
        // Reserve to reduce rehashing when adding many entities
        group_set.reserve(group_set.size() + entity_ids.size());
        m_entity_groups.reserve(m_entity_groups.size() + entity_ids.size());
    
        std::size_t added_count = 0;
        for (EntityId const entity_id: entity_ids) {
            auto [ignored, inserted] = group_set.insert(entity_id);
            if (!inserted) {
                continue;
            }
    
            auto [rev_it, created] = m_entity_groups.try_emplace(entity_id);
            (void) created;
            rev_it->second.insert(group_id);
            ++added_count;
        }
    
        return added_count;
    }

    /**
     * @brief Remove a single entity from a group.
     * @param group_id The group to remove from
     * @param entity_id The entity to remove
     * @return True if the entity was removed (group exists and entity was in group), false otherwise
     */
    bool removeEntityFromGroup(GroupId group_id, EntityId entity_id);

    /**
     * @brief Remove multiple entities from a group.
     * @param group_id The group to remove from
     * @param entity_ids The entities to remove
     * @return Number of entities successfully removed
     */
    std::size_t removeEntitiesFromGroup(GroupId group_id, std::vector<EntityId> const & entity_ids);

    /**
     * @brief Get all entities in a group.
     * @param group_id The group to query
     * @return Vector of EntityIds in the group, empty if group doesn't exist
     */
    [[nodiscard]] std::vector<EntityId> getEntitiesInGroup(GroupId group_id) const;

    /**
     * @brief Check if an entity is in a specific group.
     * @param group_id The group to check
     * @param entity_id The entity to check
     * @return True if the entity is in the group, false otherwise
     */
    [[nodiscard]] bool isEntityInGroup(GroupId group_id, EntityId entity_id) const;

    /**
     * @brief Get all groups that contain a specific entity.
     * @param entity_id The entity to query
     * @return Vector of GroupIds that contain the entity
     */
    [[nodiscard]] std::vector<GroupId> getGroupsContainingEntity(EntityId entity_id) const;

    /**
     * @brief Get the number of entities in a group.
     * @param group_id The group to query
     * @return Number of entities in the group, 0 if group doesn't exist
     */
    [[nodiscard]] std::size_t getGroupSize(GroupId group_id) const;

    /**
     * @brief Clear all entities from a group without deleting the group.
     * @param group_id The group to clear
     * @return True if the group existed and was cleared, false otherwise
     */
    bool clearGroup(GroupId group_id);

    /**
     * @brief Clear all groups and entities (session reset).
     */
    void clear();

    /**
     * @brief Get total number of groups.
     * @return Number of groups
     */
    [[nodiscard]] std::size_t getGroupCount() const;

    /**
     * @brief Get total number of unique entities across all groups.
     * @return Number of unique entities
     */
    [[nodiscard]] std::size_t getTotalEntityCount() const;

    // --- Observer integration for UI updates ---
    /**
     * @brief Access the observer sink for group changes.
     * Subscribers will be notified when notifyGroupsChanged() is called.
     */
    [[nodiscard]] ObserverData & getGroupObservers() { return m_group_observers; }

    /**
     * @brief Notify observers that group membership or descriptors changed.
     * Call this once after bulk updates to avoid excessive UI refreshes.
     */
    void notifyGroupsChanged() { m_group_observers.notifyObservers(); }

private:
    // Group metadata
    std::unordered_map<GroupId, std::string> m_group_names;
    std::unordered_map<GroupId, std::string> m_group_descriptions;
    
    // Group to entities mapping (one-to-many)
    std::unordered_map<GroupId, std::unordered_set<EntityId>> m_group_entities;
    
    // Entity to groups mapping (many-to-many reverse lookup)
    std::unordered_map<EntityId, std::unordered_set<GroupId>> m_entity_groups;
    
    GroupId m_next_group_id{1}; // Start at 1, 0 reserved for invalid/null

    // Observer for bulk change notifications
    ObserverData m_group_observers;
};

#endif // ENTITYGROUPMANAGER_HPP
