#ifndef ROW_DESCRIPTOR_H
#define ROW_DESCRIPTOR_H

#include "TimeFrame.hpp"
#include "DigitalTimeSeries/interval_data.hpp"
#include "utils/TableView/core/DataSourceNameInterner.hpp"
#include "Entity/EntityTypes.hpp"

#include <cstddef>
#include <optional>
#include <variant>


/**
 * @brief A variant type that can hold any of the possible source types that can define a row.
 * 
 * This type allows for type-safe reverse lookup from a TableView row back to its 
 * original source definition. It makes the system easily extensible for new row types.
 */
using RowDescriptor = std::variant<
    std::monostate,         // For cases where there's no descriptor
    size_t,                 // For IndexSelector
    TimeFrameIndex,         // For TimestampSelector
    TimeFrameInterval       // For IntervalSelector
>;

/**
 * @brief Lightweight row identity for expanded rows (e.g., per-line in a timestamp).
 */
struct RowId {
    TimeFrameIndex timeIndex{0};
    std::optional<int> entityIndex{}; // Per-timestamp local index (e.g., line index)
};

/**
 * @brief Extended row descriptor carrying compact source identity and optional entity index.
 */
struct ExtendedRowDescriptor {
    DataSourceId sourceId{};
    RowId row{};
    // Optional contributing entities for this row (singleton for entity-expanded rows)
    std::vector<EntityId> contributingEntities{};
};

#endif // ROW_DESCRIPTOR_H
