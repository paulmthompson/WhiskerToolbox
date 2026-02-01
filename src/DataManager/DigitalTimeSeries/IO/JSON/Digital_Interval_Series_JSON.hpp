#ifndef DIGITAL_INTERVAL_SERIES_JSON_HPP
#define DIGITAL_INTERVAL_SERIES_JSON_HPP

#include "IO/core/IOFormats.hpp"
#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class DigitalIntervalSeries;

/**
 * @deprecated Use IOFormat instead. This enum is kept for backward compatibility.
 */
enum class IntervalDataType {
    uint16,
    csv,
    multi_column_binary,
    Unknown
};

/**
 * @deprecated Use parseFormat() from IOFormats.hpp instead
 */
IntervalDataType stringToIntervalDataType(std::string const & data_type_str);

/**
 * @brief Load DigitalIntervalSeries from JSON configuration (DEPRECATED)
 * 
 * @deprecated Use the DigitalIntervalLoader plugin through LoaderRegistry instead.
 *             This function is kept for backward compatibility.
 * 
 * @param file_path Path to the data file
 * @param item JSON configuration
 * @return Loaded DigitalIntervalSeries
 */
[[deprecated("Use DigitalIntervalLoader plugin through LoaderRegistry instead")]]
std::shared_ptr<DigitalIntervalSeries> load_into_DigitalIntervalSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item);

#endif// DIGITAL_INTERVAL_SERIES_JSON_HPP
