#ifndef ENTITYRELATIONSHIPMANAGER_HPP
#define ENTITYRELATIONSHIPMANAGER_HPP

#include "Entity/EntityTypes.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Type of relationship between entities.
 */
enum class RelationshipType : std::uint8_t {
    ParentChild = 0,  ///< Parent-child relationship (e.g., mask time series -> calculated area values)
    Derived = 1,      ///< Derived data relationship (e.g., input data -> processed output)
    Linked = 2        ///< General linkage (e.g., correlated entities)
};

/**
 * @brief Descriptor for a relationship between two entities.
 */
struct EntityRelationship {
    EntityId from_entity;      ///< Source entity ID
    EntityId to_entity;        ///< Target entity ID
    RelationshipType type;     ///< Type of relationship
    std::string label;         ///< Optional label describing the relationship
};

/**
 * @brief Manages sparse relationships between entities for efficient querying.
 * 
 * @details This class provides mechanisms to store and query parent-child and other
 * relationships between entities. It's optimized for sparse relationships where only
 * a subset of entities have relationships. Uses bidirectional hash maps for O(1) lookups.
 * 
 * Common use cases:
 * - Tracking processed data derived from source data (mask area from mask time series)
 * - Linking entities across different data representations
 * - Building entity hierarchies for visualization and navigation
 */
class EntityRelationshipManager {
public:
    /**
     * @brief Add a relationship between two entities.
     * @param from_entity The source entity
     * @param to_entity The target entity
     * @param type The type of relationship
     * @param label Optional label describing the relationship
     * @return True if the relationship was added, false if it already exists
     */
    bool addRelationship(EntityId from_entity, 
                        EntityId to_entity, 
                        RelationshipType type,
                        std::string const & label = "");

    /**
     * @brief Remove a specific relationship between two entities.
     * @param from_entity The source entity
     * @param to_entity The target entity
     * @param type The type of relationship to remove
     * @return True if the relationship existed and was removed, false otherwise
     */
    bool removeRelationship(EntityId from_entity, EntityId to_entity, RelationshipType type);

    /**
     * @brief Remove all relationships of a specific entity.
     * @param entity_id The entity whose relationships should be removed
     * @return Number of relationships removed
     */
    std::size_t removeAllRelationships(EntityId entity_id);

    /**
     * @brief Check if a relationship exists between two entities.
     * @param from_entity The source entity
     * @param to_entity The target entity
     * @param type The type of relationship to check
     * @return True if the relationship exists, false otherwise
     */
    [[nodiscard]] bool hasRelationship(EntityId from_entity, 
                                       EntityId to_entity, 
                                       RelationshipType type) const;

    /**
     * @brief Get all entities that the given entity has outgoing relationships to.
     * @param entity_id The source entity
     * @param type Optional filter for relationship type
     * @return Vector of target entity IDs
     */
    [[nodiscard]] std::vector<EntityId> getRelatedEntities(
        EntityId entity_id, 
        std::optional<RelationshipType> type = std::nullopt) const;

    /**
     * @brief Get all entities that have relationships pointing to the given entity.
     * @param entity_id The target entity
     * @param type Optional filter for relationship type
     * @return Vector of source entity IDs
     */
    [[nodiscard]] std::vector<EntityId> getReverseRelatedEntities(
        EntityId entity_id,
        std::optional<RelationshipType> type = std::nullopt) const;

    /**
     * @brief Get all parent entities of a given entity.
     * Convenience method for getting entities with ParentChild relationship pointing to this entity.
     * @param entity_id The child entity
     * @return Vector of parent entity IDs
     */
    [[nodiscard]] std::vector<EntityId> getParents(EntityId entity_id) const;

    /**
     * @brief Get all child entities of a given entity.
     * Convenience method for getting entities with ParentChild relationship from this entity.
     * @param entity_id The parent entity
     * @return Vector of child entity IDs
     */
    [[nodiscard]] std::vector<EntityId> getChildren(EntityId entity_id) const;

    /**
     * @brief Get detailed information about all relationships for an entity.
     * @param entity_id The entity to query
     * @param include_reverse If true, also includes relationships where this entity is the target
     * @return Vector of EntityRelationship descriptors
     */
    [[nodiscard]] std::vector<EntityRelationship> getRelationshipDetails(
        EntityId entity_id,
        bool include_reverse = false) const;

    /**
     * @brief Get the total number of relationships stored.
     * @return Number of relationships
     */
    [[nodiscard]] std::size_t getRelationshipCount() const;

    /**
     * @brief Get the number of entities that have at least one relationship.
     * @return Number of entities with relationships
     */
    [[nodiscard]] std::size_t getEntityCount() const;

    /**
     * @brief Clear all relationships (session reset).
     */
    void clear();

private:
    // Internal key for storing relationship information
    struct RelationshipKey {
        EntityId from_entity;
        EntityId to_entity;
        RelationshipType type;

        bool operator==(RelationshipKey const & other) const {
            return from_entity == other.from_entity &&
                   to_entity == other.to_entity &&
                   type == other.type;
        }
    };

    struct RelationshipKeyHash {
        std::size_t operator()(RelationshipKey const & k) const noexcept {
            std::size_t const h1 = std::hash<EntityId>{}(k.from_entity);
            std::size_t const h2 = std::hash<EntityId>{}(k.to_entity);
            std::size_t const h3 = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(k.type));
            
            // Combine hashes using a well-known hash combination technique
            std::size_t seed = h1;
            seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    // Forward lookup: from_entity -> set of (to_entity, type) pairs
    std::unordered_map<EntityId, std::unordered_set<RelationshipKey, RelationshipKeyHash>> m_forward_relationships;
    
    // Reverse lookup: to_entity -> set of (from_entity, type) pairs
    std::unordered_map<EntityId, std::unordered_set<RelationshipKey, RelationshipKeyHash>> m_reverse_relationships;
    
    // Store relationship labels
    std::unordered_map<RelationshipKey, std::string, RelationshipKeyHash> m_relationship_labels;
};

#endif // ENTITYRELATIONSHIPMANAGER_HPP
