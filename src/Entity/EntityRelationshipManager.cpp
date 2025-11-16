#include "EntityRelationshipManager.hpp"

#include <algorithm>

bool EntityRelationshipManager::addRelationship(EntityId from_entity,
                                                EntityId to_entity,
                                                RelationshipType type,
                                                std::string const & label) {
    RelationshipKey key{from_entity, to_entity, type};
    
    // Check if relationship already exists
    auto forward_it = m_forward_relationships.find(from_entity);
    if (forward_it != m_forward_relationships.end()) {
        if (forward_it->second.find(key) != forward_it->second.end()) {
            return false;  // Relationship already exists
        }
    }
    
    // Add to forward lookup
    auto [fwd_it, fwd_created] = m_forward_relationships.try_emplace(from_entity);
    fwd_it->second.insert(key);
    
    // Add to reverse lookup
    auto [rev_it, rev_created] = m_reverse_relationships.try_emplace(to_entity);
    rev_it->second.insert(key);
    
    // Store label if provided
    if (!label.empty()) {
        m_relationship_labels[key] = label;
    }
    
    return true;
}

bool EntityRelationshipManager::removeRelationship(EntityId from_entity,
                                                   EntityId to_entity,
                                                   RelationshipType type) {
    RelationshipKey key{from_entity, to_entity, type};
    
    // Remove from forward lookup
    auto forward_it = m_forward_relationships.find(from_entity);
    if (forward_it == m_forward_relationships.end()) {
        return false;
    }
    
    auto rel_it = forward_it->second.find(key);
    if (rel_it == forward_it->second.end()) {
        return false;  // Relationship doesn't exist
    }
    
    forward_it->second.erase(rel_it);
    
    // Clean up empty set
    if (forward_it->second.empty()) {
        m_forward_relationships.erase(forward_it);
    }
    
    // Remove from reverse lookup
    auto reverse_it = m_reverse_relationships.find(to_entity);
    if (reverse_it != m_reverse_relationships.end()) {
        reverse_it->second.erase(key);
        
        // Clean up empty set
        if (reverse_it->second.empty()) {
            m_reverse_relationships.erase(reverse_it);
        }
    }
    
    // Remove label if it exists
    m_relationship_labels.erase(key);
    
    return true;
}

std::size_t EntityRelationshipManager::removeAllRelationships(EntityId entity_id) {
    std::size_t removed_count = 0;
    
    // Remove all forward relationships (where entity_id is the source)
    auto forward_it = m_forward_relationships.find(entity_id);
    if (forward_it != m_forward_relationships.end()) {
        for (auto const & key : forward_it->second) {
            // Remove from reverse lookup
            auto reverse_it = m_reverse_relationships.find(key.to_entity);
            if (reverse_it != m_reverse_relationships.end()) {
                reverse_it->second.erase(key);
                if (reverse_it->second.empty()) {
                    m_reverse_relationships.erase(reverse_it);
                }
            }
            
            // Remove label
            m_relationship_labels.erase(key);
            ++removed_count;
        }
        m_forward_relationships.erase(forward_it);
    }
    
    // Remove all reverse relationships (where entity_id is the target)
    auto reverse_it = m_reverse_relationships.find(entity_id);
    if (reverse_it != m_reverse_relationships.end()) {
        for (auto const & key : reverse_it->second) {
            // Remove from forward lookup
            auto fwd_it = m_forward_relationships.find(key.from_entity);
            if (fwd_it != m_forward_relationships.end()) {
                fwd_it->second.erase(key);
                if (fwd_it->second.empty()) {
                    m_forward_relationships.erase(fwd_it);
                }
            }
            
            // Remove label
            m_relationship_labels.erase(key);
            ++removed_count;
        }
        m_reverse_relationships.erase(reverse_it);
    }
    
    return removed_count;
}

