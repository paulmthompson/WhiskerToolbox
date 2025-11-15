#ifndef BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP
#define BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

struct EventWithId {
    TimeFrameIndex event_time;
    EntityId entity_id;

    EventWithId(TimeFrameIndex time, EntityId id)
        : event_time(time),
          entity_id(id) {}
};

#endif// BEHAVIORTOOLBOX_EVENT_WITH_ID_HPP