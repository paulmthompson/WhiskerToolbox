#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Image_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/Tensor_Data.hpp"

#include "AnalogTimeSeries/IO/JSON/Analog_Time_Series_JSON.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DigitalTimeSeries/IO/JSON/Digital_Event_Series_JSON.hpp"
#include "DigitalTimeSeries/IO/JSON/Digital_Interval_Series_JSON.hpp"
#include "Lines/IO/JSON/Line_Data_JSON.hpp"
#include "Masks/IO/JSON/Mask_Data_JSON.hpp"
#include "Media/Video_Data_Loader.hpp"
#include "Points/IO/JSON/Point_Data_JSON.hpp"

#include "loaders/binary_loaders.hpp"
#include "transforms/Masks/mask_area.hpp"

#include "TimeFrame.hpp"
#include "TimeFrame/TimeFrameV2.hpp"

#include "nlohmann/json.hpp"
#include "utils/string_manip.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>

using namespace nlohmann;

DataManager::DataManager() {
    _times["time"] = std::make_shared<TimeFrame>();
    _data["media"] = std::make_shared<MediaData>();

    setTimeFrame("media", "time");
    _output_path = std::filesystem::current_path();
}

bool DataManager::setTime(std::string const & key, std::shared_ptr<TimeFrame> timeframe, bool overwrite) {

    if (!timeframe) {
        std::cerr << "Error: Cannot register a nullptr TimeFrame for key: " << key << std::endl;
        return false;
    }

    if (_times.find(key) != _times.end()) {
        if (overwrite) {
            _times[key] = std::move(timeframe);
            return true;
        } else {
            std::cerr << "Error: Time key already exists in DataManager: " << key << std::endl;
            return false;
        }
    }

    _times[key] = std::move(timeframe);
    return true;
}

std::shared_ptr<TimeFrame> DataManager::getTime() {
    return _times["time"];
};

std::shared_ptr<TimeFrame> DataManager::getTime(std::string const & key) {
    if (_times.find(key) != _times.end()) {
        return _times[key];
    }
    return nullptr;
};

bool DataManager::removeTime(std::string const & key) {
    if (_times.find(key) == _times.end()) {
        std::cerr << "Error: could not find time key in DataManager: " << key << std::endl;
        return false;
    }

    auto it = _times.find(key);
    _times.erase(it);
    return true;
}

bool DataManager::setTimeFrame(std::string const & data_key, std::string const & time_key) {
    if (_data.find(data_key) == _data.end()) {
        std::cerr << "Error: Data key not found in DataManager: " << data_key << std::endl;
        return false;
    }

    if (_times.find(time_key) == _times.end()) {
        std::cerr << "Error: Time key not found in DataManager: " << time_key << std::endl;
        return false;
    }

    _time_frames[data_key] = time_key;
    return true;
}

std::string DataManager::getTimeFrame(std::string const & data_key) {
    // check if data_key exists
    if (_data.find(data_key) == _data.end()) {
        std::cerr << "Error: Data key not found in DataManager: " << data_key << std::endl;
        return "";
    }

    // check if data key has time frame
    if (_time_frames.find(data_key) == _time_frames.end()) {
        std::cerr << "Error: Data key "
                  << data_key
                  << " exists, but not assigned to a TimeFrame" << std::endl;
        return "";
    }

    return _time_frames[data_key];
}

