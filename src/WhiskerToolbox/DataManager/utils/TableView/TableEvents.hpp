#ifndef TABLE_EVENTS_HPP
#define TABLE_EVENTS_HPP

#include <optional>
#include <string>

/**
 * @brief Event types emitted for table lifecycle/updates.
 */
enum class TableEventType {
    Created,
    Removed,
    InfoUpdated,
    DataChanged,
};

/**
 * @brief Event payload for table notifications.
 */
struct TableEvent {
    TableEventType type;
    std::string tableId;
};

#endif // TABLE_EVENTS_HPP

