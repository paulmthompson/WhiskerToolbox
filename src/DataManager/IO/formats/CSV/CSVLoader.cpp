#include "CSVLoader.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"
#include "Points/Point_Data.hpp"
#include "analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"
#include "digitaltimeseries/Digital_Interval_Series_CSV.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "loaders/CSV_Loaders.hpp"
#include "utils/json_reflection.hpp"

#include <filesystem>
#include <iostream>

using namespace WhiskerToolbox::Reflection;

LoadResult CSVLoader::load(std::string const& filepath, 
                          IODataType dataType, 
                          nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Line:
            return loadLineDataCSV(filepath, config);
        case IODataType::Points: {
            // Check if DLC format
            std::string csv_layout = config.value("csv_layout", "");
            std::string format = config.value("format", "csv");
            if (csv_layout == "dlc" || format == "dlc_csv") {
                return loadPointDataDLC(filepath, config);
            }
            return loadPointDataCSV(filepath, config);
        }
        case IODataType::Analog:
            return loadAnalogCSV(filepath, config);
        case IODataType::DigitalEvent:
            return loadDigitalEventCSV(filepath, config);
        case IODataType::DigitalInterval:
            return loadDigitalIntervalCSV(filepath, config);
        default:
            return LoadResult("CSVLoader does not support data type: " + 
                            std::to_string(static_cast<int>(dataType)));
    }
}

bool CSVLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support CSV format for all supported data types
    if (format == "csv") {
        return dataType == IODataType::Line ||
               dataType == IODataType::Points ||
               dataType == IODataType::Analog ||
               dataType == IODataType::DigitalEvent ||
               dataType == IODataType::DigitalInterval;
    }
    
    // Support dlc_csv format for Points (legacy compatibility)
    if (format == "dlc_csv" && dataType == IODataType::Points) {
        return true;
    }
    
    return false;
}

bool CSVLoader::supportsBatchLoading(std::string const& format, 
                                     IODataType dataType) const {
    if (format != "csv" && format != "dlc_csv") {
        return false;
    }
    
    // Batch loading supported for:
    // - DigitalEvent (multiple series per identifier)
    // - Points with DLC format (multiple bodyparts)
    return dataType == IODataType::DigitalEvent ||
           dataType == IODataType::Points;
}

BatchLoadResult CSVLoader::loadBatch(std::string const& filepath,
                                     IODataType dataType,
                                     nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::DigitalEvent:
            return loadDigitalEventCSVBatch(filepath, config);
        case IODataType::Points: {
            // Check if DLC format with all_bodyparts
            std::string csv_layout = config.value("csv_layout", "");
            std::string format = config.value("format", "csv");
            bool all_bodyparts = config.value("all_bodyparts", false);
            
            if ((csv_layout == "dlc" || format == "dlc_csv") && all_bodyparts) {
                return loadPointDataDLCBatch(filepath, config);
            }
            // Fall through to single load
            auto result = load(filepath, dataType, config);
            if (result.success) {
                return BatchLoadResult::fromVector({std::move(result)});
            }
            return BatchLoadResult::error(result.error_message);
        }
        default: {
            // Fall back to single load for other types
            auto result = load(filepath, dataType, config);
            if (result.success) {
                return BatchLoadResult::fromVector({std::move(result)});
            }
            return BatchLoadResult::error(result.error_message);
        }
    }
}

LoadResult CSVLoader::save(std::string const& filepath,
                          IODataType dataType,
                          nlohmann::json const& config,
                          void const* data) const {
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    switch (dataType) {
        case IODataType::Line:
            return saveLineDataCSV(filepath, config, data);
        case IODataType::Points:
            return savePointDataCSV(filepath, config, data);
        case IODataType::Analog:
            return saveAnalogCSV(filepath, config, data);
        case IODataType::DigitalEvent:
            return saveDigitalEventCSV(filepath, config, data);
        case IODataType::DigitalInterval:
            return saveDigitalIntervalCSV(filepath, config, data);
        default:
            return LoadResult("CSVLoader does not support saving data type: " + 
                            std::to_string(static_cast<int>(dataType)));
    }
}

std::string CSVLoader::getLoaderName() const {
    return "CSVFormatLoader (Line/Points/Analog/DigitalEvent/DigitalInterval)";
}