std::vector<std::string> DataManager::getTimeFrameKeys() {
    std::vector<std::string> keys;
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

void DataManager::setData(std::string const & key, DataTypeVariant data) {
    _data[key] = data;
    setTimeFrame(key, "time");
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

    // Iterate through JSON array
    for (auto const & item: j) {

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

                auto video_data = load_video_into_VideoData(file_path);
                dm->setData<VideoData>("media", video_data);

                data_info_list.push_back({name, "VideoData", ""});
                break;
            }
            case DM_DataType::Images: {

                auto media = std::make_shared<ImageData>();
                media->LoadMedia(file_path);
                dm->setData<ImageData>("media", media);

                data_info_list.push_back({name, "ImageData", ""});
                break;
            }
            case DM_DataType::Points: {

                auto point_data = load_into_PointData(file_path, item);

                dm->setData<PointData>(name, point_data);

                std::string const color = item.value("color", "#0000FF");
                data_info_list.push_back({name, "PointData", color});
                break;
            }
            case DM_DataType::Mask: {

                auto mask_data = load_into_MaskData(file_path, item);

                std::string const color = item.value("color", "0000FF");
                dm->setData<MaskData>(name, mask_data);

                data_info_list.push_back({name, "MaskData", color});

                if (item.contains("operations")) {

                    for (auto const & operation: item["operations"]) {

                        std::string const operation_type = operation["type"];

                        if (operation_type == "area") {
                            std::cout << "Calculating area for mask: " << name << std::endl;
                            auto area_data = area(dm->getData<MaskData>(name).get());
                            std::string const output_name = name + "_area";
                            dm->setData<AnalogTimeSeries>(output_name, area_data);
                        }
                    }
                }
                break;
            }
            case DM_DataType::Line: {

                auto line_data = load_into_LineData(file_path, item);

                dm->setData<LineData>(name, line_data);

                std::string const color = item.value("color", "0000FF");

                data_info_list.push_back({name, "LineData", color});

                break;
            }
            case DM_DataType::Analog: {

                auto analog_time_series = load_into_AnalogTimeSeries(file_path, item);

                for (int channel = 0; channel < analog_time_series.size(); channel++) {
                    std::string const channel_name = name + "_" + std::to_string(channel);

                    dm->setData<AnalogTimeSeries>(channel_name, analog_time_series[channel]);

                    if (item.contains("clock")) {
                        std::string const clock = item["clock"];
                        dm->setTimeFrame(channel_name, clock);
                    }
                }
                break;
            }
            case DM_DataType::DigitalEvent: {

                auto digital_event_series = load_into_DigitalEventSeries(file_path, item);

                for (int channel = 0; channel < digital_event_series.size(); channel++) {
                    std::string const channel_name = name + "_" + std::to_string(channel);

                    dm->setData<DigitalEventSeries>(channel_name, digital_event_series[channel]);

                    if (item.contains("clock")) {
                        std::string const clock = item["clock"];
                        dm->setTimeFrame(channel_name, clock);
                    }
                }
                break;
            }
            case DM_DataType::DigitalInterval: {

                auto digital_interval_series = load_into_DigitalIntervalSeries(file_path, item);
                dm->setData<DigitalIntervalSeries>(name, digital_interval_series);

                break;
            }
            case DM_DataType::Tensor: {

                if (item["format"] == "numpy") {

                    TensorData tensor_data;
                    loadNpyToTensorData(file_path, tensor_data);

                    dm->setData<TensorData>(name, std::make_shared<TensorData>(tensor_data));

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
                    dm->setTime(name, timeframe, true);
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
                    dm->setTime(name, timeframe, true);
                }

                if (item["format"] == "filename") {

                    // Get required parameters
                    std::string const folder_path = file_path; // file path is required argument
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
                        dm->setTime(name, timeframe, true);
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
            std::string const clock = item["clock"];
            std::cout << "Setting time for " << name << " to " << clock << std::endl;
            dm->setTimeFrame(name, clock);
        }
    }

    return data_info_list;
}

DM_DataType DataManager::getType(std::string const & key) const {
    auto it = _data.find(key);
    if (it != _data.end()) {
        if (std::holds_alternative<std::shared_ptr<MediaData>>(it->second)) {
            //Dynamic cast to videodata or image data

            auto media_data = std::get<std::shared_ptr<MediaData>>(it->second);
            if (dynamic_cast<VideoData *>(media_data.get()) != nullptr) {
                return DM_DataType::Video;
            } else if (dynamic_cast<ImageData *>(media_data.get()) != nullptr) {
                return DM_DataType::Images;
            } else {
                return DM_DataType::Video; //Old behavior
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

// ========== TimeFrameV2 Implementation ==========

bool DataManager::setTimeV2(std::string const & key, AnyTimeFrame timeframe, bool overwrite) {
    if (_times_v2.find(key) != _times_v2.end()) {
        if (overwrite) {
            _times_v2[key] = std::move(timeframe);
            return true;
        } else {
            std::cerr << "Error: TimeFrameV2 key already exists in DataManager: " << key << std::endl;
            return false;
        }
    }

    _times_v2[key] = std::move(timeframe);
    return true;
}

std::optional<AnyTimeFrame> DataManager::getTimeV2(std::string const & key) {
    if (_times_v2.find(key) != _times_v2.end()) {
        return _times_v2[key];
    }
    return std::nullopt;
}

bool DataManager::removeTimeV2(std::string const & key) {
    if (_times_v2.find(key) == _times_v2.end()) {
        std::cerr << "Error: could not find TimeFrameV2 key in DataManager: " << key << std::endl;
        return false;
    }

    auto it = _times_v2.find(key);
    _times_v2.erase(it);
    return true;
}

std::vector<std::string> DataManager::getTimeFrameV2Keys() {
    std::vector<std::string> keys;
    keys.reserve(_times_v2.size());
    for (auto const & [key, value]: _times_v2) {
        keys.push_back(key);
    }
    return keys;
}

bool DataManager::createClockTimeFrame(std::string const & key, int64_t start_tick,
                                       int64_t num_samples, double sampling_rate_hz,
                                       bool overwrite) {
    auto clock_frame = TimeFrameUtils::createDenseClockTimeFrame(start_tick, num_samples, sampling_rate_hz);
    AnyTimeFrame any_frame = clock_frame;
    return setTimeV2(key, std::move(any_frame), overwrite);
}

bool DataManager::createCameraTimeFrame(std::string const & key, std::vector<int64_t> frame_indices,
                                        bool overwrite) {
    auto camera_frame = TimeFrameUtils::createSparseCameraTimeFrame(std::move(frame_indices));
    AnyTimeFrame any_frame = camera_frame;
    return setTimeV2(key, std::move(any_frame), overwrite);
}

bool DataManager::createDenseCameraTimeFrame(std::string const & key, int64_t start_frame,
                                             int64_t num_frames, bool overwrite) {
    auto camera_frame = TimeFrameUtils::createDenseCameraTimeFrame(start_frame, num_frames);
    AnyTimeFrame any_frame = camera_frame;
    return setTimeV2(key, std::move(any_frame), overwrite);
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

// ========== Enhanced AnalogTimeSeries Support Implementation ==========

bool DataManager::createAnalogTimeSeriesWithClock(std::string const & data_key,
                                                  std::string const & timeframe_key,
                                                  std::vector<float> analog_data,
                                                  int64_t start_tick,
                                                  double sampling_rate_hz,
                                                  bool overwrite) {
    // Create the clock timeframe
    if (!createClockTimeFrame(timeframe_key, start_tick,
                              static_cast<int64_t>(analog_data.size()),
                              sampling_rate_hz, overwrite)) {
        return false;
    }

    // Get the timeframe
    auto timeframe_opt = getTimeV2(timeframe_key);
    if (!timeframe_opt.has_value()) {
        return false;
    }

    // Create time vector with the actual tick values
    std::vector<TimeFrameIndex> time_vector;
    time_vector.reserve(analog_data.size());
    for (size_t i = 0; i < analog_data.size(); ++i) {
        time_vector.push_back(TimeFrameIndex(start_tick + static_cast<int64_t>(i)));
    }

    // Create the analog series
    auto series = std::make_shared<AnalogTimeSeries>(std::move(analog_data), std::move(time_vector));
    setDataV2(data_key, series, timeframe_opt.value(), timeframe_key);
    return true;
}

bool DataManager::createAnalogTimeSeriesWithCamera(std::string const & data_key,
                                                   std::string const & timeframe_key,
                                                   std::vector<float> analog_data,
                                                   std::vector<int64_t> frame_indices,
                                                   bool overwrite) {
    if (analog_data.size() != frame_indices.size()) {
        std::cerr << "Error: analog data and frame indices must have same size" << std::endl;
        return false;
    }

    // Save frame_indices before they're moved
    std::vector<int64_t> frame_indices_copy = frame_indices;

    // Create the camera timeframe
    if (!createCameraTimeFrame(timeframe_key, std::move(frame_indices), overwrite)) {
        return false;
    }

    // Get the timeframe
    auto timeframe_opt = getTimeV2(timeframe_key);
    if (!timeframe_opt.has_value()) {
        return false;
    }

    // Create time vector with the actual frame indices
    std::vector<TimeFrameIndex> time_vector;
    time_vector.reserve(analog_data.size());
    for (int64_t frame_idx: frame_indices_copy) {
        time_vector.push_back(TimeFrameIndex(frame_idx));
    }

    // Create the analog series
    auto series = std::make_shared<AnalogTimeSeries>(std::move(analog_data), std::move(time_vector));
    setDataV2(data_key, series, timeframe_opt.value(), timeframe_key);
    return true;
}

bool DataManager::createAnalogTimeSeriesWithDenseCamera(std::string const & data_key,
                                                        std::string const & timeframe_key,
                                                        std::vector<float> analog_data,
                                                        int64_t start_frame,
                                                        bool overwrite) {
    // Create the dense camera timeframe
    if (!createDenseCameraTimeFrame(timeframe_key, start_frame,
                                    static_cast<int64_t>(analog_data.size()), overwrite)) {
        return false;
    }

    // Get the timeframe
    auto timeframe_opt = getTimeV2(timeframe_key);
    if (!timeframe_opt.has_value()) {
        return false;
    }

    // Create time vector with the actual frame indices
    std::vector<TimeFrameIndex> time_vector;
    time_vector.reserve(analog_data.size());
    for (size_t i = 0; i < analog_data.size(); ++i) {
        time_vector.push_back(TimeFrameIndex(start_frame + static_cast<int64_t>(i)));
    }

    // Create the analog series
    auto series = std::make_shared<AnalogTimeSeries>(std::move(analog_data), std::move(time_vector));
    setDataV2(data_key, series, timeframe_opt.value(), timeframe_key);
    return true;
}

std::string DataManager::getAnalogCoordinateType(std::string const & data_key) {
    // Check if the data exists and is an AnalogTimeSeries
    if (_data.find(data_key) == _data.end()) {
        return "not_found";
    }

    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(_data[data_key])) {
        return "not_analog_timeseries";
    }

    auto series = std::get<std::shared_ptr<AnalogTimeSeries>>(_data[data_key]);
    if (!series) {
        return "null_series";
    }

    return series->getCoordinateType();
}

bool DataManager::analogUsesCoordinateTypeImpl(std::string const & data_key, std::string const & type_name) {
    // Check if the data exists and is an AnalogTimeSeries
    if (_data.find(data_key) == _data.end()) {
        return false;
    }

    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(_data[data_key])) {
        return false;
    }

    auto series = std::get<std::shared_ptr<AnalogTimeSeries>>(_data[data_key]);
    if (!series) {
        return false;
    }

    return series->getCoordinateType() == type_name;
}
