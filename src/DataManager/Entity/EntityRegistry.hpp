#ifndef ENTITYREGISTRY_HPP
#define ENTITYREGISTRY_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <optional>
#include <unordered_map>

/**
 * @brief Central registry of session-scoped entity identifiers.
 *
 * @details Provides deterministic, session-local mapping between
 * (data_key, kind, time, local_index) tuples and opaque EntityId values.
 */
class EntityRegistry {
public:
    /**
     * @brief Get or create an EntityId for the tuple.
     * @pre local_index >= 0
     */
    [[nodiscard]] EntityId ensureId(std::string const & data_key,
                                    EntityKind kind,
                                    TimeFrameIndex const & time,
                                    int local_index);

    /**
     * @brief Lookup descriptor for an EntityId.
     */
    [[nodiscard]] std::optional<EntityDescriptor> get(EntityId id) const;

    /**
     * @brief Clear all registered entities (session reset).
     */
    void clear();

private:
    std::unordered_map<EntityTupleKey, EntityId, EntityTupleKeyHash> m_tuple_to_id;
    std::unordered_map<EntityId, EntityDescriptor> m_id_to_descriptor;
    EntityId m_next_id {0};
};

#endif // ENTITYREGISTRY_HPP


