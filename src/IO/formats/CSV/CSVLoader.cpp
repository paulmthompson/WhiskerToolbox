#include "CSVLoader.hpp"

#include "analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "common/CSV_Loaders.hpp"
#include "digitaltimeseries/Digital_Event_Series_CSV.hpp"
#include "digitaltimeseries/Digital_Interval_Series_CSV.hpp"
#include "digitaltimeseries/MultiColumnBinaryCSV.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "IO/formats/CSV/points/Point_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "lines/Line_Data_CSV.hpp"
#include "mask/Mask_Data_CSV.hpp"

#include "CoreUtilities/json_reflection.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <filesystem>
#include <iostream>

using namespace WhiskerToolbox::Reflection;

LoadResult CSVLoader::load(std::string const & filepath,
                           DM_DataType dataType,
                           nlohmann::json const & config) const {
    switch (dataType) {
        case DM_DataType::Line:
            return loadLineDataCSV(filepath, config);
        case DM_DataType::Points: {
            // Check if DLC format
            std::string const csv_layout = config.value("csv_layout", "");
            std::string const format = config.value("format", "csv");
            if (csv_layout == "dlc" || format == "dlc_csv") {
                return loadPointDataDLC(filepath, config);
            }
            return loadPointDataCSV(filepath, config);
        }
        case DM_DataType::Mask:
            return loadMaskDataCSV(filepath, config);
        case DM_DataType::Analog:
            return loadAnalogCSV(filepath, config);
        case DM_DataType::DigitalEvent:
            return loadDigitalEventCSV(filepath, config);
        case DM_DataType::DigitalInterval: {
            // Check for binary_state layout
            std::string const csv_layout = config.value("csv_layout", "intervals");
            if (csv_layout == "binary_state") {
                return loadDigitalIntervalBinaryState(filepath, config);
            }
            return loadDigitalIntervalCSV(filepath, config);
        }
        default:
            return LoadResult("CSVLoader does not support data type: " +
                              std::to_string(static_cast<int>(dataType)));
    }
}

bool CSVLoader::supportsFormat(std::string const & format, DM_DataType dataType) const {
    // Support CSV format for all supported data types
    if (format == "csv") {
        return dataType == DM_DataType::Line ||
               dataType == DM_DataType::Points ||
               dataType == DM_DataType::Mask ||
               dataType == DM_DataType::Analog ||
               dataType == DM_DataType::DigitalEvent ||
               dataType == DM_DataType::DigitalInterval;
    }

    // Support dlc_csv format for Points (legacy compatibility)
    if (format == "dlc_csv" && dataType == DM_DataType::Points) {
        return true;
    }

    return false;
}

bool CSVLoader::supportsBatchLoading(std::string const & format,
                                     DM_DataType dataType) const {
    if (format != "csv" && format != "dlc_csv") {
        return false;
    }

    // Batch loading supported for:
    // - DigitalEvent (multiple series per identifier)
    // - DigitalInterval (multiple columns with binary_state layout)
    // - Points with DLC format (multiple bodyparts)
    return dataType == DM_DataType::DigitalEvent ||
           dataType == DM_DataType::DigitalInterval ||
           dataType == DM_DataType::Points;
}

