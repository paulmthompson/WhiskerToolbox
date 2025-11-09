#ifndef ENTITY_ID_TEST_FIXTURE_HPP
#define ENTITY_ID_TEST_FIXTURE_HPP

#include "Entity/EntityTypes.hpp"

#include <vector>

template<typename DataType>
std::vector<EntityId> get_all_entity_ids(DataType const & data) {
    std::vector<EntityId> entity_ids;
    for (auto const & [time, entries]: data.getAllEntries()) {
        for (auto const & entry: entries) {
            entity_ids.push_back(entry.entity_id);
        }
    }
    return entity_ids;
}

#endif// ENTITY_ID_TEST_FIXTURE_HPP