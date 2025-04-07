
#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/Tensor_Data.hpp"

#include "AnalogTimeSeries/Analog_Time_Series_Loader.hpp"
#include "Masks/Mask_Data_Loader.hpp"
#include "Media/Video_Data_Loader.hpp"
#include "Points/Point_Data_Loader.hpp"

#include "loaders/CSV_Loaders.hpp"
#include "loaders/binary_loaders.hpp"
#include "transforms/data_transforms.hpp"

#include "TimeFrame.hpp"

#include "utils/container.hpp"
#include "utils/glob.hpp"
#include "utils/hdf5_mask_load.hpp"

#include "nlohmann/json.hpp"
#include "utils/string_manip.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>

using namespace nlohmann;

DataManager::DataManager() {
    _times["time"] = std::make_shared<TimeFrame>();
    _data["media"] = std::make_shared<MediaData>();

    setTimeFrame("media", "time");
    _output_path = std::filesystem::current_path();
}

void DataManager::setTimeFrame(std::string const & data_key, std::string const & time_key) {
    //Check that data_key is in _data
    if (_data.find(data_key) == _data.end()) {
        std::cerr << "Data key not found in DataManager: " << data_key << std::endl;
        return;
    }

    //Check that time_key is in _times
    if (_times.find(time_key) == _times.end()) {
        std::cerr << "Time key not found in DataManager: " << time_key << std::endl;
        return;
    }

    _time_frames[data_key] = time_key;
}

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key) {
    auto myvector = load_ragged_array<float>(filepath, key);
    return myvector;
}

std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key) {
    auto myvector = load_array<int>(filepath, key);
    return myvector;
}

enum class DataType {
    Video,
    Points,
    Mask,
    Line,
    Analog,
    DigitalEvent,
    DigitalInterval,
    Tensor,
    Time,
    Unknown
};

DataType stringToDataType(std::string const & data_type_str) {
    if (data_type_str == "video") return DataType::Video;
    if (data_type_str == "points") return DataType::Points;
    if (data_type_str == "mask") return DataType::Mask;
    if (data_type_str == "line") return DataType::Line;
    if (data_type_str == "analog") return DataType::Analog;
    if (data_type_str == "digital_event") return DataType::DigitalEvent;
    if (data_type_str == "digital_interval") return DataType::DigitalInterval;
    if (data_type_str == "tensor") return DataType::Tensor;
    if (data_type_str == "time") return DataType::Time;
    return DataType::Unknown;
}

std::optional<std::string> processFilePath(
        std::string const & file_path,
        std::filesystem::path const & base_path) {
    std::filesystem::path full_path = file_path;

    // Check for wildcard character
    if (file_path.find('*') != std::string::npos) {
        // Convert wildcard pattern to regex
        std::string const pattern = std::regex_replace(full_path.string(), std::regex("\\*"), ".*");
        std::regex const regex_pattern(pattern);

        // Iterate through the directory to find matching files
        for (auto const & entry: std::filesystem::directory_iterator(base_path)) {
            std::cout << "Checking " << entry.path().string() << " with full path " << full_path << std::endl;
            if (std::regex_match(entry.path().string(), regex_pattern)) {
                std::cout << "Loading file " << entry.path().string() << std::endl;
                return entry.path().string();
            }
        }
        return std::nullopt;
    } else {
        // Check if the file path is relative
        if (!std::filesystem::path(file_path).is_absolute()) {
            full_path = base_path / file_path;
        }
        // Check for the presence of the file
        if (std::filesystem::exists(full_path)) {
            std::cout << "Loading file " << full_path.string() << std::endl;
            return full_path.string();
        } else {
            return std::nullopt;
        }
    }
}

bool checkRequiredFields(json const & item, std::vector<std::string> const & requiredFields) {
    for (auto const & field: requiredFields) {
        if (!item.contains(field)) {
            std::cerr << "Error: Missing required field \"" << field << "\" in JSON item." << std::endl;
            return false;
        }
    }
    return true;
}

int DataManager::addCallbackToData(std::string const & key, ObserverCallback callback) {

    int id = -1;

    if (_data.find(key) != _data.end()) {
        auto data = _data[key];

        id = std::visit([callback](auto & x) {
            return x.get()->addObserver(callback);
        },
                        data);
    }

    return id;
}

void DataManager::removeCallbackFromData(std::string const & key, int callback_id) {
    if (_data.find(key) != _data.end()) {
        auto data = _data[key];

        std::visit([callback_id](auto & x) {
            x.get()->removeObserver(callback_id);
        },
                   data);
    }
}

