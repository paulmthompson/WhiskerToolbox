#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "Media/Media_Data.hpp"
#include "TimeFrame.hpp"

#include <filesystem>
#include <functional>// std::function
#include <iostream>
#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <unordered_map>// std::unordered_map
#include <utility>      // std::move
#include <variant>      // std::variant
#include <vector>       // std::vector

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class LineData;
class MaskData;
class PointData;
class TensorData;

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

    void setTime(std::string const & key, std::shared_ptr<TimeFrame> timeframe) {
        _times[key] = std::move(timeframe);
    };

    std::shared_ptr<TimeFrame> getTime() {
        return _times["time"];
    };

    std::shared_ptr<TimeFrame> getTime(std::string const & key) {
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
        for (auto & observer: _observers) {
            observer();// Call the observer callback
        }
    }

    std::vector<std::string> getAllKeys() {
        std::vector<std::string> keys;
        keys.reserve(_data.size());
        for (auto const & [key, value]: _data) {

            keys.push_back(key);
        }
        return keys;
    }

    template<typename T>
    std::vector<std::string> getKeys() {
        std::vector<std::string> keys;
        for (auto const & [key, value]: _data) {
            if (std::holds_alternative<std::shared_ptr<T>>(value)) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    template<typename T>
    std::shared_ptr<T> getData(std::string const & key) {
        if (_data.find(key) != _data.end()) {
            return std::get<std::shared_ptr<T>>(_data[key]);
        }
        return nullptr;
    }

    template<typename T>
    std::shared_ptr<T> getDataFromGroup(std::string const & group_key, int index) {
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
    void setData(std::string const & key) {
        _data[key] = std::make_shared<T>();
        setTimeFrame(key, "time");
        notifyObservers();
    }

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data) {
        _data[key] = data;
        setTimeFrame(key, "time");
        notifyObservers();
    }

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data, std::string const & time_key) {
        _data[key] = data;
        setTimeFrame(key, time_key);
        notifyObservers();
    }

    [[nodiscard]] std::string getType(std::string const & key) const;

    void setTimeFrame(std::string const & data_key, std::string const & time_key);
    std::string getTimeFrame(std::string const & data_key) {
        return _time_frames[data_key];
    }

    std::vector<std::string> getTimeFrameKeys() {
        std::vector<std::string> keys;
        keys.reserve(_times.size());
        for (auto const & [key, value]: _times) {

            keys.push_back(key);
        }
        return keys;
    }

    void createDataGroup(std::string const & groupName) {
        _dataGroups[groupName] = {};
    }

    void createDataGroup(std::string const & groupName, std::vector<std::string> const & dataKeys) {
        _dataGroups[groupName] = dataKeys;
    }

    [[nodiscard]] std::vector<std::string> getDataGroup(std::string const & groupName) const {
        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            return _dataGroups.at(groupName);
        }
        return {};
    }

    [[nodiscard]] bool isDataGroup(std::string const & groupName) const {
        return _dataGroups.find(groupName) != _dataGroups.end();
    }

    [[nodiscard]] std::vector<std::string> getKeysInDataGroup(std::string const & groupName) const {
        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            return _dataGroups.at(groupName);
        }
        return {};
    }

    std::vector<std::string> getDataGroupNames() {
        std::vector<std::string> groupNames;
        groupNames.reserve(_dataGroups.size());
        for (auto const & [key, value]: _dataGroups) {
            groupNames.push_back(key);
        }
        return groupNames;
    }

    void addDataToGroup(std::string const & groupName, std::string const & dataKey) {

        //Check if key is in _data
        if (_data.find(dataKey) == _data.end()) {
            std::cerr << "Data key not found in DataManager: " << dataKey << std::endl;
            return;
        }

        if (_dataGroups.find(groupName) != _dataGroups.end()) {
            _dataGroups[groupName].push_back(dataKey);
        }
    }

    int addCallbackToData(std::string const & key, ObserverCallback callback);
    void removeCallbackFromData(std::string const & key, int callback_id);

    void setOutputPath(std::filesystem::path const & output_path) { _output_path = output_path; };

    [[nodiscard]] std::filesystem::path getOutputPath() const {
        return _output_path;
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
                                            std::shared_ptr<DigitalIntervalSeries>,
                                            std::shared_ptr<TensorData>>>
            _data;

    std::unordered_map<std::string, std::string> _time_frames;

    std::unordered_map<std::string, std::vector<std::string>> _dataGroups;

    std::filesystem::path _output_path;
};

std::vector<DataInfo> load_data_from_json_config(DataManager *, std::string const & json_filepath);


#endif// DATAMANAGER_HPP