bool EntityRelationshipManager::hasRelationship(EntityId from_entity,
                                                EntityId to_entity,
                                                RelationshipType type) const {
    auto forward_it = m_forward_relationships.find(from_entity);
    if (forward_it == m_forward_relationships.end()) {
        return false;
    }
    
    RelationshipKey key{from_entity, to_entity, type};
    return forward_it->second.find(key) != forward_it->second.end();
}

std::vector<EntityId> EntityRelationshipManager::getRelatedEntities(
    EntityId entity_id,
    std::optional<RelationshipType> type) const {
    
    auto forward_it = m_forward_relationships.find(entity_id);
    if (forward_it == m_forward_relationships.end()) {
        return {};
    }
    
    std::vector<EntityId> result;
    result.reserve(forward_it->second.size());
    
    for (auto const & key : forward_it->second) {
        if (!type.has_value() || key.type == type.value()) {
            result.push_back(key.to_entity);
        }
    }
    
    return result;
}

std::vector<EntityId> EntityRelationshipManager::getReverseRelatedEntities(
    EntityId entity_id,
    std::optional<RelationshipType> type) const {
    
    auto reverse_it = m_reverse_relationships.find(entity_id);
    if (reverse_it == m_reverse_relationships.end()) {
        return {};
    }
    
    std::vector<EntityId> result;
    result.reserve(reverse_it->second.size());
    
    for (auto const & key : reverse_it->second) {
        if (!type.has_value() || key.type == type.value()) {
            result.push_back(key.from_entity);
        }
    }
    
    return result;
}

std::vector<EntityId> EntityRelationshipManager::getParents(EntityId entity_id) const {
    return getReverseRelatedEntities(entity_id, RelationshipType::ParentChild);
}

std::vector<EntityId> EntityRelationshipManager::getChildren(EntityId entity_id) const {
    return getRelatedEntities(entity_id, RelationshipType::ParentChild);
}

std::vector<EntityRelationship> EntityRelationshipManager::getRelationshipDetails(
    EntityId entity_id,
    bool include_reverse) const {
    
    std::vector<EntityRelationship> result;
    
    // Get forward relationships
    auto forward_it = m_forward_relationships.find(entity_id);
    if (forward_it != m_forward_relationships.end()) {
        for (auto const & key : forward_it->second) {
            EntityRelationship rel{
                .from_entity = key.from_entity,
                .to_entity = key.to_entity,
                .type = key.type,
                .label = ""
            };
            
            auto label_it = m_relationship_labels.find(key);
            if (label_it != m_relationship_labels.end()) {
                rel.label = label_it->second;
            }
            
            result.push_back(rel);
        }
    }
    
    // Get reverse relationships if requested
    if (include_reverse) {
        auto reverse_it = m_reverse_relationships.find(entity_id);
        if (reverse_it != m_reverse_relationships.end()) {
            for (auto const & key : reverse_it->second) {
                EntityRelationship rel{
                    .from_entity = key.from_entity,
                    .to_entity = key.to_entity,
                    .type = key.type,
                    .label = ""
                };
                
                auto label_it = m_relationship_labels.find(key);
                if (label_it != m_relationship_labels.end()) {
                    rel.label = label_it->second;
                }
                
                result.push_back(rel);
            }
        }
    }
    
    return result;
}

std::size_t EntityRelationshipManager::getRelationshipCount() const {
    std::size_t count = 0;
    for (auto const & [entity_id, relationships] : m_forward_relationships) {
        (void)entity_id;  // Suppress unused variable warning
        count += relationships.size();
    }
    return count;
}

std::size_t EntityRelationshipManager::getEntityCount() const {
    std::unordered_set<EntityId> unique_entities;
    
    for (auto const & [entity_id, relationships] : m_forward_relationships) {
        unique_entities.insert(entity_id);
        for (auto const & key : relationships) {
            unique_entities.insert(key.to_entity);
        }
    }
    
    return unique_entities.size();
}

void EntityRelationshipManager::clear() {
    m_forward_relationships.clear();
    m_reverse_relationships.clear();
    m_relationship_labels.clear();
}
