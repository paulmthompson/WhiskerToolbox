#ifndef DIGITAL_EVENT_SERIES_LOADER_HPP
#define DIGITAL_EVENT_SERIES_LOADER_HPP

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "loaders/CSV_Loaders.hpp"
#include "loaders/binary_loaders.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

enum class EventDataType {
    uint16,
    csv,
    Unknown
};

EventDataType stringToEventDataType(std::string const & data_type_str) {
    if (data_type_str == "uint16") return EventDataType::uint16;
    if (data_type_str == "csv") return EventDataType::csv;
    return EventDataType::Unknown;
}


inline std::shared_ptr<DigitalEventSeries> load_into_DigitalEventSeries(std::string const & file_path, nlohmann::basic_json<> const & item) {
    auto digital_event_series = std::make_shared<DigitalEventSeries>();

    std::string const data_type_str = item["format"];
    EventDataType const data_type = stringToEventDataType(data_type_str);

    switch (data_type) {
        case EventDataType::uint16: {

            int const channel = item["channel"];
            std::string const transition = item["transition"];

            int const header_size = item.value("header_size", 0);
            int const num_channels = item.value("channel_count", 1);

            auto opts = Loader::BinaryAnalogOptions{
                    .file_path = file_path,
                    .header_size_bytes = static_cast<size_t>(header_size),
                    .num_channels = static_cast<size_t>(num_channels)};
            auto data = readBinaryFile<uint16_t>(opts);

            auto digital_data = Loader::extractDigitalData(data, channel);
            auto events = Loader::extractEvents(digital_data, transition);
            std::cout << "Loaded " << events.size() << " events " << std::endl;

            digital_event_series->setData(events);
            break;
        }
        case EventDataType::csv: {

            auto opts = Loader::CSVSingleColumnOptions{.filename = file_path};

            auto events = Loader::loadSingleColumnCSV(opts);
            std::cout << "Loaded " << events.size() << " events " << std::endl;

            float const scale = item.value("scale", 1.0f);
            bool const scale_divide = item.value("scale_divide", false);

            if (scale_divide) {
                for (auto & e: events) {
                    e /= scale;
                }
            } else {
                for (auto & e: events) {
                    e *= scale;
                }
            }

            digital_event_series->setData(events);
            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return digital_event_series;
}


#endif// DIGITAL_EVENT_SERIES_LOADER_HPP
