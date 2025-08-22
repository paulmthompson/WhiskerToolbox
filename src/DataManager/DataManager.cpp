#include "DataManager.hpp"

#include "Media/MediaDataFactory.hpp"
#include "ConcreteDataFactory.hpp"
#include "IO/LoaderRegistry.hpp"
#include "IO/LoaderRegistration.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/Tensor_Data.hpp"

// Media includes - now from separate MediaData library
//#include "Media/Image_Data.hpp"
#include "Media/Media_Data.hpp"
//#include "Media/Video_Data.hpp"

#include "AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.hpp"
#include "DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.hpp"
#include "Lines/IO/JSON/Line_Data_JSON.hpp"
#include "Masks/IO/JSON/Mask_Data_JSON.hpp"
#ifdef ENABLE_OPENCV
#include "Media/IO/JSON/Image_Data_JSON.hpp"
#endif
#include "Points/IO/JSON/Point_Data_JSON.hpp"
#include "Tensors/IO/numpy/Tensor_Data_numpy.hpp"
#include "utils/TableView/TableRegistry.hpp"

// Plugin system includes
#include "IO/PluginLoader.hpp"

#include "loaders/binary_loaders.hpp"
#include "transforms/Masks/mask_area.hpp"

#include "TimeFrame/TimeFrame.hpp"

#include "nlohmann/json.hpp"

#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"
#include "utils/string_manip.hpp"

#include "Entity/EntityRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>

using namespace nlohmann;

/**
 * @brief Try loading data using new registry system first, fallback to legacy if needed
 */
bool tryRegistryThenLegacyLoad(
    DataManager* dm,
    std::string const& file_path,
    DM_DataType data_type,
    nlohmann::json const& item,
    std::string const& name,
    std::vector<DataInfo>& data_info_list,
    DataFactory* factory
) {
    // Extract format if available
    if (item.contains("format")) {
        std::string const format = item["format"];
        
        // Try registry system first
        LoaderRegistry& registry = LoaderRegistry::getInstance();
        if (registry.isFormatSupported(format, toIODataType(data_type))) {
            std::cout << "Using registry loader for " << name << " (format: " << format << ")" << std::endl;

            LoadResult result = registry.tryLoad(format, toIODataType(data_type), file_path, item, factory);
            if (result.success) {
                // Handle data setting and post-loading setup based on data type
                switch (data_type) {
                    case DM_DataType::Line: {
                        // Set the LineData in DataManager
                        if (std::holds_alternative<std::shared_ptr<LineData>>(result.data)) {
                            auto line_data = std::get<std::shared_ptr<LineData>>(result.data);
                            
                            // Set up identity context
                            if (line_data) {
                                line_data->setIdentityContext(name, dm->getEntityRegistry());
                                line_data->rebuildAllEntityIds();
                            }
                            
                            dm->setData<LineData>(name, line_data, TimeKey("time"));
                            
                            std::string const color = item.value("color", "0000FF");
                            data_info_list.push_back({name, "LineData", color});
                        }
                        break;
                    }
                    case DM_DataType::Mask: {
                        // Set the MaskData in DataManager
                        if (std::holds_alternative<std::shared_ptr<MaskData>>(result.data)) {
                            auto mask_data = std::get<std::shared_ptr<MaskData>>(result.data);
                            
                            dm->setData<MaskData>(name, mask_data, TimeKey("time"));
                            
                            std::string const color = item.value("color", "0000FF");
                            data_info_list.push_back({name, "MaskData", color});
                            
                            // Handle operations if present (same as legacy)
                            if (item.contains("operations")) {
                                for (auto const & operation: item["operations"]) {
                                    std::string const operation_type = operation["type"];
                                    if (operation_type == "area") {
                                        std::cout << "Calculating area for mask: " << name << std::endl;
                                        auto area_data = area(dm->getData<MaskData>(name).get());
                                        std::string const output_name = name + "_area";
                                        dm->setData<AnalogTimeSeries>(output_name, area_data, TimeKey("time"));
                                    }
                                }
                            }
                        }
                        break;
                    }
                    // Add other data types as they get plugin support...
                    default:
                        std::cerr << "Registry loaded unsupported data type: " << static_cast<int>(data_type) << std::endl;
                        return false;
                }
                
                return true;
            } else {
                std::cout << "Registry loading failed for " << name << ": " << result.error_message 
                         << ", falling back to legacy loader" << std::endl;
            }
        }
    }
    
    return false; // Indicates we should use legacy loading
}