void checkOptionalFields(json const & item, std::vector<std::string> const & optionalFields) {
    for (auto const & field: optionalFields) {
        if (!item.contains(field)) {
            std::cout << "Warning: Optional field \"" << field << "\" is missing in JSON item." << std::endl;
        }
    }
}

std::vector<DataInfo> load_data_from_json_config(DataManager * dm, std::string const & json_filepath) {
    std::vector<DataInfo> data_info_list;
    // Open JSON file
    std::ifstream ifs(json_filepath);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open JSON file: " << json_filepath << std::endl;
        return data_info_list;
    }

    // Parse JSON
    json j;
    ifs >> j;

    // get base path of filepath
    std::filesystem::path const base_path = std::filesystem::path(json_filepath).parent_path();

    // Iterate through JSON array
    for (auto const & item: j) {

        if (!checkRequiredFields(item, {"data_type", "name", "filepath"})) {
            continue;// Exit if any required field is missing
        }

        std::string const data_type_str = item["data_type"];
        DataType const data_type = stringToDataType(data_type_str);
        if (data_type == DataType::Unknown) {
            std::cout << "Unknown data type: " << data_type_str << std::endl;
            continue;
        }

        std::string const name = item["name"];

        auto file_exists = processFilePath(item["filepath"], base_path);
        if (!file_exists) {
            std::cerr << "File does not exist: " << item["filepath"] << std::endl;
            continue;
        }

        std::string const file_path = file_exists.value();

        switch (data_type) {
            case DataType::Video: {
                // Create VideoData object
                auto video_data = load_video_into_VideoData(file_path);

                // Add VideoData to DataManager
                dm->setMedia(video_data);

                data_info_list.push_back({name, "VideoData", ""});
                break;
            }
            case DataType::Points: {

                auto point_data = load_into_PointData(file_path, item);

                dm->setData<PointData>(name, point_data);

                std::string const color = item.value("color", "#0000FF");
                data_info_list.push_back({name, "PointData", color});
                break;
            }
            case DataType::Mask: {

                auto mask_data = load_into_MaskData(file_path, item);

                std::string const color = item.value("color", "0000FF");
                dm->setData<MaskData>(name, mask_data);

                data_info_list.push_back({name, "MaskData", color});

                if (item.contains("operations")) {

                    for (auto const & operation: item["operations"]) {

                        std::string const operation_type = operation["type"];

                        if (operation_type == "area") {
                            std::cout << "Calculating area for mask: " << name << std::endl;
                            auto area_data = area(dm->getData<MaskData>(name));
                            std::string const output_name = name + "_area";
                            dm->setData<AnalogTimeSeries>(output_name, area_data);
                        }
                    }
                }
                break;
            }
            case DataType::Line: {

                auto line_map = load_line_csv(file_path);

                //Get the whisker name from the filename using filesystem
                auto whisker_filename = std::filesystem::path(file_path).filename().string();

                //Remove .csv suffix from filename
                auto whisker_name = remove_extension(whisker_filename);

                dm->setData<LineData>(whisker_name, std::make_shared<LineData>(line_map));

                std::string const color = item.value("color", "0000FF");

                data_info_list.push_back({name, "LineData", color});

                break;
            }
            case DataType::Analog: {

                auto analog_time_series = load_into_AnalogTimeSeries(file_path, item);

                for (int channel = 0; channel < analog_time_series.size(); channel++) {
                    std::string const channel_name = name + "_" + std::to_string(channel);

                    dm->setData<AnalogTimeSeries>(channel_name, analog_time_series[channel]);

                    if (item.contains("clock")) {
                        std::string const clock = item["clock"];
                        dm->setTimeFrame(channel_name, clock);
                    }
                }
                break;
            }
            case DataType::DigitalEvent: {

                if (item["format"] == "uint16") {

                    int const channel = item["channel"];
                    std::string const transition = item["transition"];

                    auto opts = createBinaryAnalogOptions(file_path, item);
                    auto data = readBinaryFile<uint16_t>(opts);

                    auto digital_data = Loader::extractDigitalData(data, channel);
                    auto events = Loader::extractEvents(digital_data, transition);
                    std::cout << "Loaded " << events.size() << " events for " << name << std::endl;

                    auto digital_event_series = std::make_shared<DigitalEventSeries>();
                    digital_event_series->setData(events);
                    dm->setData<DigitalEventSeries>(name, digital_event_series);
                } else if (item["format"] == "csv") {

                    auto opts = Loader::CSVSingleColumnOptions{.filename = file_path};

                    auto events = Loader::loadSingleColumnCSV(opts);
                    std::cout << "Loaded " << events.size() << " events for " << name << std::endl;

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

                    auto digital_event_series = std::make_shared<DigitalEventSeries>();
                    digital_event_series->setData(events);
                    dm->setData<DigitalEventSeries>(name, digital_event_series);

                } else {
                    std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
                }
                break;
            }
            case DataType::DigitalInterval: {

                if (item["format"] == "uint16") {

                    int const channel = item["channel"];
                    std::string const transition = item["transition"];

                    auto opts = createBinaryAnalogOptions(file_path, item);
                    auto data = readBinaryFile<uint16_t>(opts);

                    auto digital_data = Loader::extractDigitalData(data, channel);

                    auto intervals = Loader::extractIntervals(digital_data, transition);
                    std::cout << "Loaded " << intervals.size() << " intervals for " << name << std::endl;

                    auto digital_interval_series = std::make_shared<DigitalIntervalSeries>();
                    digital_interval_series->setData(intervals);
                    dm->setData<DigitalIntervalSeries>(name, digital_interval_series);

                } else if (item["format"] == "csv") {

                    auto opts = Loader::CSVMultiColumnOptions{.filename = file_path};

                    auto intervals = Loader::loadPairColumnCSV(opts);
                    std::cout << "Loaded " << intervals.size() << " intervals for " << name << std::endl;
                    auto digital_interval_series = std::make_shared<DigitalIntervalSeries>();
                    digital_interval_series->setData(intervals);
                    dm->setData<DigitalIntervalSeries>(name, digital_interval_series);
                } else {
                    std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
                }
                break;
            }
            case DataType::Tensor: {

                if (item["format"] == "numpy") {

                    TensorData tensor_data;
                    loadNpyToTensorData(file_path, tensor_data);

                    dm->setData<TensorData>(name, std::make_shared<TensorData>(tensor_data));

                } else {
                    std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
                }
                break;
            }
            case DataType::Time: {

                if (item["format"] == "uint16") {

                    int const channel = item["channel"];
                    std::string const transition = item["transition"];

                    auto opts = createBinaryAnalogOptions(file_path, item);
                    auto data = readBinaryFile<uint16_t>(opts);

                    auto digital_data = Loader::extractDigitalData(data, channel);
                    auto events = Loader::extractEvents(digital_data, transition);

                    // convert to int with std::transform
                    std::vector<int> events_int;
                    events_int.reserve(events.size());
                    for (auto e: events) {
                        events_int.push_back(static_cast<int>(e));
                    }
                    std::cout << "Loaded " << events_int.size() << " events for " << name << std::endl;

                    auto timeframe = std::make_shared<TimeFrame>(events_int);
                    dm->setTime(name, timeframe);
                }

                if (item["format"] == "uint16_length") {

                    auto opts = createBinaryAnalogOptions(file_path, item);
                    auto data = readBinaryFile<uint16_t>(opts);

                    std::vector<int> t(data.size());
                    std::iota(std::begin(t), std::end(t), 0);

                    std::cout << "Total of " << t.size() << " timestamps for " << name << std::endl;

                    auto timeframe = std::make_shared<TimeFrame>(t);
                    dm->setTime(name, timeframe);
                }
                break;
            }
            default:
                std::cout << "Unsupported data type: " << data_type_str << std::endl;
                continue;
        }
        if (item.contains("clock")) {
            std::string const clock = item["clock"];
            std::cout << "Setting time for " << name << " to " << clock << std::endl;
            dm->setTimeFrame(name, clock);
        }
    }

    return data_info_list;
}

std::string DataManager::getType(std::string const & key) const {
    auto it = _data.find(key);
    if (it != _data.end()) {
        if (std::holds_alternative<std::shared_ptr<MediaData>>(it->second)) {
            return "MediaData";
        } else if (std::holds_alternative<std::shared_ptr<PointData>>(it->second)) {
            return "PointData";
        } else if (std::holds_alternative<std::shared_ptr<LineData>>(it->second)) {
            return "LineData";
        } else if (std::holds_alternative<std::shared_ptr<MaskData>>(it->second)) {
            return "MaskData";
        } else if (std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(it->second)) {
            return "AnalogTimeSeries";
        } else if (std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(it->second)) {
            return "DigitalEventSeries";
        } else if (std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(it->second)) {
            return "DigitalIntervalSeries";
        } else if (std::holds_alternative<std::shared_ptr<TensorData>>(it->second)) {
            return "TensorData";
        }
        return "Unknown";
    }
}