BatchLoadResult CSVLoader::loadBatch(std::string const & filepath,
                                     DM_DataType dataType,
                                     nlohmann::json const & config) const {
    switch (dataType) {
        case DM_DataType::DigitalEvent:
            return loadDigitalEventCSVBatch(filepath, config);
        case DM_DataType::DigitalInterval: {
            // Check for binary_state layout with all_columns
            std::string const csv_layout = config.value("csv_layout", "intervals");
            bool const all_columns = config.value("all_columns", false);

            if (csv_layout == "binary_state" && all_columns) {
                return loadDigitalIntervalBinaryStateBatch(filepath, config);
            }
            // Fall through to single load
            auto result = load(filepath, dataType, config);
            if (result.success) {
                return BatchLoadResult::fromVector({std::move(result)});
            }
            return BatchLoadResult::error(result.error_message);
        }
        case DM_DataType::Points: {
            // Check if DLC format with all_bodyparts
            std::string const csv_layout = config.value("csv_layout", "");
            std::string const format = config.value("format", "csv");
            bool const all_bodyparts = config.value("all_bodyparts", false);

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

LoadResult CSVLoader::save(std::string const & filepath,
                           DM_DataType dataType,
                           nlohmann::json const & config,
                           void const * data) const {
    if (!data) {
        return LoadResult("Data pointer is null");
    }

    switch (dataType) {
        case DM_DataType::Line:
            return saveLineDataCSV(filepath, config, data);
        case DM_DataType::Points:
            return savePointDataCSV(filepath, config, data);
        case DM_DataType::Analog:
            return saveAnalogCSV(filepath, config, data);
        case DM_DataType::DigitalEvent:
            return saveDigitalEventCSV(filepath, config, data);
        case DM_DataType::DigitalInterval:
            return saveDigitalIntervalCSV(filepath, config, data);
        case DM_DataType::Mask:
            return saveMaskDataCSV(filepath, config, data);
        default:
            return LoadResult("CSVLoader does not support saving data type: " +
                              std::to_string(static_cast<int>(dataType)));
    }
}

std::string CSVLoader::getLoaderName() const {
    return "CSVFormatLoader (Line/Points/Mask/Analog/DigitalEvent/DigitalInterval)";
}

// ============================================================================
// LineData Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadLineDataCSV(std::string const & filepath,
                                      nlohmann::json const & config) {
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
            int const width = config["image_width"];
            int const height = config["image_height"];
            line_data->setImageSize(ImageSize{width, height});
        }

        return LoadResult(std::move(line_data));

    } catch (std::exception const & e) {
        return LoadResult("CSV line loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveLineDataCSV(std::string const & filepath,
                                      nlohmann::json const & config,
                                      void const * data) {
    try {
        auto const * line_data = static_cast<LineData const *>(data);

        std::string const save_type = config.value("save_type", "single");

        if (save_type == "single") {
            CSVSingleFileLineSaverOptions save_opts;

            std::filesystem::path const path(filepath);
            save_opts.parent_dir = path.parent_path().string();
            save_opts.filename = path.filename().string();

            if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
            if (config.contains("filename")) save_opts.filename = config["filename"];
            if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
            if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
            if (config.contains("save_header")) save_opts.save_header = config["save_header"];
            if (config.contains("header")) save_opts.header = config["header"];
            if (config.contains("precision")) save_opts.precision = config["precision"];

            bool const ok = ::save(line_data, save_opts);
            if (!ok) {
                return LoadResult("CSV line save failed: I/O error writing file");
            }

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

            bool const ok = ::save(line_data, save_opts);
            if (!ok) {
                return LoadResult("CSV multi-file line save failed: I/O error writing files");
            }

        } else {
            return LoadResult("Unsupported CSV save_type: " + save_type);
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV line save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// PointData Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadPointDataCSV(std::string const & filepath,
                                       nlohmann::json const & config) {
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

    } catch (std::exception const & e) {
        return LoadResult("CSV point loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::loadPointDataDLC(std::string const & filepath,
                                       nlohmann::json const & config) {
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
        std::string const bodypart = config.value("bodypart", "");

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
        auto const & first = all_bodyparts.begin();
        auto point_data = std::make_shared<PointData>(first->second);
        std::cout << "CSVLoader: Loaded first DLC bodypart '" << first->first
                  << "' with " << first->second.size() << " frames" << std::endl;
        return LoadResult(std::move(point_data));

    } catch (std::exception const & e) {
        return LoadResult("DLC point loading failed: " + std::string(e.what()));
    }
}

BatchLoadResult CSVLoader::loadPointDataDLCBatch(std::string const & filepath,
                                                 nlohmann::json const & config) {
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

        for (auto & [name, point_map]: all_bodyparts) {
            auto point_data = std::make_shared<PointData>(point_map);
            LoadResult lr(std::move(point_data));
            lr.name = name;// Set bodypart name
            results.push_back(std::move(lr));
        }

        std::cout << "CSVLoader: Batch loaded " << results.size()
                  << " DLC bodyparts from " << filepath << std::endl;

        return BatchLoadResult::fromVector(std::move(results));

    } catch (std::exception const & e) {
        return BatchLoadResult::error("DLC batch loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::savePointDataCSV(std::string const & filepath,
                                       nlohmann::json const & config,
                                       void const * data) {
    try {
        auto const * point_data = static_cast<PointData const *>(data);

        CSVPointSaverOptions save_opts;

        std::filesystem::path const path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();

        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];

        bool const ok = ::save(point_data, save_opts);
        if (!ok) {
            return LoadResult("CSV point save failed: I/O error writing file");
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV point save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// AnalogTimeSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadAnalogCSV(std::string const & filepath,
                                    nlohmann::json const & config) {
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

    } catch (std::exception const & e) {
        return LoadResult("CSV analog loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveAnalogCSV(std::string const & filepath,
                                    nlohmann::json const & config,
                                    void const * data) {
    try {
        auto const * analog_data = static_cast<AnalogTimeSeries const *>(data);

        CSVAnalogSaverOptions save_opts;

        std::filesystem::path const path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();

        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];
        if (config.contains("precision")) save_opts.precision = config["precision"];

        bool const ok = ::save(analog_data, save_opts);
        if (!ok) {
            return LoadResult("CSV analog save failed: I/O error writing file");
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV analog save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// DigitalEventSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadDigitalEventCSV(std::string const & filepath,
                                          nlohmann::json const & config) {
    try {
        CSVEventLoaderOptions opts;
        opts.filepath = filepath;

        if (config.contains("delimiter")) opts.delimiter = config["delimiter"];
        if (config.contains("has_header")) opts.has_header = config["has_header"];
        if (config.contains("event_column")) opts.event_column = config["event_column"];
        if (config.contains("base_name")) opts.base_name = config["base_name"];

        // For single load, don't use identifier column
        opts.identifier_column = -1;

        // Pass scale options to the loader - scaling is applied during parsing
        // before float-to-int conversion, which is critical for sub-1.0 timestamps
        if (config.contains("scale")) opts.scale = config["scale"];
        if (config.contains("scale_divide")) opts.scale_divide = config["scale_divide"];

        auto loaded_series = ::load(opts);

        if (loaded_series.empty()) {
            return LoadResult("No data loaded from CSV file: " + filepath);
        }

        // Scaling is now applied during parsing in the underlying load() function
        // No post-processing needed here

        std::cout << "CSVLoader: Loaded digital event series from " << filepath << std::endl;

        return LoadResult(std::move(loaded_series[0]));

    } catch (std::exception const & e) {
        return LoadResult("CSV digital event loading failed: " + std::string(e.what()));
    }
}

BatchLoadResult CSVLoader::loadDigitalEventCSVBatch(std::string const & filepath,
                                                    nlohmann::json const & config) {
    try {
        CSVEventLoaderOptions opts;
        opts.filepath = filepath;

        if (config.contains("delimiter")) opts.delimiter = config["delimiter"];
        if (config.contains("has_header")) opts.has_header = config["has_header"];
        if (config.contains("event_column")) opts.event_column = config["event_column"];
        if (config.contains("base_name")) opts.base_name = config["base_name"];
        if (config.contains("identifier_column")) opts.identifier_column = config["identifier_column"];
        if (config.contains("label_column")) opts.identifier_column = config["label_column"];// alias

        // Pass scale options to the loader - scaling is applied during parsing
        // before float-to-int conversion, which is critical for sub-1.0 timestamps
        if (config.contains("scale")) opts.scale = config["scale"];
        if (config.contains("scale_divide")) opts.scale_divide = config["scale_divide"];

        auto loaded_series = ::load(opts);

        if (loaded_series.empty()) {
            return BatchLoadResult::error("No data loaded from CSV file: " + filepath);
        }

        std::vector<LoadResult> results;
        results.reserve(loaded_series.size());

        for (auto & series: loaded_series) {
            // Scaling is now applied during parsing in the underlying load() function
            // No post-processing needed here
            results.emplace_back(std::move(series));
        }

        std::cout << "CSVLoader: Batch loaded " << results.size()
                  << " digital event series from " << filepath << std::endl;

        return BatchLoadResult::fromVector(std::move(results));

    } catch (std::exception const & e) {
        return BatchLoadResult::error("CSV digital event batch loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveDigitalEventCSV(std::string const & filepath,
                                          nlohmann::json const & config,
                                          void const * data) {
    try {
        auto const * event_data = static_cast<DigitalEventSeries const *>(data);

        CSVEventSaverOptions save_opts;

        std::filesystem::path const path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();

        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];
        if (config.contains("precision")) save_opts.precision = config["precision"];

        bool const ok = ::save(event_data, save_opts);
        if (!ok) {
            return LoadResult("CSV digital event save failed: write error");
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV digital event save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// DigitalIntervalSeries Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadDigitalIntervalCSV(std::string const & filepath,
                                             nlohmann::json const & config) {
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

    } catch (std::exception const & e) {
        return LoadResult("CSV digital interval loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveDigitalIntervalCSV(std::string const & filepath,
                                             nlohmann::json const & config,
                                             void const * data) {
    try {
        auto const * interval_data = static_cast<DigitalIntervalSeries const *>(data);

        CSVIntervalSaverOptions save_opts;

        std::filesystem::path const path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();

        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("line_delim")) save_opts.line_delim = config["line_delim"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];

        bool const ok = ::save(interval_data, save_opts);

        LoadResult result;
        result.success = ok;
        if (!ok) {
            result.error_message = "CSV digital interval save failed: write error";
        }
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV digital interval save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// DigitalIntervalSeries Binary State Layout Loading
// ============================================================================

LoadResult CSVLoader::loadDigitalIntervalBinaryState(std::string const & filepath,
                                                     nlohmann::json const & config) {
    try {
        MultiColumnBinaryCSVLoaderOptions opts;
        opts.filepath = filepath;

        // Parse optional configuration
        if (config.contains("header_lines_to_skip")) {
            opts.header_lines_to_skip = rfl::Validator<int, rfl::Minimum<0>>(
                    config["header_lines_to_skip"].get<int>());
        }
        if (config.contains("time_column")) {
            opts.time_column = rfl::Validator<int, rfl::Minimum<0>>(
                    config["time_column"].get<int>());
        }
        if (config.contains("data_column")) {
            opts.data_column = rfl::Validator<int, rfl::Minimum<0>>(
                    config["data_column"].get<int>());
        }
        if (config.contains("delimiter")) {
            opts.delimiter = config["delimiter"].get<std::string>();
        }
        if (config.contains("binary_threshold")) {
            opts.binary_threshold = config["binary_threshold"].get<double>();
        }
        if (config.contains("sampling_rate")) {
            opts.sampling_rate = rfl::Validator<double, rfl::Minimum<0.0>>(
                    config["sampling_rate"].get<double>());
        }

        auto interval_series = ::load(opts);

        if (!interval_series) {
            return LoadResult("Failed to load binary state intervals from " + filepath);
        }

        std::cout << "CSVLoader: Loaded " << interval_series->size()
                  << " intervals from binary state column " << opts.getDataColumn()
                  << " in " << filepath << std::endl;

        return LoadResult(interval_series);

    } catch (std::exception const & e) {
        return LoadResult("CSV binary state interval loading failed: " + std::string(e.what()));
    }
}

BatchLoadResult CSVLoader::loadDigitalIntervalBinaryStateBatch(std::string const & filepath,
                                                               nlohmann::json const & config) {
    try {
        // Get column names to determine how many data columns exist
        int const header_lines = config.value("header_lines_to_skip", 5);
        std::string const delimiter = config.value("delimiter", "\t");
        int const time_column = config.value("time_column", 0);

        auto column_names = getColumnNames(filepath, header_lines, delimiter);

        if (column_names.empty()) {
            return BatchLoadResult::error("Failed to read column names from " + filepath);
        }

        std::cout << "CSVLoader: Found " << column_names.size()
                  << " columns in binary state file" << std::endl;

        std::vector<LoadResult> results;

        // Load each data column (skip time column)
        for (size_t col = 0; col < column_names.size(); ++col) {
            if (static_cast<int>(col) == time_column) {
                continue;// Skip the time column
            }

            // Create config for this column
            nlohmann::json col_config = config;
            col_config["data_column"] = static_cast<int>(col);

            auto result = loadDigitalIntervalBinaryState(filepath, col_config);

            if (result.success) {
                // Set the name from column header
                result.name = column_names[col];
                results.push_back(std::move(result));

                std::cout << "CSVLoader: Loaded column '" << column_names[col]
                          << "' (index " << col << ")" << std::endl;
            } else {
                std::cerr << "CSVLoader: Failed to load column '" << column_names[col]
                          << "': " << result.error_message << std::endl;
            }
        }

        if (results.empty()) {
            return BatchLoadResult::error("No data columns could be loaded from " + filepath);
        }

        std::cout << "CSVLoader: Batch loaded " << results.size()
                  << " interval series from " << filepath << std::endl;

        return BatchLoadResult::fromVector(std::move(results));

    } catch (std::exception const & e) {
        return BatchLoadResult::error("CSV binary state batch loading failed: " + std::string(e.what()));
    }
}

// ============================================================================
// MaskData Loading/Saving
// ============================================================================

LoadResult CSVLoader::loadMaskDataCSV(std::string const & filepath,
                                      nlohmann::json const & config) {
    try {
        CSVMaskRLELoaderOptions opts;
        opts.filepath = filepath;

        if (config.contains("delimiter")) {
            opts.delimiter = config["delimiter"].get<std::string>();
        }
        if (config.contains("rle_delimiter")) {
            opts.rle_delimiter = config["rle_delimiter"].get<std::string>();
        }
        if (config.contains("has_header")) {
            opts.has_header = config["has_header"].get<bool>();
        }
        if (config.contains("header_identifier")) {
            opts.header_identifier = config["header_identifier"].get<std::string>();
        }

        auto mask_map = ::load(opts);

        auto mask_data = std::make_shared<MaskData>();
        for (auto & [time, masks]: mask_map) {
            for (auto & mask: masks) {
                mask_data->addAtTime(time, std::move(mask), NotifyObservers::No);
            }
        }

        // Apply image size if specified in config
        if (config.contains("image_width") && config.contains("image_height")) {
            int const width = config["image_width"];
            int const height = config["image_height"];
            mask_data->setImageSize(ImageSize{width, height});
        }

        std::cout << "CSVLoader: Loaded mask data (RLE) from " << filepath << std::endl;

        return LoadResult(std::move(mask_data));

    } catch (std::exception const & e) {
        return LoadResult("CSV mask RLE loading failed: " + std::string(e.what()));
    }
}

LoadResult CSVLoader::saveMaskDataCSV(std::string const & filepath,
                                      nlohmann::json const & config,
                                      void const * data) {
    try {
        auto const * mask_data = static_cast<MaskData const *>(data);

        CSVMaskRLESaverOptions save_opts;

        std::filesystem::path const path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();

        if (config.contains("parent_dir")) save_opts.parent_dir = config["parent_dir"];
        if (config.contains("filename")) save_opts.filename = config["filename"];
        if (config.contains("delimiter")) save_opts.delimiter = config["delimiter"];
        if (config.contains("rle_delimiter")) save_opts.rle_delimiter = config["rle_delimiter"];
        if (config.contains("save_header")) save_opts.save_header = config["save_header"];
        if (config.contains("header")) save_opts.header = config["header"];

        bool const ok = ::save(mask_data, save_opts);
        if (!ok) {
            return LoadResult("CSV mask RLE save failed: write error");
        }

        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const & e) {
        return LoadResult("CSV mask RLE save failed: " + std::string(e.what()));
    }
}

// ============================================================================
// Saver Info
// ============================================================================

std::vector<SaverInfo> CSVLoader::getSaverInfo() const {
    using WhiskerToolbox::Transforms::V2::extractParameterSchema;
    return {
            {"csv", DM_DataType::Points, "CSV point data (frame, x, y)",
             extractParameterSchema<CSVPointSaverOptions>()},
            {"csv", DM_DataType::Analog, "CSV analog time series",
             extractParameterSchema<CSVAnalogSaverOptions>()},
            {"csv", DM_DataType::DigitalEvent, "CSV digital event timestamps",
             extractParameterSchema<CSVEventSaverOptions>()},
            {"csv", DM_DataType::DigitalInterval, "CSV digital intervals (start/end)",
             extractParameterSchema<CSVIntervalSaverOptions>()},
            {"csv", DM_DataType::Mask, "CSV mask RLE data",
             extractParameterSchema<CSVMaskRLESaverOptions>()},
            {"csv", DM_DataType::Line, "CSV line data (single file)",
             extractParameterSchema<CSVSingleFileLineSaverOptions>()},
    };
}
