
#include "Analog_Time_Series_JSON.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "utils/json_helpers.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

using namespace WhiskerToolbox::Reflection;

AnalogDataType stringToAnalogDataType(std::string const & data_type_str) {
    if (data_type_str == "int16") return AnalogDataType::int16;
    if (data_type_str == "csv") return AnalogDataType::csv;
    return AnalogDataType::Unknown;
}

std::vector<std::shared_ptr<AnalogTimeSeries>> load_into_AnalogTimeSeries(std::string const & file_path,
                                                                          nlohmann::basic_json<> const & item) {

    std::vector<std::shared_ptr<AnalogTimeSeries>> analog_time_series;

    if (!requiredFieldsExist(
                item,
                {"format"},
                "Error: Missing required fields in AnalogTimeSeries")) {
        return analog_time_series;
    }
    std::string const data_type_str = item["format"];
    AnalogDataType const data_type = stringToAnalogDataType(data_type_str);

    switch (data_type) {
        case AnalogDataType::int16: {

            // Try reflected version with validation first
            auto opts_result = parseJson<BinaryAnalogLoaderOptions>(item);
            
            if (opts_result) {
                // Successfully parsed with reflect-cpp, use validated options
                auto opts = opts_result.value();
                opts.filename = file_path;
                analog_time_series = load(opts);
            } else {
                // Fall back to manual parsing for backward compatibility
                std::cerr << "Warning: BinaryAnalogLoader parsing failed. "
                          << "Validation error: " << opts_result.error()->what() << "\n";
            }

            break;
        }
        case AnalogDataType::csv: {
            
            // Try reflected version with validation first
            auto opts_result = parseJson<CSVAnalogLoaderOptions>(item);
            
            if (opts_result) {
                // Successfully parsed with reflect-cpp, use validated options
                auto opts = opts_result.value();
                opts.filepath = file_path;
                
                auto single_series = load(opts);
                if (single_series) {
                    analog_time_series.push_back(single_series);
                }
            } else {
                // Fall back to manual parsing for backward compatibility
                std::cerr << "Warning: CSVAnalogLoader parsing failed. "
                          << "Validation error: " << opts_result.error()->what() << "\n";
            }

            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return analog_time_series;
}
