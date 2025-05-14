#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "DataManagerTypes.hpp"

#include "TimeFrame.hpp"

#include <filesystem>
#include <functional>   // std::function
#include <iostream>
#include <memory>       // std::shared_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <unordered_map>// std::unordered_map
#include <utility>      // std::move
#include <variant>      // std::variant
#include <vector>       // std::vector

class DataManager {

public:
    DataManager();

    /**
    * @brief Register a new temporal coordinate system with a unique key
    *
    * This function stores a TimeFrame object in the DataManager under the provided key.
    * The TimeFrame specifies the temporal coordinate system that can be assigned to data objects.
    *
    * @param key The unique identifier for this TimeFrame
    * @param timeframe The TimeFrame object to register
    * @return bool True if the TimeFrame was successfully registered, false otherwise
    *
    * @note If the key already exists or timeframe is nullptr, a warning message will be printed
    *       to std::cerr and the function will return false
    */
    bool setTime(std::string const & key, std::shared_ptr<TimeFrame> timeframe);

    /**
    * @brief Get the default time frame object
    *
    * Returns the TimeFrame associated with the default "time" key.
    *
    * @return A shared pointer to the default TimeFrame object
    * @note This function always returns a valid pointer since the default TimeFrame
    *       is created in the constructor
    */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTime();

    /**
    * @brief Get the time frame object for a specific key
    *
    * @param key The key to retrieve the TimeFrame for
    * @return A shared pointer to the TimeFrame if the key exists, nullptr otherwise
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTime(std::string const & key);

    /**
    * @brief Set the time frame for a specific data key
    *
    * Associates a data object with a specific temporal coordinate system.
    *
    * @param data_key The data key to set the time frame for
    * @param time_key The time key to associate with the data
    * @return bool True if the time frame was successfully set, false otherwise
    *
    * @note If data_key or time_key doesn't exist, an error message will be printed
    *       to std::cerr and the function will return false
    */
    bool setTimeFrame(std::string const & data_key, std::string const & time_key);

    /**
    * @brief Get the time frame for a specific data key
    *
    * Retrieves the TimeFrame key associated with a particular data object.
    *
    * @param data_key The data key to get the time frame for
    * @return The TimeFrame key as a string, or empty string if an error occurred
    *
    * @note If data_key doesn't exist or doesn't have an associated TimeFrame,
    *       an error message will be printed to std::cerr and an empty string will be returned
    */
    [[nodiscard]] std::string getTimeFrame(std::string const & data_key);

    /**
    * @brief Get all registered TimeFrame keys
    *
    * Retrieves a vector of all keys used for TimeFrame objects in the DataManager.
    *
    * @return A vector of strings containing all TimeFrame keys
    * @note The default "time" key will always be included in the returned vector
    */
    [[nodiscard]] std::vector<std::string> getTimeFrameKeys();

    using ObserverCallback = std::function<void()>;

    /**
    * @brief Add a callback function to a specific data object
    *
    * Registers a callback function that will be invoked when the specified data object changes.
    *
    * @param key The data key to attach the callback to
    * @param callback The function to call when the data changes
    * @return int A unique identifier for the callback (>= 0) or -1 if registration failed
    *
    * @note If the data key doesn't exist, the function returns -1
    */
    [[nodiscard]] int addCallbackToData(std::string const & key, ObserverCallback callback);

    /**
    * @brief Remove a callback from a specific data object
    *
    * Removes a previously registered callback using its unique identifier.
    *
    * @param key The data key from which to remove the callback
    * @param callback_id The unique identifier of the callback to remove
    * @return bool True if the callback was successfully removed, false if the key doesn't exist
    *
    * @note If the data key doesn't exist, the function returns false.
    *       The underlying removeObserver implementation determines what happens if the callback_id is invalid.
    */
    bool removeCallbackFromData(std::string const & key, int callback_id);

    /**
    * @brief Register a callback function for DataManager state changes
    *
    * Adds a callback function that will be invoked when the DataManager's state changes,
    * such as when data is added or modified.
    *
    * @param callback The function to call when the DataManager state changes
    *
    * @note Unlike addCallbackToData, this function doesn't return an ID,
    *       so callbacks cannot be selectively removed later
    */
    void addObserver(ObserverCallback callback);

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

    std::optional<DataTypeVariant> getDataVariant(std::string const & key) {
        if (_data.find(key) != _data.end()) {
            return _data[key];
        }
        return std::nullopt;
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
        _notifyObservers();
    }

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data) {
        _data[key] = data;
        setTimeFrame(key, "time");
        _notifyObservers();
    }

    void setData(std::string const & key, DataTypeVariant data) {
        _data[key] = data;
        setTimeFrame(key, "time");
        _notifyObservers();
    }

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data, std::string const & time_key) {
        _data[key] = data;
        setTimeFrame(key, time_key);
        _notifyObservers();
    }

    [[nodiscard]] DM_DataType getType(std::string const & key) const;

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

    void setOutputPath(std::filesystem::path const & output_path) { _output_path = output_path; };

    [[nodiscard]] std::filesystem::path getOutputPath() const {
        return _output_path;
    }

private:

    std::unordered_map<std::string, std::shared_ptr<TimeFrame>> _times;

    std::vector<ObserverCallback> _observers;

    std::unordered_map<std::string, DataTypeVariant>
            _data;

    std::unordered_map<std::string, std::string> _time_frames;

    std::unordered_map<std::string, std::vector<std::string>> _dataGroups;

    std::filesystem::path _output_path;

    void _notifyObservers();
};

std::vector<DataInfo> load_data_from_json_config(DataManager *, std::string const & json_filepath);

std::string convert_data_type_to_string(DM_DataType type);

#endif// DATAMANAGER_HPP
