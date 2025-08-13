#include "EntityRegistry.hpp"

// Other project headers


// Third-party

// Standard
#include <cassert>

EntityId EntityRegistry::ensureId(std::string const & data_key,
                                  EntityKind kind,
                                  TimeFrameIndex const & time,
                                  int local_index) {
    assert(local_index >= 0);

    EntityTupleKey const key{data_key, kind, time.getValue(), local_index};
    auto it = m_tuple_to_id.find(key);
    if (it != m_tuple_to_id.end()) {
        return it->second;
    }

    EntityId const id = m_next_id++;
    m_tuple_to_id.emplace(key, id);
    m_id_to_descriptor.emplace(id, EntityDescriptor{data_key, kind, time.getValue(), local_index});
    return id;
}

std::optional<EntityDescriptor> EntityRegistry::get(EntityId id) const {
    auto it = m_id_to_descriptor.find(id);
    if (it == m_id_to_descriptor.end()) {
        return std::nullopt;
    }
    return it->second;
}

void EntityRegistry::clear() {
    m_tuple_to_id.clear();
    m_id_to_descriptor.clear();
    m_next_id = 1;
}


