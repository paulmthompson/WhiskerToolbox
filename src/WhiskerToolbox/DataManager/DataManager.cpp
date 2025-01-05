
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Media/Video_Data.hpp"
#include "transforms/data_transforms.hpp"
#include "loaders/binary_loaders.hpp"
#include "loaders/CSV_Loaders.hpp"

#include "TimeFrame.hpp"

#include "utils/hdf5_mask_load.hpp"
#include "utils/container.hpp"
#include "utils/glob.hpp"

#include "nlohmann/json.hpp"
#include "utils/string_manip.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>

using namespace nlohmann;

DataManager::DataManager()
{
    _times["time"] = std::make_shared<TimeFrame>();
    _data["media"] = std::make_shared<MediaData>();

    setTimeFrame("media", "time");
}

void DataManager::setTimeFrame(std::string data_key, std::string time_key)
{
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

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key)
{
    auto myvector = load_ragged_array<float>(filepath, key);
    return myvector;
}

std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key)
{
    auto myvector = load_array<int>(filepath, key);
    return myvector;
}

void DataManager::load_A(std::string const & filepath)
{
    std::cerr << "load_A called with " << filepath << std::endl;
}

void DataManager::load_B(std::string const & filepath)
{
    std::cerr << "load_B called with " << filepath << std::endl;
}

void DataManager::loadFromJSON(std::string const & filepath)
{
    // Set CWD to the directory of the JSON file to allow relative paths
    auto original_path = std::filesystem::current_path();
    std::filesystem::current_path(std::filesystem::path(filepath).parent_path());

    // Open JSON
    std::ifstream ifs(std::filesystem::path(filepath).filename());
    json j = basic_json<>::parse(ifs);

    // Configure the functions to call based on the JSON keys
    const std::map<std::string, void(DataManager::*)(std::string const &)> load_functions = {
        {"load_A", &DataManager::load_A},
        {"load_B", &DataManager::load_B}
    };

    /*
    for (json::iterator it = j.begin(); it != j.end(); ++it) {
        if (it.key() == "children"){
            // Special key "children"
            // Specifies additinal .json files to recurse into
            for (auto item : it.value()){
                assert(item.is_string());
                for (auto& p : glob::glob(item.get<std::string>())){
                    loadFromJSON(p);
                }
            }
        } else if (load_functions.count(it.key())){
            for (auto item : it.value()){
                assert(item.is_string());
                for (auto& p : glob::glob(item.get<std::string>())){
                    (this->*load_functions.at(it.key()))(p);
                }
            }
        } else {
            std::cout << "Unknown key found in JSON: " << it.key() << "\n";
        }
    }
    */
    std::filesystem::current_path(original_path);
}

