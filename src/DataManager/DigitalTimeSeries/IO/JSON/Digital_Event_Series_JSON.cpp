#include "Digital_Event_Series_JSON.hpp"

#include "utils/json_helpers.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"

#include "IO/formats/Binary/common/binary_loaders.hpp"

EventDataType stringToEventDataType(std::string const & data_type_str) {
    if (data_type_str == "uint16") return EventDataType::uint16;
    if (data_type_str == "csv") return EventDataType::csv;
    return EventDataType::Unknown;
}

void scale_events(std::vector<TimeFrameIndex> & events, float const scale, bool const scale_divide)
{
    if (scale_divide) {
        for (auto & e: events) {
            e = TimeFrameIndex(static_cast<int64_t>(e.getValue() / scale));
        }
    } else {
        for (auto & e: events) {
            e = TimeFrameIndex(static_cast<int64_t>(e.getValue() * scale));
        }
    }
}

std::vector<std::shared_ptr<DigitalEventSeries>> load_into_DigitalEventSeries(std::string const & file_path, nlohmann::basic_json<> const & item) {
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

            digital_event_series.push_back(std::make_shared<DigitalEventSeries>(events));
            break;
        }
        case EventDataType::csv: {

            // Create CSV options from JSON configuration
            CSVEventLoaderOptions opts;
            opts.filepath = file_path;
            opts.delimiter = item.value("delimiter", ",");
            opts.has_header = item.value("has_header", false);
            opts.event_column = item.value("event_column", 0);
            
            // Check for multi-column configuration
            if (item.contains("identifier_column") || item.contains("label_column")) {
                // Multi-column case: use identifier_column (preferred) or label_column (legacy)
                opts.identifier_column = item.value("identifier_column", item.value("label_column", 1));
            } else {
                // Single column case
                opts.identifier_column = -1;
            }
            
            // Set base name for series naming
            opts.base_name = item.value("name", "events");
            
            // Load using new CSV system
            auto loaded_series = load(opts);
            
            // Apply scaling if specified
            float const scale = item.value("scale", 1.0f);
            bool const scale_divide = item.value("scale_divide", false);
            
            if (scale != 1.0f) {
                for (auto & series : loaded_series) {
                    auto events_view = series->view();
                    auto events = std::vector<TimeFrameIndex>();
                    for (auto e : events_view) {
                        events.push_back(e.time());
                    }
                    std::vector<TimeFrameIndex> scaled_events = events;  // Make a copy
                    scale_events(scaled_events, scale, scale_divide);
                    series = std::make_shared<DigitalEventSeries>(scaled_events);
                }
            }
            
            // Add to result
            digital_event_series.insert(digital_event_series.end(), loaded_series.begin(), loaded_series.end());
            
            std::cout << "Loaded " << loaded_series.size() << " digital event series from CSV" << std::endl;
            
            break;
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return digital_event_series;
}
