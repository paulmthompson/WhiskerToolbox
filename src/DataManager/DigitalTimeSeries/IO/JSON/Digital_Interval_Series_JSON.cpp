
#include "Digital_Interval_Series_JSON.hpp"

#include "loaders/CSV_Loaders.hpp"
#include "loaders/binary_loaders.hpp"
#include "utils/json_helpers.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"



IntervalDataType stringToIntervalDataType(std::string const & data_type_str) {
    if (data_type_str == "uint16") return IntervalDataType::uint16;
    if (data_type_str == "csv") return IntervalDataType::csv;
    return IntervalDataType::Unknown;
}


std::shared_ptr<DigitalIntervalSeries> load_into_DigitalIntervalSeries(std::string const & file_path,
                                                                       nlohmann::basic_json<> const & item) {
    auto digital_interval_series = std::make_shared<DigitalIntervalSeries>();

    if (!requiredFieldsExist(
                item,
                {"format"},
                "Error: Missing required fields in DigitalIntervalSeries")) {
        return digital_interval_series;
    }
    std::string const data_type_str = item["format"];
    IntervalDataType const data_type = stringToIntervalDataType(data_type_str);

    switch (data_type) {
        case IntervalDataType::uint16: {

            if (!requiredFieldsExist(
                        item,
                        {"channel", "transition"},
                        "Error: Missing required fields in uint16 DigitalIntervalSeries")) {
                return digital_interval_series;
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

            auto intervals = Loader::extractIntervals(digital_data, transition);
            std::cout << "Loaded " << intervals.size() << " intervals " << std::endl;

            digital_interval_series = std::make_shared<DigitalIntervalSeries>(intervals);

            return digital_interval_series;
        }
        case IntervalDataType::csv: {

            auto opts = Loader::CSVPairColumnOptions{.filename = file_path};
            
            // Configure CSV loading options from JSON
            if (item.contains("skip_header")) {
                opts.skip_header = item["skip_header"];
            } else {
                // Default to skipping header if not specified
                opts.skip_header = true;
            }
            
            if (item.contains("delimiter")) {
                opts.col_delimiter = item["delimiter"];
            }
            
            if (item.contains("flip_column_order")) {
                opts.flip_column_order = item["flip_column_order"];
            }

            auto intervals = Loader::loadPairColumnCSV(opts);
            std::cout << "Loaded " << intervals.size() << " intervals " << std::endl;
            return std::make_shared<DigitalIntervalSeries>(intervals);
        }
        default: {
            std::cout << "Format " << data_type_str << " not found " << std::endl;
        }
    }

    return std::make_shared<DigitalIntervalSeries>();
}
