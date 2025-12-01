#ifndef DATAMANAGER_HPP
#define DATAMANAGER_HPP

#include "DataManagerTypes.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <functional>   // std::function
#include <memory>       // std::shared_ptr
#include <optional>     // std::optional
#include <string>       // std::string
#include <unordered_map>// std::unordered_map
#include <variant>      // std::variant
#include <vector>       // std::vector

#include "nlohmann/json_fwd.hpp"

// Forward declarations for identity
class EntityRegistry;
class EntityGroupManager;

class TableRegistry;
struct TableEvent;

class DataManager {

public:
    DataManager();
    ~DataManager();
    // ======= Table Registry access =======
    /**
     * @brief Get the centralized TableRegistry owned by this DataManager
     */
    TableRegistry * getTableRegistry();
    TableRegistry const * getTableRegistry() const;

    // ======= Table observer channel (separate from data observers) =======
    using TableObserver = std::function<void(TableEvent const &)>;
    /**
     * @brief Subscribe to table events
     * @return Subscription id (>=1) or -1 on failure
     */
    [[nodiscard]] int addTableObserver(TableObserver callback);
    /**
     * @brief Unsubscribe from table events
     */
    bool removeTableObserver(int callback_id);

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
    bool setTime(TimeKey const & key, std::shared_ptr<TimeFrame> timeframe, bool overwrite = false);

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
    [[nodiscard]] std::shared_ptr<TimeFrame> getTime(TimeKey const & key);

    [[nodiscard]] TimeIndexAndFrame getCurrentIndexAndFrame(TimeKey const & key);

    bool removeTime(TimeKey const & key);

    /**
    * @brief Set the time key for a specific data key
    *
    * Associates a data object with a specific temporal coordinate system.
    *
    * @param data_key The data key to set the time key for
    * @param time_key The time key to associate with the data
    * @return bool True if the time frame was successfully set, false otherwise
    *
    * @note If data_key or time_key doesn't exist, an error message will be printed
    *       to std::cerr and the function will return false
    */
    bool setTimeKey(std::string const & data_key, TimeKey const & time_key);

    /**
    * @brief Get the time key for a specific data key
    *
    * Retrieves the TimeFrame key associated with a particular data object.
    *
    * @param data_key The data key to get the time frame for
    * @return The TimeKey, or empty string if an error occurred
    *
    * @note If data_key doesn't exist or doesn't have an associated TimeFrame,
    *       an error message will be printed to std::cerr and an empty string will be returned
    */
    [[nodiscard]] TimeKey getTimeKey(std::string const & data_key);

    /**
    * @brief Get all registered TimeFrame keys
    *
    * Retrieves a vector of all keys used for TimeFrame objects in the DataManager.
    *
    * @return A vector of strings containing all TimeFrame keys
    * @note The default "time" key will always be included in the returned vector
    */
    [[nodiscard]] std::vector<TimeKey> getTimeFrameKeys();

    /**
    * @brief Clear all data and reset DataManager to initial state
    *
    * This function removes all loaded data objects, TimeFrame objects (except the default "time" frame),
    * and clears all mappings between data keys and time frame keys. This is useful for batch processing
    * where you want to load different datasets with the same configuration.
    * 
    * The function will:
    * - Remove all data objects from _data
    * - Remove all TimeFrame objects except the default "time" frame
    * - Clear all data-to-timeframe mappings
    * - Remove all TimeFrameV2 objects and mappings
    * - Reset media data to default empty state
    * - Notify observers of the state change
    *
    * @note The default "time" TimeFrame and "media" data key are preserved but reset to empty state
    */
    void reset();


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
        auto it = _data.find(key);
        if (it != _data.end()) {
            if (std::holds_alternative<std::shared_ptr<T>>(it->second)) {
                return std::get<std::shared_ptr<T>>(it->second);
            }
        }
        return nullptr;
    }

    template<typename T>
    void setData(std::string const & key, TimeKey const & time_key) {
        _data[key] = std::make_shared<T>();
        setTimeKey(key, time_key);

        //Rebuild the EntityIds
        if constexpr ((std::is_same_v<T, LineData>) ||
                      (std::is_same_v<T, PointData>) ||
                      (std::is_same_v<T, DigitalEventSeries>) ||
                      (std::is_same_v<T, DigitalIntervalSeries>) ||
                      (std::is_same_v<T, MaskData>) ) {
            std::get<std::shared_ptr<T>>(_data[key])->setIdentityContext(key, getEntityRegistry());
            std::get<std::shared_ptr<T>>(_data[key])->rebuildAllEntityIds();
        }
        _notifyObservers();
    }

    void setData(std::string const & key, DataTypeVariant data, TimeKey const & time_key);

    template<typename T>
    void setData(std::string const & key, std::shared_ptr<T> data, TimeKey const & time_key) {
        // Loop through all _data. If shared_ptr data is already in _data, return
        for (auto const & [existing_key, existing_variant]: _data) {
            bool found = std::visit([&data](auto const & existing_ptr) -> bool {
                using ExistingType = std::decay_t<decltype(*existing_ptr)>;
                using NewType = std::decay_t<decltype(*data)>;
                if constexpr (std::is_same_v<ExistingType, NewType>) {
                    return existing_ptr == data;
                } else {
                    return false;
                }
            },
                                    existing_variant);
            if (found) {
                std::cerr << "Data with key '" << key
                          << "' already exists, not setting again." << std::endl;
                return;// Data already exists, do not set again
            }
        }

        _data[key] = data;
        setTimeKey(key, time_key);

        if constexpr ((std::is_same_v<T, LineData>) ||
                      (std::is_same_v<T, PointData>) ||
                      (std::is_same_v<T, DigitalEventSeries>) ||
                      (std::is_same_v<T, DigitalIntervalSeries>) ||
                      (std::is_same_v<T, MaskData>) ) {
            std::get<std::shared_ptr<T>>(_data[key])->setIdentityContext(key, getEntityRegistry());
            std::get<std::shared_ptr<T>>(_data[key])->rebuildAllEntityIds();
        }

        _notifyObservers();
    }

    /**
     * @brief Delete data associated with the specified key
     * 
     * Removes the data object and its associated time frame mapping from the DataManager.
     * All observers are notified of the change, allowing dependent widgets to clean up.
     * 
     * @param key The key of the data to delete
     * @return bool True if the data was successfully deleted, false if the key doesn't exist
     * 
     * @note This method will:
     *       - Remove the data from the internal storage
     *       - Remove the time frame mapping for this data
     *       - Notify all observers of the change
     *       - The shared_ptr will be automatically cleaned up when no other references exist
     */
    bool deleteData(std::string const & key);

    [[nodiscard]] DM_DataType getType(std::string const & key) const;

    void setOutputPath(std::string const & output_path) { _output_path = output_path; };

    [[nodiscard]] std::string const & getOutputPath() const {
        return _output_path;
    }

    void notifyTableObservers(TableEvent const & ev);

    /**
     * @brief Access the session-scoped EntityRegistry.
     */
    [[nodiscard]] EntityRegistry * getEntityRegistry() const { return _entity_registry.get(); }

    /**
     * @brief Access the session-scoped EntityGroupManager.
     */
    [[nodiscard]] EntityGroupManager * getEntityGroupManager() const { return _entity_group_manager.get(); }

