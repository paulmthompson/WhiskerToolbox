#ifndef ROW_DESCRIPTOR_H
#define ROW_DESCRIPTOR_H

#include "TimeFrame.hpp"
#include "DigitalTimeSeries/interval_data.hpp"

#include <variant>
#include <cstddef>

/**
 * @brief A variant type that can hold any of the possible source types that can define a row.
 * 
 * This type allows for type-safe reverse lookup from a TableView row back to its 
 * original source definition. It makes the system easily extensible for new row types.
 */
using RowDescriptor = std::variant<
    std::monostate,         // For cases where there's no descriptor
    size_t,                 // For IndexSelector
    double,                 // For TimestampSelector
    TimeFrameInterval       // For IntervalSelector
>;

#endif // ROW_DESCRIPTOR_H
