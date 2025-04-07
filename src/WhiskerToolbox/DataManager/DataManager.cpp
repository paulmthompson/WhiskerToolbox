
#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/Tensor_Data.hpp"

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


Loader::BinaryAnalogOptions createBinaryAnalogOptions(std::string const & file_path, nlohmann::basic_json<> const & item) {

    int const header_size = item.value("header_size", 0);
    int const num_channels = item.value("channel_count", 1);

    auto opts = Loader::BinaryAnalogOptions{
            .file_path = file_path,
            .header_size_bytes = static_cast<size_t>(header_size),
            .num_channels = static_cast<size_t>(num_channels)};

    return opts;
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

        std::string const data_type = item["data_type"];
        std::string const name = item["name"];

        auto file_exists = processFilePath(item["filepath"], base_path);
        if (!file_exists) {
            std::cerr << "File does not exist: " << item["filepath"] << std::endl;
            continue;
        }

        std::string const file_path = file_exists.value();

        if (data_type == "video") {
            // Create VideoData object
            auto video_data = std::make_shared<VideoData>();

            video_data->LoadMedia(file_path);

            std::cout << "Video has " << video_data->getTotalFrameCount() << " frames" << std::endl;
            // Add VideoData to DataManager
            dm->setMedia(video_data);

            data_info_list.push_back({name, "VideoData", ""});

        } else if (data_type == "points") {

            int const frame_column = item["frame_column"];
            int const x_column = item["x_column"];
            int const y_column = item["y_column"];

            std::string const color = item.value("color", "#0000FF");
            std::string const delim = item.value("delim", " ");

            int const height = item.value("height", -1);
            int const width = item.value("width", -1);

            int const scaled_height = item.value("scale_to_height", -1);
            int const scaled_width = item.value("scale_to_width", -1);

            auto keypoints = load_points_from_csv(
                    file_path,
                    frame_column,
                    x_column,
                    y_column,
                    delim.c_str()[0]);

            std::cout << "There are " << keypoints.size() << " keypoints " << std::endl;

            auto point_data = std::make_shared<PointData>(keypoints);
            point_data->setImageSize(ImageSize{width, height});

            if (scaled_height > 0 && scaled_width > 0) {
                scale(point_data, ImageSize{scaled_width, scaled_height});
            }

            dm->setData<PointData>(name, point_data);

            data_info_list.push_back({name, "PointData", color});

        } else if (data_type == "mask") {

            std::string const frame_key = item["frame_key"];
            std::string const prob_key = item["probability_key"];
            std::string const x_key = item["x_key"];
            std::string const y_key = item["y_key"];

            int const height = item.value("height", -1);
            int const width = item.value("width", -1);

            std::string const color = item.value("color", "0000FF");

            auto frames = read_array_hdf5(file_path, frame_key);
            auto probs = read_ragged_hdf5(file_path, prob_key);
            auto y_coords = read_ragged_hdf5(file_path, y_key);
            auto x_coords = read_ragged_hdf5(file_path, x_key);

            auto mask_data = std::make_shared<MaskData>();
            mask_data->setImageSize(ImageSize{width, height});

            for (std::size_t i = 0; i < frames.size(); i++) {
                auto frame = frames[i];
                auto prob = probs[i];
                auto x = x_coords[i];
                auto y = y_coords[i];
                mask_data->addMaskAtTime(frame, x, y);
            }

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
        } else if (data_type == "line") {

            auto line_map = load_line_csv(file_path);

            //Get the whisker name from the filename using filesystem
            auto whisker_filename = std::filesystem::path(file_path).filename().string();

            //Remove .csv suffix from filename
            auto whisker_name = remove_extension(whisker_filename);

            dm->setData<LineData>(whisker_name, std::make_shared<LineData>(line_map));

            std::string const color = item.value("color", "0000FF");

            data_info_list.push_back({name, "LineData", color});

        } else if (data_type == "analog") {

            if (item["format"] == "int16") {

                auto opts = createBinaryAnalogOptions(file_path, item);

                if (opts.num_channels > 1) {

                    auto data = readBinaryFileMultiChannel<int16_t>(opts);

                    std::cout << "Read " << data.size() << " channels" << std::endl;

                    for (int channel = 0; channel < data.size(); channel++) {
                        // convert to float with std::transform
                        std::vector<float> data_float;
                        std::transform(
                                data[channel].begin(),
                                data[channel].end(),
                                std::back_inserter(data_float), [](int16_t i) { return i; });

                        auto analog_time_series = std::make_shared<AnalogTimeSeries>();
                        analog_time_series->setData(data_float);

                        std::string const channel_name = name + "_" + std::to_string(channel);

                        dm->setData<AnalogTimeSeries>(channel_name, analog_time_series);

                        if (item.contains("clock")) {
                            std::string const clock = item["clock"];
                            dm->setTimeFrame(channel_name, clock);
                        }
                    }

                } else {

                    auto data = readBinaryFile<int16_t>(opts);

                    // convert to float with std::transform
                    std::vector<float> data_float;
                    std::transform(
                            data.begin(),
                            data.end(),
                            std::back_inserter(data_float), [](int16_t i) { return i; });

                    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
                    analog_time_series->setData(data_float);
                    dm->setData<AnalogTimeSeries>(name, analog_time_series);
                }

            } else {
                std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
            }

        } else if (data_type == "digital_event") {

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
        } else if (data_type == "digital_interval") {

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
        } else if (data_type == "tensor") {

            if (item["format"] == "numpy") {

                TensorData tensor_data;
                loadNpyToTensorData(file_path, tensor_data);

                dm->setData<TensorData>(name, std::make_shared<TensorData>(tensor_data));

            } else {
                std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
            }

        } else if (data_type == "time") {

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