std::optional<std::string> processFilePath(
        const std::string& file_path,
        const std::filesystem::path& base_path)
{
    std::filesystem::path full_path = file_path;

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

bool checkRequiredFields(const json& item, const std::vector<std::string>& requiredFields) {
    for (const auto& field : requiredFields) {
        if (!item.contains(field)) {
            std::cerr << "Error: Missing required field \"" << field << "\" in JSON item." << std::endl;
            return false;
        }
    }
    return true;
}

void checkOptionalFields(const json& item, const std::vector<std::string>& optionalFields) {
    for (const auto& field : optionalFields) {
        if (!item.contains(field)) {
            std::cout << "Warning: Optional field \"" << field << "\" is missing in JSON item." << std::endl;
        }
    }
}

std::vector<DataInfo> load_data_from_json_config(std::shared_ptr<DataManager> dm, std::string json_filepath)
{
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
    std::filesystem::path base_path = std::filesystem::path(json_filepath).parent_path();

    // Iterate through JSON array
    for (const auto& item : j) {

        if (!checkRequiredFields(item, {"data_type", "name", "filepath"})) {
            continue; // Exit if any required field is missing
        }

        std::string data_type = item["data_type"];
        std::string name = item["name"];

        auto file_exists = processFilePath(item["filepath"], base_path);
        if (!file_exists) {
            std::cerr << "File does not exist: " << item["filepath"] << std::endl;
            continue;
        }

        std::string file_path = file_exists.value();

        if (data_type == "video") {
            // Create VideoData object
            auto video_data = std::make_shared<VideoData>();

            video_data->LoadMedia(file_path);

            std::cout << "Video has " << video_data->getTotalFrameCount() << " frames" << std::endl;
            // Add VideoData to DataManager
            dm->setMedia(video_data);

            data_info_list.push_back({name, "VideoData", ""});
        } else if (data_type == "points") {

            int frame_column = item["frame_column"];
            int x_column = item["x_column"];
            int y_column = item["y_column"];

            std::string color = item.value("color","0000FF");
            std::string delim = item.value("delim", " ");

            auto keypoints = load_points_from_csv(
                    file_path,
                    frame_column,
                    x_column,
                    y_column,
                    delim.c_str()[0]);

            auto keypoint_key = name;

            std::cout << "There are " <<  keypoints.size() << " keypoints " << std::endl;

            auto point_data = std::make_shared<PointData>(keypoints);

            dm->setData<PointData>(keypoint_key, point_data);

            data_info_list.push_back({keypoint_key, "PointData", color});

        } else if (data_type == "mask") {

            std::string frame_key = item["frame_key"];
            std::string prob_key = item["probability_key"];
            std::string x_key = item["x_key"];
            std::string y_key = item["y_key"];

            std::string color = item.value("color","0000FF");

            auto frames =  read_array_hdf5(file_path, frame_key);
            auto probs = read_ragged_hdf5(file_path, prob_key);
            auto y_coords = read_ragged_hdf5(file_path, y_key);
            auto x_coords = read_ragged_hdf5(file_path, x_key);

            auto mask_data = std::make_shared<MaskData>();

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

                for (const auto& operation : item["operations"]) {

                    std::string operation_type = operation["type"];

                    if (operation_type == "area") {
                        std::cout << "Calculating area for mask: " << name << std::endl;
                        auto area_data = area(dm->getData<MaskData>(name));
                        std::string output_name = name + "_area";
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

            std::string color = item.value("color","0000FF");

            data_info_list.push_back({name, "LineData", color});

        } else if (data_type == "digital_event") {

            if (item["format"] == "uint16") {

                int channel = item["channel"];
                std::string transition = item["transition"];
                int header_size = item.value("header_size", 0);

                auto data = readBinaryFile<uint16_t>(file_path, header_size);

                auto digital_data = extractDigitalData(data, channel);
                auto events = extractEvents(digital_data, transition);
                std::cout << "Loaded " << events.size() << " events for " << name << std::endl;

                auto digital_event_series = std::make_shared<DigitalEventSeries>();
                digital_event_series->setData(events);
                dm->setData<DigitalEventSeries>(name, digital_event_series);
            }
        } else if (data_type == "digital_interval") {

            if (item["format"] == "uint16") {

                int channel = item["channel"];
                std::string transition = item["transition"];
                int header_size = item.value("header_size", 0);

                auto data = readBinaryFile<uint16_t>(file_path, header_size);

                auto digital_data = extractDigitalData(data, channel);

                auto intervals = extractIntervals(digital_data, transition);
                std::cout << "Loaded " << intervals.size() << " intervals for " << name << std::endl;

                auto digital_interval_series = std::make_shared<DigitalIntervalSeries>();
                digital_interval_series->setData(intervals);
                dm->setData<DigitalIntervalSeries>(name, digital_interval_series);

            } else if (item["format"] == "csv") {

                auto intervals =  CSVLoader::loadPairColumnCSV(file_path);
                std::cout << "Loaded " << intervals.size() << " intervals for " << name << std::endl;
                auto digital_interval_series = std::make_shared<DigitalIntervalSeries>();
                digital_interval_series->setData(intervals);
                dm->setData<DigitalIntervalSeries>(name, digital_interval_series);
            }
        } else if (data_type == "time") {

            if (item["format"] == "uint16") {

                int channel = item["channel"];
                std::string transition = item["transition"];
                int header_size = item.value("header_size", 0);

                auto data = readBinaryFile<uint16_t>(file_path, header_size);

                auto digital_data = extractDigitalData(data, channel);
                auto events = extractEvents(digital_data, transition);

                // convert to int with std::transform
                std::vector<int> events_int;
                for (auto e : events) {
                    events_int.push_back(e);
                }
                std::cout << "Loaded " << events_int.size() << " events for " << name << std::endl;

                auto timeframe = std::make_shared<TimeFrame>(events_int);
                dm->setTime(name, timeframe);
            }

            if (item["format"] == "uint16_length") {

                int header_size = item.value("header_size", 0);

                auto data = readBinaryFile<uint16_t>(file_path, header_size);

                std::vector<int> t(data.size()) ;
                std::iota (std::begin(t), std::end(t), 0);

                std::cout << "Total of " << t.size() << " timestamps for " << name << std::endl;

                auto timeframe = std::make_shared<TimeFrame>(t);
                dm->setTime(name, timeframe);
            }
        }
        if (item.contains("clock")) {
            std::string clock = item["clock"];
            std::cout << "Setting time for " << name << " to " << clock << std::endl;
            dm->setTimeFrame(name, clock);
        }
    }

    return data_info_list;
}

std::string DataManager::getType(const std::string& key) const {
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
        }
        return "Unknown";
    }
}

