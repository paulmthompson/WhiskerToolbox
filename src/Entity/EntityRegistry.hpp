#ifndef ENTITYREGISTRY_HPP
#define ENTITYREGISTRY_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <mutex>
#include <optional>
#include <unordered_map>

/**
 * @brief Central registry of session-scoped entity identifiers.
 *
 * @details Provides deterministic, session-local mapping between
 * (data_key, kind, time, local_index) tuples and opaque EntityId values.
 * Thread-safe for concurrent access.
 */
class EntityRegistry {
public:
    /**
     * @brief Get or create an EntityId for the tuple.
     * @pre local_index >= 0
     * @note Thread-safe
     */
    [[nodiscard]] EntityId ensureId(std::string const & data_key,
                                    EntityKind kind,
                                    TimeFrameIndex const & time,
                                    int local_index);

    /**
     * @brief Lookup descriptor for an EntityId.
     * @note Thread-safe
     */
    [[nodiscard]] std::optional<EntityDescriptor> get(EntityId id) const;

    /**
     * @brief Clear all registered entities (session reset).
     * @note Thread-safe
     */
    void clear();

private:
    mutable std::mutex m_mutex;  // Protects all member variables
    std::unordered_map<EntityTupleKey, EntityId, EntityTupleKeyHash> m_tuple_to_id;
    std::unordered_map<EntityId, EntityDescriptor> m_id_to_descriptor;
    EntityId m_next_id {1};  // Start from 1 since 0 is used as sentinel "no entity" value
};

#endif // ENTITYREGISTRY_HPP


