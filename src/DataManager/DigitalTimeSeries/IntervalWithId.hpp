#ifndef BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP
#define BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

/**
 * @brief Structure to hold an interval with its associated EntityID
 */
struct IntervalWithId {
    Interval interval;
    EntityId entity_id;

    IntervalWithId(Interval const & interval_data, EntityId id)
        : interval(interval_data),
          entity_id(id) {}
};

#endif// BEHAVIORTOOLBOX_INTERVAL_WITH_ID_HPP