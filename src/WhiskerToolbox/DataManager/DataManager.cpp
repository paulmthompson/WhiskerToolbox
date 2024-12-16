#include <fstream>
#include <filesystem>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Media/Video_Data.hpp"

#include "TimeFrame.hpp"

#include "utils/hdf5_mask_load.hpp"
#include "utils/container.hpp"
#include "utils/glob.hpp"

#include "nlohmann/json.hpp"
using namespace nlohmann;

DataManager::DataManager() :
    _time{std::make_shared<TimeFrame>()}
{
    _data["media"] = std::make_shared<MediaData>();
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

        // If entry doesn't have a data_type and filepath, skip
        if (!item.contains("data_type") || !item.contains("filepath")) {
            std::cerr << "Entry missing data_type or filepath" << std::endl;
            continue;
        }
        std::string data_type = item["data_type"];
        std::string file_path = item["filepath"];
        std::string name = item["name"];

        // Check if the file path is relative
        if (!std::filesystem::path(file_path).is_absolute()) {
            file_path = (base_path / file_path).string();
        }

        if (data_type == "video") {
            // Create VideoData object
            auto video_data = std::make_shared<VideoData>();
            //check if the file exists
            if (!std::filesystem::exists(file_path)) {
                std::cerr << "File does not exist: " << file_path << std::endl;
                continue;
            }

            std::cout << "Loading video file: " << file_path << std::endl;
            video_data->LoadMedia(file_path);

            std::cout << "Video has " << video_data->getTotalFrameCount() << " frames" << std::endl;
            // Add VideoData to DataManager
            dm->setMedia(video_data);

            data_info_list.push_back({name, "VideoData", ""});
        } else if (data_type == "points") {

            if (!std::filesystem::exists(file_path)) {
                std::cerr << "File does not exist: " << file_path << std::endl;
                continue;
            }

            int frame_column = item["frame_column"];
            int x_column = item["x_column"];
            int y_column = item["y_column"];

            std::string color = "0000FF";
            if (item.contains("color"))
            {
                color = item["color"];
            }

            std::cout << "Loading points file: " << file_path << std::endl;

            auto keypoints = load_points_from_csv(
                    file_path,
                    frame_column,
                    x_column,
                    y_column);

            auto keypoint_key = name;

            std::cout << "There are " <<  keypoints.size() << " keypoints " << std::endl;

            auto point_data = std::make_shared<PointData>(keypoints);

            dm->setData<PointData>(keypoint_key, point_data);

            data_info_list.push_back({keypoint_key, "PointData", color});
        }
    }

    return data_info_list;
}
