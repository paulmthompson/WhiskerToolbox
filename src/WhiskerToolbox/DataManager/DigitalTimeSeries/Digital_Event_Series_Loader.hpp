#ifndef DIGITAL_EVENT_SERIES_LOADER_HPP
#define DIGITAL_EVENT_SERIES_LOADER_HPP

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "loaders/CSV_Loaders.hpp"
#include "loaders/binary_loaders.hpp"
#include "utils/json_helpers.hpp"

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

void scale_events(std::vector<float> & events, float const scale, bool const scale_divide)
{
    if (scale_divide) {
        for (auto & e: events) {
            e /= scale;
        }
    } else {
        for (auto & e: events) {
            e *= scale;
        }
    }
}

inline std::vector<std::shared_ptr<DigitalEventSeries>> load_into_DigitalEventSeries(std::string const & file_path, nlohmann::basic_json<> const & item) {
    auto digital_event_series = std::vector<std::shared_ptr<DigitalEventSeries>>();

    if (!requiredFieldsExist(
                item,
                {"format"},
                "Error: Missing required fields in DigitalEventSeries")) {
        return digital_event_series;
    }
    std::string const data_type_str = item["format"];
    EventDataType const data_type = stringToEventDataType(data_type_str);

    switch (data_type) {
        case EventDataType::uint16: {

            if (!requiredFieldsExist(
                        item,
                        {"channel", "transition"},
                        "Error: Missing required fields in uint16 DigitalEventSeries")) {
                return digital_event_series;
            }

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

            digital_event_series.push_back(std::make_shared<DigitalEventSeries>());
            digital_event_series.back()->setData(events);
            break;
        }
        case EventDataType::csv: {

            int const num_channels = item.value("channel_count", 1);
            float const scale = item.value("scale", 1.0f);
            bool const scale_divide = item.value("scale_divide", false);

            if (num_channels == 1) {

                auto opts = Loader::CSVSingleColumnOptions{.filename = file_path};

                auto events = Loader::loadSingleColumnCSV(opts);
                std::cout << "Loaded " << events.size() << " events " << std::endl;

                scale_events(events, scale, scale_divide);

                digital_event_series.push_back(std::make_shared<DigitalEventSeries>());
                digital_event_series.back()->setData(events);
            } else {

                int const event_column = item["event_column"];
                int const label_column = item["label_column"];

                auto opts = Loader::CSVMultiColumnOptions {.filename = file_path,
                                                          .key_column = static_cast<size_t>(label_column),
                                                          .value_column = static_cast<size_t>(event_column)};

                auto events = Loader::loadMultiColumnCSV(opts);

                for (auto & [event_id, event] : events){

                    scale_events(event, scale, scale_divide);

                    digital_event_series.push_back(std::make_shared<DigitalEventSeries>());
                    digital_event_series.back()->setData(event);
                }
            }
            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return digital_event_series;
}


#endif// DIGITAL_EVENT_SERIES_LOADER_HPP