DataManager::DataManager() {
    _times[TimeKey("time")] = std::make_shared<TimeFrame>();
    _data["media"] = std::make_shared<EmptyMediaData>();

    setTimeKey("media", TimeKey("time"));
    _output_path = std::filesystem::current_path();

    // Initialize TableRegistry
    _table_registry = std::make_unique<TableRegistry>(*this);

    // Initialize EntityRegistry
    _entity_registry = std::make_unique<EntityRegistry>();

    // Register all available loaders
    static bool loaders_registered = false;
    if (!loaders_registered) {
        registerAllLoaders();
        loaders_registered = true;
    }
}

DataManager::~DataManager() = default;

void DataManager::reset() {
    std::cout << "DataManager: Resetting to initial state..." << std::endl;
    
    // Clear all data objects except media (which we'll reset)
    _data.clear();
    
    // Reset media to a fresh empty MediaData object
    _data["media"] = std::make_shared<EmptyMediaData>();
    
    // Clear all TimeFrame objects except the default "time" frame
    _times.clear();
    
    // Recreate the default "time" TimeFrame
    _times[TimeKey("time")] = std::make_shared<TimeFrame>();
    
    // Clear all data-to-timeframe mappings and recreate the default media mapping
    _time_frames.clear();
    setTimeKey("media", TimeKey("time"));

    
    // Reset current time
    _current_time = 0;
    
    // Notify observers that the state has changed
    _notifyObservers();
    
    std::cout << "DataManager: Reset complete. Default 'time' frame and 'media' data restored." << std::endl;

    // Reset entity registry for a new session context
    if (_entity_registry) {
         _entity_registry->clear();
    }
}

bool DataManager::setTime(TimeKey const & key, std::shared_ptr<TimeFrame> timeframe, bool overwrite) {

    if (!timeframe) {
        std::cerr << "Error: Cannot register a nullptr TimeFrame for key: " << key << std::endl;
        return false;
    }

    if (_times.find(key) != _times.end()) {
        if (!overwrite) {
            std::cerr << "Error: Time key already exists in DataManager: " << key << std::endl;
            return false;
        }
    }

    _times[key] = std::move(timeframe);

    // Move ptr to new time frame to all data that hold 
    for (auto const & [data_key, data] : _data) {
        if (_time_frames.find(data_key) != _time_frames.end()) {
            auto time_key = _time_frames[data_key];
            if (time_key == key) {
                std::visit([this, timeframe](auto & x) {
                    x->setTimeFrame(timeframe);
                },
                           data);
            }
        }
    }

    return true;
}

std::shared_ptr<TimeFrame> DataManager::getTime() {
    return _times[TimeKey("time")];
};

std::shared_ptr<TimeFrame> DataManager::getTime(TimeKey const & key) {
    if (_times.find(key) != _times.end()) {
        return _times[key];
    }
    return nullptr;
};

TimeIndexAndFrame DataManager::getCurrentIndexAndFrame(TimeKey const & key) {
    if (_times.find(key) != _times.end()) {
        return {TimeFrameIndex(_current_time), _times[key]};
    }
    return {TimeFrameIndex(_current_time), nullptr};
}

bool DataManager::removeTime(TimeKey const & key) {
    if (_times.find(key) == _times.end()) {
        std::cerr << "Error: could not find time key in DataManager: " << key << std::endl;
        return false;
    }

    auto it = _times.find(key);
    _times.erase(it);
    return true;
}

bool DataManager::setTimeKey(std::string const & data_key, TimeKey const & time_key) {
    if (_data.find(data_key) == _data.end()) {
        std::cerr << "Error: Data key not found in DataManager: " << data_key << std::endl;
        return false;
    }

    if (_times.find(time_key) == _times.end()) {
        std::cerr << "Error: Time key not found in DataManager: " << time_key << std::endl;
        return false;
    }

    _time_frames[data_key] = time_key;

    if (_data.find(data_key) != _data.end()) {
        auto data = _data[data_key];
        std::visit([this, time_key](auto & x) {
            x->setTimeFrame(this->_times[time_key]);
        },
                   data);
    }
    return true;
}

