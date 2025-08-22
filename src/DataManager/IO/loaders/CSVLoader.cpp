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

LoadResult CSVLoader::save(std::string const& filepath,
                           IODataType dataType,
                           nlohmann::json const& config,
                           void const* data) const {
    static_cast<void>(filepath);
    if (dataType != IODataType::Line) {
        return LoadResult("CSVLoader only supports saving LineData");
    }

    if (!data) {
        return LoadResult("Data pointer is null");
    }

    try {
        // Cast void pointer back to LineData
        auto const* line_data = static_cast<LineData const*>(data);

        // Check if it's single-file or multi-file format
        std::string save_type = config.value("save_type", "single");

        if (save_type == "single") {
            // Convert JSON config to CSVSingleFileLineSaverOptions
            CSVSingleFileLineSaverOptions save_opts;
            save_opts.parent_dir = config.value("parent_dir", ".");
            save_opts.filename = config.value("filename", "line_data.csv");
            save_opts.delimiter = config.value("delimiter", ",");
            save_opts.line_delim = config.value("line_delim", "\n");
            save_opts.save_header = config.value("save_header", true);
            save_opts.header = config.value("header", "Frame,X,Y");
            save_opts.precision = config.value("precision", 1);

            // Call the existing save function
            ::save(line_data, save_opts);

        } else if (save_type == "multi") {
            // Convert JSON config to CSVMultiFileLineSaverOptions
            CSVMultiFileLineSaverOptions save_opts;
            save_opts.parent_dir = config.value("parent_dir", ".");
            save_opts.delimiter = config.value("delimiter", ",");
            save_opts.line_delim = config.value("line_delim", "\n");
            save_opts.save_header = config.value("save_header", true);
            save_opts.header = config.value("header", "X,Y");
            save_opts.precision = config.value("precision", 1);
            save_opts.frame_id_padding = config.value("frame_id_padding", 7);
            save_opts.overwrite_existing = config.value("overwrite_existing", false);

            // Call the existing save function
            ::save(line_data, save_opts);

        } else {
            return LoadResult("Unsupported CSV save_type: " + save_type + ". Use 'single' or 'multi'");
        }

        // Return success - use default constructor which sets success=true by default
        LoadResult result;
        result.success = true;
        return result;

    } catch (std::exception const& e) {
        return LoadResult("CSVLoader save failed: " + std::string(e.what()));
    }
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
