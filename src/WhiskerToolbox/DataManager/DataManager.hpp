#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"
#include "TimeFrame.hpp"

#include <functional> // std::function
#include <memory> // std::shared_ptr
#include <iostream>
#include <string> // std::string
#include <unordered_map> // std::unordered_map
#include <utility> // std::move
#include <variant> // std::variant
#include <vector> // std::vector

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class PointData;
class LineData;
class MaskData;

struct DataInfo {
    std::string key;
    std::string data_class;
    std::string color;
};

struct DataGroup {
    std::string groupName;
    std::vector<std::string> dataKeys;
};

class DataManager {

public:
    DataManager();

    void setMedia(std::shared_ptr<MediaData> media) {
        _data["media"] = media;
    };

    void setTime(const std::string& key, std::shared_ptr<TimeFrame> timeframe) {
        _times[key] = timeframe;
    };

    std::shared_ptr<TimeFrame> getTime() {
        return _times["time"];
    };

    std::shared_ptr<TimeFrame> getTime(const std::string& key) {
        if (_times.find(key) != _times.end()) {
            return _times[key];
        }
        return nullptr;
    };
    //std::shared_ptr<TimeFrame> getTime() {return _time;};

    using ObserverCallback = std::function<void()>;

    void addObserver(ObserverCallback callback) {
        _observers.push_back(std::move(callback));
    }

    // Method to notify all observers of a change
    void notifyObservers() {
        for (auto& observer : _observers) {
            observer(); // Call the observer callback
        }
    }

    void loadFromJSON(std::string const & filepath);

    void load_A(const std::string & filepath);
    void load_B(const std::string & filepath);

    std::vector<std::string> getAllKeys()
    {
        std::vector<std::string> keys;
        for (const auto& [key, value] : _data) {

            keys.push_back(key);

        }
        return keys;
    }

    template<typename T>
    std::vector<std::string> getKeys()
    {
        std::vector<std::string> keys;
        for (const auto& [key, value] : _data) {
            if (std::holds_alternative<std::shared_ptr<T>>(value)) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    template<typename T>
    std::shared_ptr<T> getData(const std::string& key) {
        if (_data.find(key) != _data.end()) {
            return std::get<std::shared_ptr<T>>(_data[key]);
        }
        return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> getDataFromGroup(const std::string& group_key, int index) {
        if (_dataGroups.find(group_key) != _dataGroups.end()) {

            if (index >= _dataGroups.at(group_key).size()) {
                return nullptr;
            }
            if (index < 0) {
                return nullptr;
            }

            auto keys = _dataGroups.at(group_key);
            if (std::holds_alternative<std::shared_ptr<T>>(_data[keys[index]])) {
                return std::get<std::shared_ptr<T>>(_data[keys[index]]);
            }
        }
        return nullptr;
    }

    template<typename T>
    void setData(const std::string& key){
        _data[key] = std::make_shared<T>();
        setTimeFrame(key, "time");
    }

    template<typename T>
    void setData(const std::string& key, std::shared_ptr<T> data){
        _data[key] = data;
        setTimeFrame(key, "time");
    }

    template<typename T>
    void setData(const std::string& key, std::shared_ptr<T> data, const std::string time_key){
        _data[key] = data;
        setTimeFrame(key, time_key);
    }

    std::string getType(const std::string& key) const;

    void setTimeFrame(std::string data_key, std::string time_key);
    std::string getTimeFrame(std::string data_key) {
        return _time_frames[data_key];
    }

    std::vector<std::string> getTimeFrameKeys()
    {
        std::vector<std::string> keys;
        for (const auto& [key, value] : _times) {

            keys.push_back(key);

        }
        return keys;
    }

    void createDataGroup(const std::string& groupName) {
        _dataGroups[groupName] = {};
    }

    void createDataGroup(const std::string& groupName, const std::vector<std::string>& dataKeys) {
        _dataGroups[groupName] = dataKeys;
    }

    std::vector<std::string> getDataGroup(const std::string& groupName) const {
        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            return _dataGroups.at(groupName);
        }
        return {};
    }

    bool isDataGroup(const std::string& groupName) const {
        return _dataGroups.find(groupName) != _dataGroups.end();
    }

    std::vector<std::string> getKeysInDataGroup(const std::string& groupName) const {
        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            return _dataGroups.at(groupName);
        }
        return {};
    }

    std::vector<std::string> getDataGroupNames() {
        std::vector<std::string> groupNames;
        for (const auto& [key, value] : _dataGroups) {
            groupNames.push_back(key);
        }
        return groupNames;
    }

    void addDataToGroup(const std::string& groupName, const std::string& dataKey) {

        //Check if key is in _data
        if (_data.find(dataKey) == _data.end()) {
            std::cerr << "Data key not found in DataManager: " << dataKey << std::endl;
            return;
        }

        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            _dataGroups[groupName].push_back(dataKey);
        }
    }

private:

    //std::shared_ptr<TimeFrame> _time;
    std::unordered_map<std::string, std::shared_ptr<TimeFrame>> _times;

    std::vector<ObserverCallback> _observers;

    std::unordered_map<std::string, std::variant<
                                        std::shared_ptr<MediaData>,
                                        std::shared_ptr<PointData>,
                                        std::shared_ptr<LineData>,
                                        std::shared_ptr<MaskData>,
                                        std::shared_ptr<AnalogTimeSeries>,
                                        std::shared_ptr<DigitalEventSeries>,
                                        std::shared_ptr<DigitalIntervalSeries>>> _data;

    std::unordered_map<std::string, std::string> _time_frames;

    std::unordered_map<std::string, std::vector<std::string>> _dataGroups;

};

std::vector<std::vector<float>> read_ragged_hdf5(std::string const & filepath, std::string const & key);

std::vector<int> read_array_hdf5(std::string const & filepath, std::string const & key);

std::vector<DataInfo> load_data_from_json_config(std::shared_ptr<DataManager>, std::string json_filepath);


#endif // DATAMANAGER_HPP