// ============================================================================
// LineData Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadLineDataCSV(std::string const& filepath, 
                                     nlohmann::json const& config) const {
    try {
        std::map<TimeFrameIndex, std::vector<Line2D>> line_map;
        
        if (config.contains("multi_file") && config["multi_file"] == true) {
            // Multi-file CSV loading
            CSVMultiFileLineLoaderOptions opts;
            opts.parent_dir = filepath;
            
            if (config.contains("delimiter")) {
                opts.delimiter = config["delimiter"];
            }
            if (config.contains("x_column")) {
                opts.x_column = config["x_column"];
            }
            if (config.contains("y_column")) {
                opts.y_column = config["y_column"];
            }
            if (config.contains("has_header")) {
                opts.has_header = config["has_header"];
            }
            
            line_map = ::load(opts);
        } else {
            // Single-file CSV loading
            CSVSingleFileLineLoaderOptions opts;
            opts.filepath = filepath;
            
            if (config.contains("delimiter")) {
                opts.delimiter = config["delimiter"];
            }
            if (config.contains("coordinate_delimiter")) {
                opts.coordinate_delimiter = config["coordinate_delimiter"];
            }
            if (config.contains("has_header")) {
                opts.has_header = config["has_header"];
            }
            if (config.contains("header_identifier")) {
                opts.header_identifier = config["header_identifier"];
            }
            
            line_map = ::load(opts);
        }
        
        // Create LineData directly
        auto line_data = std::make_shared<LineData>(line_map);
        
        // Apply image size if specified in config
        if (config.contains("image_width") && config.contains("image_height")) {
            int width = config["image_width"];
            int height = config["image_height"];
            line_data->setImageSize(ImageSize{width, height});
        }
        
        return LoadResult(std::move(line_data));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV line loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveLineDataCSV(std::string const& filepath,
                                     nlohmann::json const& config,
                                     void const* data) const {
    try {
        auto const* line_data = static_cast<LineData const*>(data);
        
        std::string save_type = config.value("save_type", "single");

        if (save_type == "single") {
            CSVSingleFileLineSaverOptions save_opts;
            
            std::filesystem::path path(filepath);
            save_opts.parent_dir = path.parent_path().string();
            save_opts.filename = path.filename().string();
            
            if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
            if (config.contains("filename")) save_opts.filename = config["filename"];
            if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
            if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
            if (config.contains("save_header")) save_opts.save_header = config["save_header"];
            if (config.contains("header")) save_opts.header = config["header"];
            if (config.contains("precision")) save_opts.precision = config["precision"];

            ::save(line_data, save_opts);

        } else if (save_type == "multi") {
            CSVMultiFileLineSaverOptions save_opts;
            save_opts.parent_dir = config.value("parent_dir", ".");
            if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
            if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
            if (config.contains("save_header")) save_opts.save_header = config["save_header"];
            if (config.contains("header")) save_opts.header = config["header"];
            if (config.contains("precision")) save_opts.precision = config["precision"];
            if (config.contains("frame_id_padding")) save_opts.frame_id_padding = config["frame_id_padding"];
            if (config.contains("overwrite_existing")) save_opts.overwrite_existing = config["overwrite_existing"];

            ::save(line_data, save_opts);

        } else {
            return LoadResult("Unsupported CSV save_type: " + save_type);
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSV line save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// PointData Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadPointDataCSV(std::string const& filepath, 
                                      nlohmann::json const& config) const {
    try {
        auto opts_result = parseJson<CSVPointLoaderOptions>(config);
        
        if (!opts_result) {
            return LoadResult("CSVPointLoader parsing failed: " + 
                            std::string(opts_result.error()->what()));
        }
        
        auto opts = opts_result.value();
        opts.filepath = filepath;
        
        auto point_map = ::load(opts);
        
        auto point_data = std::make_shared<PointData>(point_map);
        
        std::cout << "CSVLoader: Loaded " << point_map.size() 
                  << " point frames from " << filepath << std::endl;
        
        return LoadResult(std::move(point_data));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV point loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::loadPointDataDLC(std::string const& filepath, 
                                      nlohmann::json const& config) const {
    try {
        // Add filepath to config for reflection parsing
        nlohmann::json config_with_filepath = config;
        config_with_filepath["filepath"] = filepath;
        
        auto opts_result = parseJson<DLCPointLoaderOptions>(config_with_filepath);
        
        if (!opts_result) {
            return LoadResult("DLCPointLoader parsing failed: " + 
                            std::string(opts_result.error()->what()));
        }
        
        auto opts = opts_result.value();
        
        // Check for specific bodypart
        std::string bodypart = config.value("bodypart", "");
        
        auto all_bodyparts = load_dlc_csv(opts);
        
        if (all_bodyparts.empty()) {
            return LoadResult("No bodyparts found in DLC file: " + filepath);
        }
        
        if (!bodypart.empty()) {
            // Return specific bodypart
            auto it = all_bodyparts.find(bodypart);
            if (it == all_bodyparts.end()) {
                return LoadResult("Bodypart '" + bodypart + "' not found in DLC file");
            }
            auto point_data = std::make_shared<PointData>(it->second);
            std::cout << "CSVLoader: Loaded DLC bodypart '" << bodypart 
                      << "' with " << it->second.size() << " frames" << std::endl;
            return LoadResult(std::move(point_data));
        }
        
        // Return first bodypart
        auto const& first = all_bodyparts.begin();
        auto point_data = std::make_shared<PointData>(first->second);
        std::cout << "CSVLoader: Loaded first DLC bodypart '" << first->first 
                  << "' with " << first->second.size() << " frames" << std::endl;
        return LoadResult(std::move(point_data));
        
    } catch (std::exception const& e) {
        return LoadResult("DLC point loading failed: " + std::string(e.what()));
    }
}

BatchLoadResult CSVLoader::loadPointDataDLCBatch(std::string const& filepath, 
                                                 nlohmann::json const& config) const {
    try {
        // Add filepath to config for reflection parsing
        nlohmann::json config_with_filepath = config;
        config_with_filepath["filepath"] = filepath;
        
        auto opts_result = parseJson<DLCPointLoaderOptions>(config_with_filepath);
        
        if (!opts_result) {
            return BatchLoadResult::error("DLCPointLoader parsing failed: " + 
                                         std::string(opts_result.error()->what()));
        }
        
        auto opts = opts_result.value();
        
        auto all_bodyparts = load_dlc_csv(opts);
        
        if (all_bodyparts.empty()) {
            return BatchLoadResult::error("No bodyparts found in DLC file: " + filepath);
        }
        
        std::vector<LoadResult> results;
        results.reserve(all_bodyparts.size());
        
        for (auto& [name, point_map] : all_bodyparts) {
            auto point_data = std::make_shared<PointData>(point_map);
            LoadResult lr(std::move(point_data));
            lr.name = name;  // Set bodypart name
            results.push_back(std::move(lr));
        }
        
        std::cout << "CSVLoader: Batch loaded " << results.size() 
                  << " DLC bodyparts from " << filepath << std::endl;
        
        return BatchLoadResult::fromVector(std::move(results));
        
    } catch (std::exception const& e) {
        return BatchLoadResult::error("DLC batch loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::savePointDataCSV(std::string const& filepath,
                                      nlohmann::json const& config,
                                      void const* data) const {
    try {
        auto const* point_data = static_cast<PointData const*>(data);
        
        CSVPointSaverOptions save_opts;
        
        std::filesystem::path path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();
        
        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];

        ::save(point_data, save_opts);

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSV point save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// AnalogTimeSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadAnalogCSV(std::string const& filepath, 
                                   nlohmann::json const& config) const {
    try {
        auto opts_result = parseJson<CSVAnalogLoaderOptions>(config);
        
        if (!opts_result) {
            return LoadResult("CSVAnalogLoader parsing failed: " + 
                            std::string(opts_result.error()->what()));
        }
        
        auto opts = opts_result.value();
        opts.filepath = filepath;
        
        auto analog_series = ::load(opts);
        
        if (!analog_series) {
            return LoadResult("No data loaded from CSV file: " + filepath);
        }
        
        std::cout << "CSVLoader: Loaded analog time series from " << filepath << std::endl;
        
        return LoadResult(std::move(analog_series));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV analog loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveAnalogCSV(std::string const& filepath,
                                   nlohmann::json const& config,
                                   void const* data) const {
    try {
        auto const* analog_data = static_cast<AnalogTimeSeries const*>(data);
        
        CSVAnalogSaverOptions save_opts;
        
        std::filesystem::path path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();
        
        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];
        if (config.contains("precision")) save_opts.precision = config["precision"];

        ::save(const_cast<AnalogTimeSeries*>(analog_data), save_opts);

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSV analog save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// DigitalEventSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadDigitalEventCSV(std::string const& filepath, 
                                         nlohmann::json const& config) const {
    try {
        CSVEventLoaderOptions opts;
        opts.filepath = filepath;
        
        if (config.contains("delimiter")) opts.delimiter = config["delimiter"];
        if (config.contains("has_header")) opts.has_header = config["has_header"];
        if (config.contains("event_column")) opts.event_column = config["event_column"];
        if (config.contains("base_name")) opts.base_name = config["base_name"];
        
        // For single load, don't use identifier column
        opts.identifier_column = -1;
        
        auto loaded_series = ::load(opts);
        
        if (loaded_series.empty()) {
            return LoadResult("No data loaded from CSV file: " + filepath);
        }
        
        // Apply scaling if specified
        float const scale = config.value("scale", 1.0f);
        bool const scale_divide = config.value("scale_divide", false);
        
        if (scale != 1.0f) {
            auto& series = loaded_series[0];
            auto events_view = series->view();
            std::vector<TimeFrameIndex> events;
            for (auto e : events_view) {
                events.push_back(e.time());
            }
            
            if (scale_divide) {
                for (auto& e : events) {
                    e = TimeFrameIndex(static_cast<int64_t>(e.getValue() / scale));
                }
            } else {
                for (auto& e : events) {
                    e = TimeFrameIndex(static_cast<int64_t>(e.getValue() * scale));
                }
            }
            
            loaded_series[0] = std::make_shared<DigitalEventSeries>(events);
        }
        
        std::cout << "CSVLoader: Loaded digital event series from " << filepath << std::endl;
        
        return LoadResult(std::move(loaded_series[0]));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV digital event loading failed: " + std::string(e.what()));
    }
}

BatchLoadResult CSVLoader::loadDigitalEventCSVBatch(std::string const& filepath, 
                                                   nlohmann::json const& config) const {
    try {
        CSVEventLoaderOptions opts;
        opts.filepath = filepath;
        
        if (config.contains("delimiter")) opts.delimiter = config["delimiter"];
        if (config.contains("has_header")) opts.has_header = config["has_header"];
        if (config.contains("event_column")) opts.event_column = config["event_column"];
        if (config.contains("base_name")) opts.base_name = config["base_name"];
        if (config.contains("identifier_column")) opts.identifier_column = config["identifier_column"];
        if (config.contains("label_column")) opts.identifier_column = config["label_column"]; // alias
        
        auto loaded_series = ::load(opts);
        
        if (loaded_series.empty()) {
            return BatchLoadResult::error("No data loaded from CSV file: " + filepath);
        }
        
        // Apply scaling if specified
        float const scale = config.value("scale", 1.0f);
        bool const scale_divide = config.value("scale_divide", false);
        
        std::vector<LoadResult> results;
        results.reserve(loaded_series.size());
        
        for (auto& series : loaded_series) {
            if (scale != 1.0f) {
                auto events_view = series->view();
                std::vector<TimeFrameIndex> events;
                for (auto e : events_view) {
                    events.push_back(e.time());
                }
                
                if (scale_divide) {
                    for (auto& e : events) {
                        e = TimeFrameIndex(static_cast<int64_t>(e.getValue() / scale));
                    }
                } else {
                    for (auto& e : events) {
                        e = TimeFrameIndex(static_cast<int64_t>(e.getValue() * scale));
                    }
                }
                
                series = std::make_shared<DigitalEventSeries>(events);
            }
            
            results.push_back(LoadResult(std::move(series)));
        }
        
        std::cout << "CSVLoader: Batch loaded " << results.size() 
                  << " digital event series from " << filepath << std::endl;
        
        return BatchLoadResult::fromVector(std::move(results));
        
    } catch (std::exception const& e) {
        return BatchLoadResult::error("CSV digital event batch loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveDigitalEventCSV(std::string const& filepath,
                                         nlohmann::json const& config,
                                         void const* data) const {
    try {
        auto const* event_data = static_cast<DigitalEventSeries const*>(data);
        
        CSVEventSaverOptions save_opts;
        
        std::filesystem::path path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();
        
        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];
        if (config.contains("precision")) save_opts.precision = config["precision"];

        ::save(event_data, save_opts);

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSV digital event save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// DigitalIntervalSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadDigitalIntervalCSV(std::string const& filepath, 
                                            nlohmann::json const& config) const {
    try {
        auto opts = Loader::CSVPairColumnOptions{.filename = filepath};
        
        if (config.contains("skip_header")) {
            opts.skip_header = config["skip_header"];
        } else {
            opts.skip_header = true;
        }
        
        if (config.contains("delimiter")) {
            opts.col_delimiter = config["delimiter"];
        }
        
        if (config.contains("flip_column_order")) {
            opts.flip_column_order = config["flip_column_order"];
        }
        
        auto intervals = Loader::loadPairColumnCSV(opts);
        
        std::cout << "CSVLoader: Loaded " << intervals.size() 
                  << " intervals from " << filepath << std::endl;
        
        return LoadResult(std::make_shared<DigitalIntervalSeries>(intervals));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV digital interval loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveDigitalIntervalCSV(std::string const& filepath,
                                            nlohmann::json const& config,
                                            void const* data) const {
    try {
        auto const* interval_data = static_cast<DigitalIntervalSeries const*>(data);
        
        CSVIntervalSaverOptions save_opts;
        
        std::filesystem::path path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();
        
        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];

        ::save(interval_data, save_opts);

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSV digital interval save failed: " + std::string(e.what()));
    }
}
