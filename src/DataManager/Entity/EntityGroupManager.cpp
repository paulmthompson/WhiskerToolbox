#include "EntityGroupManager.hpp"

#include <algorithm>
#include <cassert>

// ========== Group Management ==========

GroupId EntityGroupManager::createGroup(std::string const & name, std::string const & description) {
    GroupId const id = m_next_group_id++;
    
    m_group_names.emplace(id, name);
    m_group_descriptions.emplace(id, description);
    m_group_entities.emplace(id, std::unordered_set<EntityId>{});
    
    return id;
}

bool EntityGroupManager::deleteGroup(GroupId group_id) {
    auto it = m_group_entities.find(group_id);
    if (it == m_group_entities.end()) {
        return false;
    }
    
    // Remove this group from all entities' reverse lookup
    for (EntityId entity_id : it->second) {
        auto entity_it = m_entity_groups.find(entity_id);
        if (entity_it != m_entity_groups.end()) {
            entity_it->second.erase(group_id);
            // Clean up empty entity entries
            if (entity_it->second.empty()) {
                m_entity_groups.erase(entity_it);
            }
        }
    }
    
    // Remove group data
    m_group_entities.erase(it);
    m_group_names.erase(group_id);
    m_group_descriptions.erase(group_id);
    
    return true;
}

bool EntityGroupManager::hasGroup(GroupId group_id) const {
    return m_group_entities.find(group_id) != m_group_entities.end();
}

std::optional<GroupDescriptor> EntityGroupManager::getGroupDescriptor(GroupId group_id) const {
    auto entities_it = m_group_entities.find(group_id);
    if (entities_it == m_group_entities.end()) {
        return std::nullopt;
    }
    
    auto name_it = m_group_names.find(group_id);
    auto desc_it = m_group_descriptions.find(group_id);
    
    return GroupDescriptor{
        group_id,
        name_it != m_group_names.end() ? name_it->second : "",
        desc_it != m_group_descriptions.end() ? desc_it->second : "",
        entities_it->second.size()
    };
}

bool EntityGroupManager::updateGroup(GroupId group_id, std::string const & name, std::string const & description) {
    if (!hasGroup(group_id)) {
        return false;
    }
    
    m_group_names[group_id] = name;
    m_group_descriptions[group_id] = description;
    
    return true;
}

std::vector<GroupId> EntityGroupManager::getAllGroupIds() const {
    std::vector<GroupId> group_ids;
    group_ids.reserve(m_group_entities.size());
    
    for (auto const & [group_id, entities] : m_group_entities) {
        (void)entities; // Suppress unused variable warning
        group_ids.push_back(group_id);
    }
    
    return group_ids;
}

std::vector<GroupDescriptor> EntityGroupManager::getAllGroupDescriptors() const {
    std::vector<GroupDescriptor> descriptors;
    descriptors.reserve(m_group_entities.size());
    
    for (auto const & [group_id, entities] : m_group_entities) {
        auto name_it = m_group_names.find(group_id);
        auto desc_it = m_group_descriptions.find(group_id);
        
        descriptors.push_back(GroupDescriptor{
            group_id,
            name_it != m_group_names.end() ? name_it->second : "",
            desc_it != m_group_descriptions.end() ? desc_it->second : "",
            entities.size()
        });
    }
    
    return descriptors;
}

// ========== Entity Management ==========

bool EntityGroupManager::addEntityToGroup(GroupId group_id, EntityId entity_id) {
    auto group_it = m_group_entities.find(group_id);
    if (group_it == m_group_entities.end()) {
        return false;
    }
    
    // Check if entity is already in group
    if (group_it->second.find(entity_id) != group_it->second.end()) {
        return false; // Already exists
    }
    
    // Add entity to group
    group_it->second.insert(entity_id);
    
    // Add group to entity's reverse lookup
    m_entity_groups[entity_id].insert(group_id);
    
    return true;
}

std::size_t EntityGroupManager::addEntitiesToGroup(GroupId group_id, std::vector<EntityId> const & entity_ids) {
    auto group_it = m_group_entities.find(group_id);
    if (group_it == m_group_entities.end()) {
        return 0;
    }
    
    std::size_t added_count = 0;
    for (EntityId entity_id : entity_ids) {
        // Check if entity is already in group
        if (group_it->second.find(entity_id) == group_it->second.end()) {
            // Add entity to group
            group_it->second.insert(entity_id);
            
            // Add group to entity's reverse lookup
            m_entity_groups[entity_id].insert(group_id);
            
            ++added_count;
        }
    }
    
    return added_count;
}

