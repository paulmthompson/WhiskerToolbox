#include "CSVLoader.hpp"

#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "Lines/Line_Data.hpp"
#include "utils/json_helpers.hpp"

#include <iostream>

LoadResult CSVLoader::load(std::string const& filepath, 
                          IODataType dataType, 
                          nlohmann::json const& config, 
                          DataFactory* factory) const {
    if (!factory) {
        return LoadResult("DataFactory is null");
    }

    switch (dataType) {
        case IODataType::Line:
            return loadLineDataCSV(filepath, config, factory);
            
        default:
            return LoadResult("CSV loader does not support data type: " + std::to_string(static_cast<int>(dataType)));
    }
}

bool CSVLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support CSV format for LineData
    if (format == "csv" && dataType == IODataType::Line) {
        return true;
    }
    
    // Could add support for other data types in the future
    return false;
}

std::string CSVLoader::getLoaderName() const {
    return "CSVLoader (Internal)";
}

LoadResult CSVLoader::loadLineDataCSV(std::string const& filepath, 
                                     nlohmann::json const& config, 
                                     DataFactory* factory) const {
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
        
        // Create LineData using factory
        auto line_data_variant = factory->createLineData(line_map);
        
        // Apply image size if specified in config
        if (config.contains("image_width") && config.contains("image_height")) {
            int width = config["image_width"];
            int height = config["image_height"];
            factory->setLineDataImageSize(line_data_variant, width, height);
        }
        
        return LoadResult(std::move(line_data_variant));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV loading failed: " + std::string(e.what()));
    }
}
