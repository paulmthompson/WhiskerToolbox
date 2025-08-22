#ifndef ENTITYTYPES_HPP
#define ENTITYTYPES_HPP

#include <cstdint>
#include <functional>
#include <string>

/**
 * @brief Opaque identifier for a discrete entity (point, line, event, interval) for the current session.
 */
using EntityId = std::uint64_t;

/**
 * @brief Kinds of discrete entities that can be identified by an EntityId.
 */
enum class EntityKind : std::uint8_t {
    Point = 0,
    Line = 1,
    Event = 2,
    IntervalType = 3
};

/**
 * @brief Descriptor for a discrete entity, sufficient to regenerate or reason about its origin.
 */
struct EntityDescriptor {
    std::string data_key;        ///< DataManager key for the data object
    EntityKind kind;             ///< Kind of entity
    std::int64_t time_value;     ///< Time index value of the entity (session index)
    int local_index;             ///< Stable index within the time (0-based)
};

// Internal key used by EntityRegistry maps
struct EntityTupleKey {
    std::string data_key;
    EntityKind kind;
    std::int64_t time_value; // store as primitive for hashing
    int local_index;

    bool operator==(EntityTupleKey const & other) const {
        return kind == other.kind &&
               local_index == other.local_index &&
               time_value == other.time_value &&
               data_key == other.data_key;
    }
};

struct EntityTupleKeyHash {
    std::size_t operator()(EntityTupleKey const & k) const noexcept {
        std::size_t h1 = std::hash<std::string>{}(k.data_key);
        std::size_t h2 = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(k.kind));
        std::size_t h3 = std::hash<std::int64_t>{}(k.time_value);
        std::size_t h4 = std::hash<int>{}(k.local_index);
        // Combine hashes
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h4 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

#endif // ENTITYTYPES_HPP


