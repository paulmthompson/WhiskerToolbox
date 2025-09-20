#ifndef BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP
#define BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"

struct EventWithId {
    float event_time;
    EntityId entity_id;

    EventWithId(float time, EntityId id)
        : event_time(time),
          entity_id(id) {}
};

#endif// BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP