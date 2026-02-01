#include "PointLoader.hpp"

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"
#include "utils/json_reflection.hpp"
#include "loaders/loading_utils.hpp"

#include <iostream>

LoadResult PointLoader::load(std::string const& filepath, 
                            IODataType dataType, 
                            nlohmann::json const& config) const {
    if (dataType != IODataType::Points) {
        return LoadResult("PointLoader only supports Points data type");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "csv") {
        return loadCSV(filepath, config);
    } else if (format == "dlc_csv") {
        return loadDLC(filepath, config);
    }
    
    return LoadResult("PointLoader: Unsupported format '" + format + "'");
}

LoadResult PointLoader::save(std::string const& filepath, 
                            IODataType dataType, 
                            nlohmann::json const& config, 
                            void const* data) const {
    if (dataType != IODataType::Points) {
        return LoadResult("PointLoader only supports Points data type");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "csv") {
        return saveCSV(filepath, config, data);
    }
    
    return LoadResult("PointLoader: Saving not supported for format '" + format + "'");
}

bool PointLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    if (dataType != IODataType::Points) {
        return false;
    }
    
    return format == "csv" || format == "dlc_csv";
}

std::string PointLoader::getLoaderName() const {
    return "PointLoader";
}

LoadResult PointLoader::loadCSV(std::string const& filepath, 
                               nlohmann::json const& config) const {
    try {
        // Inject filepath into JSON for reflection-based parsing
        auto json_with_path = config;
        json_with_path["filepath"] = filepath;
        
        auto result = WhiskerToolbox::Reflection::parseJson<CSVPointLoaderOptions>(json_with_path);
        if (!result) {
            return LoadResult("PointLoader: Failed to parse CSV options: " + 
                            std::string(result.error()->what()));
        }
        
        auto opts = result.value();
        
        // Support legacy 'delim' field by mapping to 'column_delim'
        if (config.contains("delim") && !config.contains("column_delim")) {
            std::string const delim = config["delim"];
            opts.column_delim = delim;
        }
        
        // Use global load function from Point_Data_CSV.hpp
        auto keypoints = ::load(opts);
        
        auto point_data = std::make_shared<PointData>(keypoints);
        
        // Apply image size transformation if specified
        change_image_size_json(point_data, config);
        
        return LoadResult(point_data);
        
    } catch (std::exception const& e) {
        return LoadResult("PointLoader: CSV loading failed: " + std::string(e.what()));
    }
}

LoadResult PointLoader::loadDLC(std::string const& filepath, 
                               nlohmann::json const& config) const {
    try {
        // Inject filepath into JSON for reflection-based parsing
        auto json_with_path = config;
        json_with_path["filepath"] = filepath;
        
        auto result = WhiskerToolbox::Reflection::parseJson<DLCPointLoaderOptions>(json_with_path);
        if (!result) {
            return LoadResult("PointLoader: Failed to parse DLC options: " + 
                            std::string(result.error()->what()));
        }
        
        auto const opts = result.value();
        
        auto dlc_data = load_dlc_csv(opts);
        
        if (dlc_data.empty()) {
            return LoadResult("PointLoader: No bodyparts found in DLC file");
        }
        
        // Return the first bodypart for single-point loading
        // Full multi-bodypart support is handled by DataManager's direct DLC loading
        auto first_bodypart_data = dlc_data.begin()->second;
        auto point_data = std::make_shared<PointData>(first_bodypart_data);
        
        // Apply image size transformation if specified
        change_image_size_json(point_data, config);
        
        return LoadResult(point_data);
        
    } catch (std::exception const& e) {
        return LoadResult("PointLoader: DLC loading failed: " + std::string(e.what()));
    }
}

LoadResult PointLoader::saveCSV(std::string const& filepath,
                               nlohmann::json const& config,
                               void const* data) const {
    try {
        auto const* point_data = static_cast<PointData const*>(data);
        if (!point_data) {
            return LoadResult("PointLoader: Null data pointer for save");
        }
        
        CSVPointSaverOptions opts;
        opts.filename = filepath;
        
        // Get optional configuration
        if (config.contains("delimiter")) {
            opts.delimiter = config["delimiter"].get<std::string>();
        }
        if (config.contains("save_header")) {
            opts.save_header = config["save_header"].get<bool>();
        }
        if (config.contains("header")) {
            opts.header = config["header"].get<std::string>();
        }
        
        // Use global save function from Point_Data_CSV.hpp
        ::save(point_data, opts);
        
        return LoadResult(std::shared_ptr<PointData>(nullptr)); // Success, no data to return
        
    } catch (std::exception const& e) {
        return LoadResult("PointLoader: CSV saving failed: " + std::string(e.what()));
    }
}
