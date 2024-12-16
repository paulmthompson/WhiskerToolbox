#include <fstream>
#include <filesystem>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

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
