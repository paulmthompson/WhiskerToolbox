#include "EntityRegistry.hpp"

#include <cassert>
#include <mutex>

EntityId EntityRegistry::ensureId(std::string const & data_key,
                                  EntityKind kind,
                                  TimeFrameIndex const & time,
                                  int local_index) {
    assert(local_index >= 0);

    std::lock_guard<std::mutex> const lock(m_mutex);

    EntityTupleKey const key{data_key, kind, time.getValue(), local_index};
    auto it = m_tuple_to_id.find(key);
    if (it != m_tuple_to_id.end()) {
        return it->second;
    }

    // Allocate a fresh, unique EntityId and increment the counter
    EntityId const id = m_next_id;
    m_next_id = EntityId(m_next_id.id + 1);
    m_tuple_to_id.emplace(key, id);
    m_id_to_descriptor.emplace(id, EntityDescriptor{data_key, kind, time.getValue(), local_index});
    return id;
}

std::optional<EntityDescriptor> EntityRegistry::get(EntityId id) const {
    std::lock_guard<std::mutex> const lock(m_mutex);

    auto it = m_id_to_descriptor.find(id);
    if (it == m_id_to_descriptor.end()) {
        return std::nullopt;
    }
    return it->second;
}

void EntityRegistry::rebindKey(EntityId id, TimeFrameIndex const & time, int local_index) {
    assert(local_index >= 0);

    std::lock_guard<std::mutex> const lock(m_mutex);

    auto desc_it = m_id_to_descriptor.find(id);
    if (desc_it == m_id_to_descriptor.end()) {
        return;
    }

    EntityDescriptor const & old_desc = desc_it->second;
    EntityTupleKey const old_key{old_desc.data_key, old_desc.kind, old_desc.time_value, old_desc.local_index};
    m_tuple_to_id.erase(old_key);

    EntityTupleKey const new_key{old_desc.data_key, old_desc.kind, time.getValue(), local_index};
    m_tuple_to_id[new_key] = id;

    desc_it->second.time_value = time.getValue();
    desc_it->second.local_index = local_index;
}

void EntityRegistry::clear() {
    std::lock_guard<std::mutex> const lock(m_mutex);

    m_tuple_to_id.clear();
    m_id_to_descriptor.clear();
    m_next_id = EntityId(1);// Reset to 1, not 0, since 0 is sentinel value
}