private:
    std::unordered_map<TimeKey, std::shared_ptr<TimeFrame>> _times;

    std::vector<ObserverCallback> _observers;

    std::unordered_map<std::string, DataTypeVariant>
            _data;

    std::unordered_map<std::string, TimeKey> _time_frames;

    std::string _output_path;

    void _notifyObservers();

    int64_t _current_time{0};

    // ======= Table Registry and observer internals =======
    std::unique_ptr<TableRegistry> _table_registry;
    std::unordered_map<int, TableObserver> _table_observers;
    int _next_table_observer_id{1};

    // ======= Identity / Entity registry =======
    std::unique_ptr<EntityRegistry> _entity_registry;
    std::unique_ptr<EntityGroupManager> _entity_group_manager;
};

/**
 * @brief Progress callback function type for JSON config loading
 * @param current Current item index (0-based)
 * @param total Total number of items to load
 * @param message Description of current operation (e.g., "Loading points from file.json")
 * @return true to continue loading, false to cancel
 */
using JsonLoadProgressCallback = std::function<bool(int current, int total, std::string const & message)>;

std::vector<DataInfo> load_data_from_json_config(DataManager *, std::string const & json_filepath);
std::vector<DataInfo> load_data_from_json_config(DataManager *, std::string const & json_filepath, JsonLoadProgressCallback progress_callback);
std::vector<DataInfo> load_data_from_json_config(DataManager * dm, nlohmann::json const & j, std::string const & base_path);
std::vector<DataInfo> load_data_from_json_config(DataManager * dm, nlohmann::json const & j, std::string const & base_path, JsonLoadProgressCallback progress_callback);

std::string convert_data_type_to_string(DM_DataType type);


extern template std::shared_ptr<AnalogTimeSeries> DataManager::getData<AnalogTimeSeries>(std::string const & key);
extern template void DataManager::setData<AnalogTimeSeries>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<AnalogTimeSeries>(std::string const & key, std::shared_ptr<AnalogTimeSeries> data, TimeKey const & time_key);

extern template std::shared_ptr<DigitalEventSeries> DataManager::getData<DigitalEventSeries>(std::string const & key);
extern template void DataManager::setData<DigitalEventSeries>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<DigitalEventSeries>(std::string const & key, std::shared_ptr<DigitalEventSeries> data, TimeKey const & time_key);

extern template std::shared_ptr<DigitalIntervalSeries> DataManager::getData<DigitalIntervalSeries>(std::string const & key);
extern template void DataManager::setData<DigitalIntervalSeries>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<DigitalIntervalSeries>(std::string const & key, std::shared_ptr<DigitalIntervalSeries> data, TimeKey const & time_key);

extern template std::shared_ptr<LineData> DataManager::getData<LineData>(std::string const & key);
extern template void DataManager::setData<LineData>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<LineData>(std::string const & key, std::shared_ptr<LineData> data, TimeKey const & time_key);

extern template std::shared_ptr<MaskData> DataManager::getData<MaskData>(std::string const & key);
extern template void DataManager::setData<MaskData>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<MaskData>(std::string const & key, std::shared_ptr<MaskData> data, TimeKey const & time_key);

extern template std::shared_ptr<MediaData> DataManager::getData<MediaData>(std::string const & key);

extern template std::shared_ptr<PointData> DataManager::getData<PointData>(std::string const & key);
extern template void DataManager::setData<PointData>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<PointData>(std::string const & key, std::shared_ptr<PointData> data, TimeKey const & time_key);

extern template std::shared_ptr<TensorData> DataManager::getData<TensorData>(std::string const & key);
extern template void DataManager::setData<TensorData>(std::string const & key, TimeKey const & time_key);
extern template void DataManager::setData<TensorData>(std::string const & key, std::shared_ptr<TensorData> data, TimeKey const & time_key);


#endif// DATAMANAGER_HPP