TimeKey DataManager::getTimeKey(std::string const & data_key) {
    // check if data_key exists
    if (_data.find(data_key) == _data.end()) {
        std::cerr << "Error: Data key not found in DataManager: " << data_key << std::endl;
        return TimeKey("");
    }

    // check if data key has time frame
    if (_time_frames.find(data_key) == _time_frames.end()) {
        std::cerr << "Error: Data key "
                  << data_key
                  << " exists, but not assigned to a TimeFrame" << std::endl;
        return TimeKey("");
    }

    return _time_frames[data_key];
}

std::vector<TimeKey> DataManager::getTimeFrameKeys() {
    std::vector<TimeKey> keys;
    keys.reserve(_times.size());
    for (auto const & [key, value]: _times) {

        keys.push_back(key);
    }
    return keys;
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

bool DataManager::removeCallbackFromData(std::string const & key, int callback_id) {
    if (_data.find(key) != _data.end()) {
        auto data = _data[key];

        std::visit([callback_id](auto & x) {
            x.get()->removeObserver(callback_id);
        },
                   data);

        return true;
    }

    return false;
}

void DataManager::addObserver(ObserverCallback callback) {
    _observers.push_back(std::move(callback));
}

void DataManager::_notifyObservers() {
    for (auto & observer: _observers) {
        observer();
    }
}

// ===== Table Registry accessors =====
TableRegistry * DataManager::getTableRegistry() {
    return _table_registry.get();
}

TableRegistry const * DataManager::getTableRegistry() const {
    return _table_registry.get();
}

// ===== Table observer channel =====
int DataManager::addTableObserver(TableObserver callback) {
    if (!callback) return -1;
    int id = _next_table_observer_id++;
    _table_observers[id] = std::move(callback);
    return id;
}

bool DataManager::removeTableObserver(int callback_id) {
    return _table_observers.erase(callback_id) > 0;
}

void DataManager::notifyTableObservers(TableEvent const & ev) {
    for (auto const & [id, cb] : _table_observers) {
        (void)id;
        cb(ev);
    }
}

// Provide C-style bridge for TableRegistry to call
void DataManager__NotifyTableObservers(DataManager & dm, TableEvent const & ev) {
    dm.notifyTableObservers(ev);
}

std::vector<std::string> DataManager::getAllKeys() {
    std::vector<std::string> keys;
    keys.reserve(_data.size());
    for (auto const & [key, value]: _data) {

        keys.push_back(key);
    }
    return keys;
}

std::optional<DataTypeVariant> DataManager::getDataVariant(std::string const & key) {
    if (_data.find(key) != _data.end()) {
        return _data[key];
    }
    return std::nullopt;
}

void DataManager::setData(std::string const & key, DataTypeVariant data, TimeKey const & time_key) {
    _data[key] = data;
    setTimeKey(key, time_key);
    _notifyObservers();
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

void checkOptionalFields(json const & item, std::vector<std::string> const & optionalFields) {
    for (auto const & field: optionalFields) {
        if (!item.contains(field)) {
            std::cout << "Warning: Optional field \"" << field << "\" is missing in JSON item." << std::endl;
        }
    }
}

DM_DataType stringToDataType(std::string const & data_type_str) {
    if (data_type_str == "video") return DM_DataType::Video;
    if (data_type_str == "images") return DM_DataType::Images;
    if (data_type_str == "points") return DM_DataType::Points;
    if (data_type_str == "mask") return DM_DataType::Mask;
    if (data_type_str == "line") return DM_DataType::Line;
    if (data_type_str == "analog") return DM_DataType::Analog;
    if (data_type_str == "digital_event") return DM_DataType::DigitalEvent;
    if (data_type_str == "digital_interval") return DM_DataType::DigitalInterval;
    if (data_type_str == "tensor") return DM_DataType::Tensor;
    if (data_type_str == "time") return DM_DataType::Time;
    return DM_DataType::Unknown;
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

    // Create factory for plugin system
    ConcreteDataFactory factory;

    // Iterate through JSON array
    for (auto const & item: j) {

        // Skip transformation objects - they will be processed separately
        if (item.contains("transformations")) {
            continue;
        }

        if (!checkRequiredFields(item, {"data_type", "name", "filepath"})) {
            continue;// Exit if any required field is missing
        }

        std::string const data_type_str = item["data_type"];
        auto const data_type = stringToDataType(data_type_str);
        if (data_type == DM_DataType::Unknown) {
            std::cout << "Unknown data type: " << data_type_str << std::endl;
            continue;
        }

        std::string const name = item["name"];

        auto file_exists = processFilePath(item["filepath"], base_path);
        if (!file_exists) {
            std::cerr << "File does not exist: " << item["filepath"] << std::endl;
            continue;
        }

        std::string const file_path = file_exists.value();

        switch (data_type) {
            case DM_DataType::Video: {
                auto media_data = MediaDataFactory::loadMediaData(data_type, file_path, item);
                if (media_data) {
                    dm->setData<MediaData>("media", media_data, TimeKey("time"));
                    data_info_list.push_back({name, "VideoData", ""});
                } else {
                    std::cerr << "Failed to load video data: " << file_path << std::endl;
                }
                break;
            }
#ifdef ENABLE_OPENCV
            case DM_DataType::Images: {
                auto media_data = MediaDataFactory::loadMediaData(data_type, file_path, item);
                if (media_data) {
                    dm->setData<MediaData>("media", media_data, TimeKey("time"));
                    data_info_list.push_back({name, "ImageData", ""});
                } else {
                    std::cerr << "Failed to load image data: " << file_path << std::endl;
                }
                break;
            }
#endif
            case DM_DataType::Points: {

                auto point_data = load_into_PointData(file_path, item);

                // Attach identity context and generate EntityIds
                if (point_data) {
                    point_data->setIdentityContext(name, dm->getEntityRegistry());
                    point_data->rebuildAllEntityIds();
                }

                dm->setData<PointData>(name, point_data, TimeKey("time"));

                std::string const color = item.value("color", "#0000FF");
                data_info_list.push_back({name, "PointData", color});
                break;
            }
            case DM_DataType::Mask: {

                // Try registry system first, then fallback to legacy
                if (tryRegistryThenLegacyLoad(dm, file_path, data_type, item, name, data_info_list, &factory)) {
                    break; // Successfully loaded with plugin
                }

                // Legacy loading fallback
                auto mask_data = load_into_MaskData(file_path, item);

                std::string const color = item.value("color", "0000FF");
                dm->setData<MaskData>(name, mask_data, TimeKey("time"));

                data_info_list.push_back({name, "MaskData", color});

                if (item.contains("operations")) {

                    for (auto const & operation: item["operations"]) {

                        std::string const operation_type = operation["type"];

                        if (operation_type == "area") {
                            std::cout << "Calculating area for mask: " << name << std::endl;
                            auto area_data = area(dm->getData<MaskData>(name).get());
                            std::string const output_name = name + "_area";
                            dm->setData<AnalogTimeSeries>(output_name, area_data, TimeKey("time"));
                        }
                    }
                }
                break;
            }
            case DM_DataType::Line: {
                
                // Try registry system first, then fallback to legacy
                if (tryRegistryThenLegacyLoad(dm, file_path, data_type, item, name, data_info_list, &factory)) {
                    break; // Successfully loaded with plugin
                }
                
                // Legacy loading fallback
                auto line_data = load_into_LineData(file_path, item);

                // Attach identity context and generate EntityIds
                if (line_data) {
                    line_data->setIdentityContext(name, dm->getEntityRegistry());
                    line_data->rebuildAllEntityIds();
                }

                dm->setData<LineData>(name, line_data, TimeKey("time"));

                std::string const color = item.value("color", "0000FF");

                data_info_list.push_back({name, "LineData", color});

                break;
            }
            case DM_DataType::Analog: {

                auto analog_time_series = load_into_AnalogTimeSeries(file_path, item);

                for (size_t channel = 0; channel < analog_time_series.size(); channel++) {
                    std::string const channel_name = name + "_" + std::to_string(channel);

                    dm->setData<AnalogTimeSeries>(channel_name, analog_time_series[channel], TimeKey("time"));

                    if (item.contains("clock")) {
                        std::string const clock_str = item["clock"];
                        auto const clock = TimeKey(clock_str);
                        dm->setTimeKey(channel_name, clock);
                    }
                }
                break;
            }
            case DM_DataType::DigitalEvent: {

                auto digital_event_series = load_into_DigitalEventSeries(file_path, item);

                for (size_t channel = 0; channel < digital_event_series.size(); channel++) {
                    std::string const channel_name = name + "_" + std::to_string(channel);

                    // Attach identity context and generate EntityIds
                    if (digital_event_series[channel]) {
                        digital_event_series[channel]->setIdentityContext(channel_name, dm->getEntityRegistry());
                        digital_event_series[channel]->rebuildAllEntityIds();
                    }

                    dm->setData<DigitalEventSeries>(channel_name, digital_event_series[channel], TimeKey("time"));

                    if (item.contains("clock")) {
                        std::string const clock_str = item["clock"];
                        auto const clock = TimeKey(clock_str);
                        dm->setTimeKey(channel_name, clock);
                    }
                }
                break;
            }
            case DM_DataType::DigitalInterval: {

                auto digital_interval_series = load_into_DigitalIntervalSeries(file_path, item);
                if (digital_interval_series) {
                    digital_interval_series->setIdentityContext(name, dm->getEntityRegistry());
                    digital_interval_series->rebuildAllEntityIds();
                }
                dm->setData<DigitalIntervalSeries>(name, digital_interval_series, TimeKey("time"));

                break;
            }
            case DM_DataType::Tensor: {

                if (item["format"] == "numpy") {

                    TensorData tensor_data;
                    loadNpyToTensorData(file_path, tensor_data);

                    dm->setData<TensorData>(name, std::make_shared<TensorData>(tensor_data), TimeKey("time"));

                } else {
                    std::cout << "Format " << item["format"] << " not found for " << name << std::endl;
                }
                break;
            }
            case DM_DataType::Time: {

                if (item["format"] == "uint16") {

                    int const channel = item["channel"];
                    std::string const transition = item["transition"];

                    int const header_size = item.value("header_size", 0);

                    auto opts = Loader::BinaryAnalogOptions{.file_path = file_path,
                                                            .header_size_bytes = static_cast<size_t>(header_size)};
                    auto data = Loader::readBinaryFile<uint16_t>(opts);

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
                    dm->setTime(TimeKey(name), timeframe, true);
                }

                if (item["format"] == "uint16_length") {

                    int const header_size = item.value("header_size", 0);

                    auto opts = Loader::BinaryAnalogOptions{.file_path = file_path,
                                                            .header_size_bytes = static_cast<size_t>(header_size)};
                    auto data = Loader::readBinaryFile<uint16_t>(opts);

                    std::vector<int> t(data.size());
                    std::iota(std::begin(t), std::end(t), 0);

                    std::cout << "Total of " << t.size() << " timestamps for " << name << std::endl;

                    auto timeframe = std::make_shared<TimeFrame>(t);
                    dm->setTime(TimeKey(name), timeframe, true);
                }

                if (item["format"] == "filename") {

                    // Get required parameters
                    std::string const folder_path = file_path;// file path is required argument
                    std::string const regex_pattern = item["regex_pattern"];

                    // Get optional parameters with defaults
                    std::string const file_extension = item.value("file_extension", "");
                    std::string const mode_str = item.value("mode", "found_values");
                    bool const sort_ascending = item.value("sort_ascending", true);

                    // Convert mode string to enum
                    FilenameTimeFrameMode mode = FilenameTimeFrameMode::FOUND_VALUES;
                    if (mode_str == "zero_to_max") {
                        mode = FilenameTimeFrameMode::ZERO_TO_MAX;
                    } else if (mode_str == "min_to_max") {
                        mode = FilenameTimeFrameMode::MIN_TO_MAX;
                    }

                    // Create options
                    FilenameTimeFrameOptions options;
                    options.folder_path = folder_path;
                    options.file_extension = file_extension;
                    options.regex_pattern = regex_pattern;
                    options.mode = mode;
                    options.sort_ascending = sort_ascending;

                    // Create TimeFrame from filenames
                    auto timeframe = createTimeFrameFromFilenames(options);
                    if (timeframe) {
                        dm->setTime(TimeKey(name), timeframe, true);
                        std::cout << "Created TimeFrame '" << name << "' from filenames in "
                                  << folder_path << std::endl;
                    } else {
                        std::cerr << "Error: Failed to create TimeFrame from filenames for "
                                  << name << std::endl;
                    }
                }
                break;
            }
            default:
                std::cout << "Unsupported data type: " << data_type_str << std::endl;
                continue;
        }
        if (item.contains("clock")) {
            std::string clock_str = item["clock"];
            auto clock = TimeKey(clock_str);
            std::cout << "Setting time for " << name << " to " << clock << std::endl;
            dm->setTimeKey(name, clock);
        }
    }

    // Process all transformation objects found in the JSON array
    for (auto const & item: j) {
        if (item.contains("transformations")) {
            std::cout << "Found transformations section, executing pipeline..." << std::endl;
            
            try {
                // Create registry and pipeline with proper constructors
                auto registry = std::make_unique<TransformRegistry>();
                TransformPipeline pipeline(dm, registry.get());
                
                // Load the pipeline configuration from JSON
                if (!pipeline.loadFromJson(item["transformations"])) {
                    std::cerr << "Failed to load pipeline configuration from JSON" << std::endl;
                    continue;
                }
                
                // Execute the pipeline with a progress callback
                auto result = pipeline.execute([](int step_index, const std::string& step_name, int step_progress, int overall_progress) {
                    std::cout << "Step " << step_index << " ('" << step_name << "'): " 
                              << step_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
                });
                
                if (result.success) {
                    std::cout << "Pipeline executed successfully!" << std::endl;
                    std::cout << "Steps completed: " << result.steps_completed << "/" << result.total_steps << std::endl;
                    std::cout << "Total execution time: " << result.total_execution_time_ms << " ms" << std::endl;
                } else {
                    std::cerr << "Pipeline execution failed: " << result.error_message << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Exception during pipeline execution: " << e.what() << std::endl;
            }
        }
    }

    return data_info_list;
}

DM_DataType DataManager::getType(std::string const & key) const {
    auto it = _data.find(key);
    if (it != _data.end()) {
        if (std::holds_alternative<std::shared_ptr<MediaData>>(it->second)) {
            auto media_data = std::get<std::shared_ptr<MediaData>>(it->second);
            switch (media_data->getMediaType()) {
                case MediaData::MediaType::Video:
                    return DM_DataType::Video;
                case MediaData::MediaType::Images:
                    return DM_DataType::Images;
                case MediaData::MediaType::HDF5:
                    // For HDF5, we might need additional logic to determine if it's video or images
                    // For now, defaulting to Video (old behavior)
                    return DM_DataType::Video;
                default:
                    return DM_DataType::Video; // Old behavior for unknown types
            }
        } else if (std::holds_alternative<std::shared_ptr<PointData>>(it->second)) {
            return DM_DataType::Points;
        } else if (std::holds_alternative<std::shared_ptr<LineData>>(it->second)) {
            return DM_DataType::Line;
        } else if (std::holds_alternative<std::shared_ptr<MaskData>>(it->second)) {
            return DM_DataType::Mask;
        } else if (std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(it->second)) {
            return DM_DataType::Analog;
        } else if (std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(it->second)) {
            return DM_DataType::DigitalEvent;
        } else if (std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(it->second)) {
            return DM_DataType::DigitalInterval;
        } else if (std::holds_alternative<std::shared_ptr<TensorData>>(it->second)) {
            return DM_DataType::Tensor;
        }
        return DM_DataType::Unknown;
    }
    return DM_DataType::Unknown;
}

std::string convert_data_type_to_string(DM_DataType type) {
    switch (type) {
        case DM_DataType::Video:
            return "video";
        case DM_DataType::Images:
            return "images";
        case DM_DataType::Points:
            return "points";
        case DM_DataType::Mask:
            return "mask";
        case DM_DataType::Line:
            return "line";
        case DM_DataType::Analog:
            return "analog";
        case DM_DataType::DigitalEvent:
            return "digital_event";
        case DM_DataType::DigitalInterval:
            return "digital_interval";
        case DM_DataType::Tensor:
            return "tensor";
        case DM_DataType::Time:
            return "time";
        default:
            return "unknown";
    }
}

