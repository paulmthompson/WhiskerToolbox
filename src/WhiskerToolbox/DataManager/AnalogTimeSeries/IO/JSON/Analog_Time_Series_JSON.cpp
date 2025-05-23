
#include "Analog_Time_Series_JSON.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "utils/json_helpers.hpp"

#include <iostream>

AnalogDataType stringToAnalogDataType(std::string const & data_type_str) {
    if (data_type_str == "int16") return AnalogDataType::int16;
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

            auto opts = BinaryAnalogLoaderOptions();
            opts.filename = file_path;
            opts.header_size = item.value("header_size", 0);
            opts.num_channels = item.value("channel_count", 1);
            analog_time_series = load(opts);

            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return analog_time_series;
}
