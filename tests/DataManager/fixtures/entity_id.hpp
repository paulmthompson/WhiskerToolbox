#ifndef ENTITY_ID_TEST_FIXTURE_HPP
#define ENTITY_ID_TEST_FIXTURE_HPP

#include "Entity/EntityTypes.hpp"

#include <vector>

template<typename DataType>
std::vector<EntityId> get_all_entity_ids(DataType const & data) {
    std::vector<EntityId> entity_ids;
    // Use elements() which provides a flat view of (time, DataEntry) pairs
    for (auto const & [time, entry]: data.elements()) {
        static_cast<void>(time);
        entity_ids.push_back(entry.entity_id);
    }
    return entity_ids;
}

#endif// ENTITY_ID_TEST_FIXTURE_HPP