#ifndef NEURALYZER_ENTITYID_HPP
#define NEURALYZER_ENTITYID_HPP

#include <cstdint>

/**
 * @brief Opaque identifier for a discrete entity (point, line, event, interval) for the current session.
 */
struct EntityId {
    EntityId() = default;

    explicit EntityId(uint64_t value)
        : id{value} {}

    uint64_t id;

    bool operator==(EntityId const & other) const {
        return id == other.id;
    }

    bool operator!=(EntityId const & other) const {
        return id != other.id;
    }

    bool operator<(EntityId const & other) const {
        return id < other.id;
    }

    bool operator>(EntityId const & other) const {
        return id > other.id;
    }

    bool operator<=(EntityId const & other) const {
        return id <= other.id;
    }
    bool operator>=(EntityId const & other) const {
        return id >= other.id;
    }
};

#endif // NEURALYZER_ENTITYID_HPP