bool EntityGroupManager::removeEntityFromGroup(GroupId group_id, EntityId entity_id) {
    auto group_it = m_group_entities.find(group_id);
    if (group_it == m_group_entities.end()) {
        return false;
    }
    
    // Check if entity is in group
    auto entity_it = group_it->second.find(entity_id);
    if (entity_it == group_it->second.end()) {
        return false; // Not in group
    }
    
    // Remove entity from group
    group_it->second.erase(entity_it);
    
    // Remove group from entity's reverse lookup
    auto reverse_it = m_entity_groups.find(entity_id);
    if (reverse_it != m_entity_groups.end()) {
        reverse_it->second.erase(group_id);
        // Clean up empty entity entries
        if (reverse_it->second.empty()) {
            m_entity_groups.erase(reverse_it);
        }
    }
    
    return true;
}

std::size_t EntityGroupManager::removeEntitiesFromGroup(GroupId group_id, std::vector<EntityId> const & entity_ids) {
    auto group_it = m_group_entities.find(group_id);
    if (group_it == m_group_entities.end()) {
        return 0;
    }
    
    std::size_t removed_count = 0;
    for (EntityId entity_id : entity_ids) {
        // Check if entity is in group
        auto entity_it = group_it->second.find(entity_id);
        if (entity_it != group_it->second.end()) {
            // Remove entity from group
            group_it->second.erase(entity_it);
            
            // Remove group from entity's reverse lookup
            auto reverse_it = m_entity_groups.find(entity_id);
            if (reverse_it != m_entity_groups.end()) {
                reverse_it->second.erase(group_id);
                // Clean up empty entity entries
                if (reverse_it->second.empty()) {
                    m_entity_groups.erase(reverse_it);
                }
            }
            
            ++removed_count;
        }
    }
    
    return removed_count;
}

std::vector<EntityId> EntityGroupManager::getEntitiesInGroup(GroupId group_id) const {
    auto it = m_group_entities.find(group_id);
    if (it == m_group_entities.end()) {
        return {};
    }
    
    std::vector<EntityId> entities;
    entities.reserve(it->second.size());
    
    for (EntityId entity_id : it->second) {
        entities.push_back(entity_id);
    }
    
    return entities;
}

bool EntityGroupManager::isEntityInGroup(GroupId group_id, EntityId entity_id) const {
    auto it = m_group_entities.find(group_id);
    if (it == m_group_entities.end()) {
        return false;
    }
    
    return it->second.find(entity_id) != it->second.end();
}

std::vector<GroupId> EntityGroupManager::getGroupsContainingEntity(EntityId entity_id) const {
    auto it = m_entity_groups.find(entity_id);
    if (it == m_entity_groups.end()) {
        return {};
    }
    
    std::vector<GroupId> groups;
    groups.reserve(it->second.size());
    
    for (GroupId group_id : it->second) {
        groups.push_back(group_id);
    }
    
    return groups;
}

std::size_t EntityGroupManager::getGroupSize(GroupId group_id) const {
    auto it = m_group_entities.find(group_id);
    if (it == m_group_entities.end()) {
        return 0;
    }
    
    return it->second.size();
}

bool EntityGroupManager::clearGroup(GroupId group_id) {
    auto group_it = m_group_entities.find(group_id);
    if (group_it == m_group_entities.end()) {
        return false;
    }
    
    // Remove this group from all entities' reverse lookup
    for (EntityId entity_id : group_it->second) {
        auto entity_it = m_entity_groups.find(entity_id);
        if (entity_it != m_entity_groups.end()) {
            entity_it->second.erase(group_id);
            // Clean up empty entity entries
            if (entity_it->second.empty()) {
                m_entity_groups.erase(entity_it);
            }
        }
    }
    
    // Clear the group's entities
    group_it->second.clear();
    
    return true;
}

void EntityGroupManager::clear() {
    m_group_names.clear();
    m_group_descriptions.clear();
    m_group_entities.clear();
    m_entity_groups.clear();
    m_next_group_id = 1;
}

std::size_t EntityGroupManager::getGroupCount() const {
    return m_group_entities.size();
}

std::size_t EntityGroupManager::getTotalEntityCount() const {
    return m_entity_groups.size();
}
