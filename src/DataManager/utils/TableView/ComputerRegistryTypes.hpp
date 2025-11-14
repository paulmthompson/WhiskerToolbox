#ifndef COMPUTERREGISTRYTYPES_HPP
#define COMPUTERREGISTRYTYPES_HPP

#include <cstdint>
#include <memory>
#include <variant>

class DigitalEventSeries;
class IAnalogSource;
class IIntervalSource;
class LineData;

/**
 * @brief Enumeration of supported row selector types for matching computers.
 */
enum class RowSelectorType : std::uint8_t {
    IntervalBased,   ///< IntervalSelector - works with time intervals
    Timestamp,  ///< TimestampSelector - works with specific timestamps
    Index       ///< IndexSelector - works with discrete indices
};

/**
 * @brief Variant type for data sources that can be used with computers.
 * 
 * This includes both direct data manager sources and existing table columns
 * that implement the required interfaces.
 */
using DataSourceVariant = std::variant<
    std::shared_ptr<IAnalogSource>,
    std::shared_ptr<DigitalEventSeries>,
    std::shared_ptr<IIntervalSource>,
    std::shared_ptr<LineData>
>;

#endif // COMPUTERREGISTRYTYPES_HPP