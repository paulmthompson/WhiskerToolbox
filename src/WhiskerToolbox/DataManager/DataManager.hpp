#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "DataManagerTypes.hpp"

#include "TimeFrame.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <filesystem>
#include <functional>   // std::function
#include <memory>       // std::shared_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <unordered_map>// std::unordered_map
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
    bool setTime(std::string const & key, std::shared_ptr<TimeFrame> timeframe, bool overwrite = false);

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

    bool removeTime(std::string const & key);

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

    // ========== New TimeFrameV2 API (Parallel System) ==========

    /**
    * @brief Register a new strongly-typed temporal coordinate system
    *
    * Stores a TimeFrameV2 object with strong coordinate types in the DataManager.
    * This is the new API that provides type safety and memory efficiency.
    *
    * @param key The unique identifier for this TimeFrameV2
    * @param timeframe The TimeFrameV2 variant to register
    * @param overwrite Whether to overwrite an existing key
    * @return bool True if the TimeFrameV2 was successfully registered, false otherwise
    *
    * @note This API works in parallel with the legacy setTime() API
    */
    bool setTimeV2(std::string const & key, AnyTimeFrame timeframe, bool overwrite = false);

    /**
    * @brief Get a strongly-typed TimeFrameV2 by key
    *
    * @param key The key to retrieve the TimeFrameV2 for
    * @return Optional AnyTimeFrame variant if the key exists, nullopt otherwise
    */
    [[nodiscard]] std::optional<AnyTimeFrame> getTimeV2(std::string const & key);

    /**
    * @brief Remove a TimeFrameV2 from the registry
    *
    * @param key The key of the TimeFrameV2 to remove
    * @return bool True if the TimeFrameV2 was found and removed, false otherwise
    */
    bool removeTimeV2(std::string const & key);

    /**
    * @brief Get all registered TimeFrameV2 keys
    *
    * @return A vector of strings containing all TimeFrameV2 keys
    */
    [[nodiscard]] std::vector<std::string> getTimeFrameV2Keys();

    /**
    * @brief Helper to create and register a dense ClockTimeFrame
    *
    * @param key The unique identifier for this TimeFrame
    * @param start_tick Starting tick value
    * @param num_samples Number of samples
    * @param sampling_rate_hz Sampling rate in Hz
    * @param overwrite Whether to overwrite an existing key
    * @return bool True if successfully created and registered
    */
    bool createClockTimeFrame(std::string const & key, int64_t start_tick, 
                             int64_t num_samples, double sampling_rate_hz, 
                             bool overwrite = false);

    /**
    * @brief Helper to create and register a sparse CameraTimeFrame
    *
    * @param key The unique identifier for this TimeFrame
    * @param frame_indices Vector of camera frame indices
    * @param overwrite Whether to overwrite an existing key
    * @return bool True if successfully created and registered
    */
    bool createCameraTimeFrame(std::string const & key, std::vector<int64_t> frame_indices, 
                              bool overwrite = false);

    /**
    * @brief Helper to create and register a dense CameraTimeFrame
    *
    * @param key The unique identifier for this TimeFrame
    * @param start_frame Starting frame index
    * @param num_frames Number of frames
    * @param overwrite Whether to overwrite an existing key
    * @return bool True if successfully created and registered
    */
    bool createDenseCameraTimeFrame(std::string const & key, int64_t start_frame, 
                                   int64_t num_frames, bool overwrite = false);

    int64_t getCurrentTime() { return _current_time; };
    void setCurrentTime(int64_t time) { _current_time = time; }

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

    /**
    * @brief Get all registered data keys
    *
    * Retrieves a vector of all data object keys in the DataManager.
    *
    * @return A vector of strings containing all data keys
    *
    * @example
    * @code
    * DataManager dm;
    * dm.setData<PointData>("points1");
    * dm.setData<LineData>("line1");
    *
    * // Get all keys
    * auto keys = dm.getAllKeys(); // Returns ["media", "points1", "line1"]
    * @endcode
    */
    [[nodiscard]] std::vector<std::string> getAllKeys();

    /**
    * @brief Get all keys associated with a specific data type
    *
    * Retrieves a vector of all keys that correspond to data objects
    * of the specified template type T.
    *
    * @tparam T The data type to filter by (e.g., PointData, LineData)
    * @return A vector of strings containing all keys with data of type T
    *
    * @example
    * @code
    * DataManager dm;
    * dm.setData<PointData>("points1");
    * dm.setData<PointData>("points2");
    * dm.setData<LineData>("line1");
    *
    * // Get only keys for PointData objects
    * auto pointKeys = dm.getKeys<PointData>(); // Returns ["points1", "points2"]
    * @endcode
    */
    template<typename T>
    [[nodiscard]] std::vector<std::string> getKeys() {
        std::vector<std::string> keys;
        for (auto const & [key, value]: _data) {
            if (std::holds_alternative<std::shared_ptr<T>>(value)) {
                keys.push_back(key);
            }
        }
        return keys;
    }

    /**
    * @brief Get data as a variant type
    *
    * Retrieves data associated with the specified key as a variant wrapper,
    * allowing access without knowing the concrete type at compile time.
    *
    * @param key The key associated with the data to retrieve
    * @return An optional containing the data variant if the key exists, empty optional otherwise
    *
    */
    std::optional<DataTypeVariant> getDataVariant(std::string const & key);

    template<typename T>
    std::shared_ptr<T> getData(std::string const & key) {
        if (_data.find(key) != _data.end()) {
            return std::get<std::shared_ptr<T>>(_data[key]);
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

    void setData(std::string const & key, DataTypeVariant data);

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data, std::string const & time_key) {
        _data[key] = data;
        setTimeFrame(key, time_key);
        _notifyObservers();
    }

    // ========== New setData overloads for TimeFrameV2 ==========

    /**
    * @brief Set data with a direct TimeFrameV2 reference
    *
    * This new API allows data objects to hold direct references to their TimeFrameV2,
    * providing better type safety and eliminating the need for string-based lookups.
    *
    * @tparam T The data type (must support setTimeFrameV2() method)
    * @param key The data key
    * @param data The data object
    * @param timeframe_v2 The TimeFrameV2 variant to associate with this data
    * @param timeframe_key Optional key to also register the TimeFrameV2 in the registry
    */
    template<typename T>
    void setDataV2(std::string const & key, std::shared_ptr<T> data, 
                   AnyTimeFrame timeframe_v2, 
                   std::optional<std::string> timeframe_key = std::nullopt) {
        // Set the direct TimeFrame reference in the data object
        if constexpr (requires { data->setTimeFrameV2(timeframe_v2); }) {
            data->setTimeFrameV2(timeframe_v2);
        }
        
        // Store the data
        _data[key] = data;
        
        // Optionally register the TimeFrame in our registry
        if (timeframe_key.has_value()) {
            setTimeV2(timeframe_key.value(), timeframe_v2, true);
            _time_frames_v2[key] = timeframe_key.value();
        }
        
        _notifyObservers();
    }

    /**
    * @brief Set data with a TimeFrameV2 key reference
    *
    * Similar to the legacy setData(key, data, time_key) but uses the TimeFrameV2 registry.
    *
    * @tparam T The data type (must support setTimeFrameV2() method)
    * @param key The data key
    * @param data The data object
    * @param timeframe_v2_key The key of a TimeFrameV2 already registered in the system
    */
    template<typename T>
    void setDataV2(std::string const & key, std::shared_ptr<T> data, 
                   std::string const & timeframe_v2_key) {
        auto timeframe_opt = getTimeV2(timeframe_v2_key);
        if (!timeframe_opt.has_value()) {
            std::cerr << "Error: TimeFrameV2 key not found: " << timeframe_v2_key << std::endl;
            return;
        }
        
        // Set the direct TimeFrame reference in the data object
        if constexpr (requires { data->setTimeFrameV2(timeframe_opt.value()); }) {
            data->setTimeFrameV2(timeframe_opt.value());
        }
        
        // Store the data
        _data[key] = data;
        _time_frames_v2[key] = timeframe_v2_key;
        
        _notifyObservers();
    }

    [[nodiscard]] DM_DataType getType(std::string const & key) const;

    void setOutputPath(std::filesystem::path const & output_path) { _output_path = output_path; };

    [[nodiscard]] std::filesystem::path getOutputPath() const {
        return _output_path;
    }

    // ========== Enhanced AnalogTimeSeries Support ==========

    /**
    * @brief Create AnalogTimeSeries with ClockTicks timeframe 
    *
    * Convenience method that creates both the data and timeframe in one call.
    * Ideal for neural/physiological data sampled at regular rates.
    *
    * @param data_key Unique key for the analog data
    * @param timeframe_key Unique key for the timeframe (optional)
    * @param analog_data Vector of analog values
    * @param start_tick Starting clock tick
    * @param sampling_rate_hz Sampling rate in Hz
    * @param overwrite Whether to overwrite existing keys
    * @return bool True if successfully created
    */
    bool createAnalogTimeSeriesWithClock(std::string const & data_key,
                                        std::string const & timeframe_key,
                                        std::vector<float> analog_data,
                                        int64_t start_tick,
                                        double sampling_rate_hz,
                                        bool overwrite = false);

    /**
    * @brief Create AnalogTimeSeries with CameraFrameIndex timeframe
    *
    * Convenience method for data synchronized to camera frames.
    * Useful for behavioral or imaging data.
    *
    * @param data_key Unique key for the analog data
    * @param timeframe_key Unique key for the timeframe (optional)
    * @param analog_data Vector of analog values
    * @param frame_indices Vector of camera frame indices
    * @param overwrite Whether to overwrite existing keys
    * @return bool True if successfully created
    */
    bool createAnalogTimeSeriesWithCamera(std::string const & data_key,
                                         std::string const & timeframe_key,
                                         std::vector<float> analog_data,
                                         std::vector<int64_t> frame_indices,
                                         bool overwrite = false);

    /**
    * @brief Create AnalogTimeSeries with dense camera frames
    *
    * Convenience method for data from regularly sampled camera frames.
    *
    * @param data_key Unique key for the analog data
    * @param timeframe_key Unique key for the timeframe
    * @param analog_data Vector of analog values
    * @param start_frame Starting camera frame index
    * @param overwrite Whether to overwrite existing keys
    * @return bool True if successfully created
    */
    bool createAnalogTimeSeriesWithDenseCamera(std::string const & data_key,
                                              std::string const & timeframe_key,
                                              std::vector<float> analog_data,
                                              int64_t start_frame,
                                              bool overwrite = false);

    /**
    * @brief Query analog data using any coordinate type (runtime type checking)
    *
    * This method allows you to query any AnalogTimeSeries regardless of its 
    * coordinate type, as long as you provide matching coordinates.
    *
    * @param data_key Key of the AnalogTimeSeries to query
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type)
    * @return Vector of values in the specified range
    * @throws std::runtime_error if data doesn't exist or coordinate types don't match
    */
    std::vector<float> queryAnalogData(std::string const & data_key,
                                      TimeCoordinate start_coord,
                                      TimeCoordinate end_coord);

    /**
    * @brief Query analog data and coordinates using any coordinate type
    *
    * @param data_key Key of the AnalogTimeSeries to query
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type)
    * @return Pair of coordinate vector and value vector
    */
    std::pair<std::vector<TimeCoordinate>, std::vector<float>> queryAnalogDataWithCoords(
            std::string const & data_key,
            TimeCoordinate start_coord,
            TimeCoordinate end_coord);

    /**
    * @brief Get coordinate type information for an AnalogTimeSeries
    *
    * @param data_key Key of the AnalogTimeSeries
    * @return String describing the coordinate type ("ClockTicks", "CameraFrameIndex", etc.)
    */
    std::string getAnalogCoordinateType(std::string const & data_key);

    /**
    * @brief Check if AnalogTimeSeries uses a specific coordinate type
    *
    * @tparam CoordinateType The coordinate type to check for
    * @param data_key Key of the AnalogTimeSeries
    * @return True if the series uses the specified coordinate type
    */
    template<typename CoordinateType>
    bool analogUsesCoordinateType(std::string const & data_key);

private:
    std::unordered_map<std::string, std::shared_ptr<TimeFrame>> _times;

    std::vector<ObserverCallback> _observers;

    std::unordered_map<std::string, DataTypeVariant>
            _data;

    std::unordered_map<std::string, std::string> _time_frames;

    // ========== New TimeFrameV2 storage ==========
    std::unordered_map<std::string, AnyTimeFrame> _times_v2;
    std::unordered_map<std::string, std::string> _time_frames_v2;

    std::filesystem::path _output_path;

    void _notifyObservers();

    int64_t _current_time{0};

    // Helper function for template method (implemented in .cpp)
    bool analogUsesCoordinateTypeImpl(std::string const & data_key, std::string const & type_name);
};

std::vector<DataInfo> load_data_from_json_config(DataManager *, std::string const & json_filepath);

std::string convert_data_type_to_string(DM_DataType type);

// ========== Template implementations ==========

template<typename CoordinateType>
bool DataManager::analogUsesCoordinateType(std::string const & data_key) {
    // Map coordinate types to string identifiers
    std::string type_name;
    if constexpr (std::is_same_v<CoordinateType, ClockTicks>) {
        type_name = "ClockTicks";
    } else if constexpr (std::is_same_v<CoordinateType, CameraFrameIndex>) {
        type_name = "CameraFrameIndex";
    } else if constexpr (std::is_same_v<CoordinateType, Seconds>) {
        type_name = "Seconds";
    } else if constexpr (std::is_same_v<CoordinateType, UncalibratedIndex>) {
        type_name = "UncalibratedIndex";
    } else {
        return false; // Unknown coordinate type
    }
    
    return analogUsesCoordinateTypeImpl(data_key, type_name);
}

#endif// DATAMANAGER_HPP
