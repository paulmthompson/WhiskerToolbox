#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "loaders/binary_loaders.hpp"
#include "utils/json_helpers.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

enum class AnalogDataType {
    int16,
    Unknown
};

AnalogDataType stringToAnalogDataType(std::string const & data_type_str) {
    if (data_type_str == "int16") return AnalogDataType::int16;
    return AnalogDataType::Unknown;
}


inline std::vector<std::shared_ptr<AnalogTimeSeries>> load_into_AnalogTimeSeries(std::string const & file_path, nlohmann::basic_json<> const & item) {

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

            int const header_size = item.value("header_size", 0);
            int const num_channels = item.value("channel_count", 1);

            auto opts = Loader::BinaryAnalogOptions{
                    .file_path = file_path,
                    .header_size_bytes = static_cast<size_t>(header_size),
                    .num_channels = static_cast<size_t>(num_channels)};

            if (opts.num_channels > 1) {

                auto data = readBinaryFileMultiChannel<int16_t>(opts);

                std::cout << "Read " << data.size() << " channels" << std::endl;

                for (auto & channel: data) {
                    // convert to float with std::transform
                    std::vector<float> data_float;
                    std::transform(
                            channel.begin(),
                            channel.end(),
                            std::back_inserter(data_float), [](int16_t i) { return i; });

                    analog_time_series.push_back(std::make_shared<AnalogTimeSeries>());
                    analog_time_series.back()->setData(data_float);
                }

            } else {

                auto data = readBinaryFile<int16_t>(opts);

                // convert to float with std::transform
                std::vector<float> data_float;
                std::transform(
                        data.begin(),
                        data.end(),
                        std::back_inserter(data_float), [](int16_t i) { return i; });

                analog_time_series.push_back(std::make_shared<AnalogTimeSeries>());
                analog_time_series.back()->setData(data_float);
            }
            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return analog_time_series;
}


#endif// ANALOG_TIME_SERIES_LOADER_HPP
