#include <fstream>
#include <filesystem>

#include "DataManager.hpp"

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

void DataManager::createPoint(std::string const & point_key)
{
    _points[point_key] = std::make_shared<PointData>();
}

std::shared_ptr<PointData> DataManager::getPoint(std::string const & point_key)
{
    return _points[point_key];
}

std::vector<std::string> DataManager::getPointKeys()
{
    return get_keys(_points);
}

void DataManager::createLine(const std::string line_key)
{
    _lines[line_key] = std::make_shared<LineData>();
}

std::shared_ptr<LineData> DataManager::getLine(const std::string line_key)
{

    return _lines[line_key];
}

std::vector<std::string> DataManager::getLineKeys()
{
    return get_keys(_lines);
}

void DataManager::createMask(const std::string& mask_key)
{
    _masks[mask_key] = std::make_shared<MaskData>();
}

void DataManager::createMask(const std::string& mask_key, int const width, int const height)
{
    _masks[mask_key] = std::make_shared<MaskData>();
    _masks[mask_key]->setMaskWidth(width);
    _masks[mask_key]->setMaskHeight(height);
}

std::shared_ptr<MaskData> DataManager::getMask(const std::string& mask_key)
{

    return _masks[mask_key];
}

std::vector<std::string> DataManager::getMaskKeys()
{
    return get_keys(_masks);
}

void DataManager::createAnalogTimeSeries(std::string const & key)
{
    _analog[key] = std::make_shared<AnalogTimeSeries>();
}

std::shared_ptr<AnalogTimeSeries> DataManager::getAnalogTimeSeries(std::string const & analog_key)
{
    return _analog[analog_key];
}

std::vector<std::string> DataManager::getAnalogTimeSeriesKeys()
{
    return get_keys(_analog);
}

void DataManager::createDigitalTimeSeries(std::string const & digital_key)
{
    _digital[digital_key] = std::make_shared<DigitalIntervalSeries>();
}

std::shared_ptr<DigitalIntervalSeries> DataManager::getDigitalTimeSeries(std::string const & digital_key)
{
    return _digital[digital_key];
}

std::vector<std::string> DataManager::getDigitalTimeSeriesKeys()
{
    return get_keys(_digital);
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
