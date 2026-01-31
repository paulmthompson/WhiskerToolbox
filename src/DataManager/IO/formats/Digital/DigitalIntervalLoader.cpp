#include "DigitalIntervalLoader.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/IO/Binary/Digital_Interval_Series_Binary.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "DigitalTimeSeries/IO/CSV/MultiColumnBinaryCSV.hpp"
#include "loaders/binary_loaders.hpp"
#include "loaders/CSV_Loaders.hpp"

#include <rfl.hpp>
#include <filesystem>
#include <iostream>

LoadResult DigitalIntervalLoader::load(std::string const& filepath, 
                                      IODataType dataType, 
                                      nlohmann::json const& config) const {
    if (dataType != IODataType::DigitalInterval) {
        return LoadResult("DigitalIntervalLoader only supports DigitalInterval data type");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "uint16") {
        return loadUint16Binary(filepath, config);
    } else if (format == "csv") {
        return loadCSV(filepath, config);
    } else if (format == "multi_column_binary") {
        return loadMultiColumnBinary(filepath, config);
    }
    
    return LoadResult("DigitalIntervalLoader does not support format: " + format);
}

bool DigitalIntervalLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    if (dataType != IODataType::DigitalInterval) {
        return false;
    }
    
    return format == "uint16" || format == "csv" || format == "multi_column_binary";
}

LoadResult DigitalIntervalLoader::save(std::string const& filepath,
                                      IODataType dataType,
                                      nlohmann::json const& config,
                                      void const* data) const {
    if (dataType != IODataType::DigitalInterval) {
        return LoadResult("DigitalIntervalLoader only supports saving DigitalInterval data type");
    }
    
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "csv") {
        return saveCSV(filepath, config, data);
    }
    
    return LoadResult("DigitalIntervalLoader does not support saving format: " + format);
}

std::string DigitalIntervalLoader::getLoaderName() const {
    return "DigitalIntervalLoader (uint16/CSV/MultiColumnBinary)";
}

LoadResult DigitalIntervalLoader::loadUint16Binary(std::string const& filepath, 
                                                  nlohmann::json const& config) const {
    try {
        // Validate required fields
        if (!config.contains("channel") || !config.contains("transition")) {
            return LoadResult("Missing required fields 'channel' and/or 'transition' for uint16 format");
        }
        
        int const channel = config["channel"];
        std::string const transition = config["transition"];
        
        int const header_size = config.value("header_size", 0);
        int const num_channels = config.value("channel_count", 1);
        
        auto opts = Loader::BinaryAnalogOptions{
            .file_path = filepath,
            .header_size_bytes = static_cast<size_t>(header_size),
            .num_channels = static_cast<size_t>(num_channels)
        };
        
        auto data = Loader::readBinaryFile<uint16_t>(opts);
        
        if (data.empty()) {
            return LoadResult("No data read from binary file: " + filepath);
        }
        
        auto digital_data = Loader::extractDigitalData(data, channel);
        auto intervals = Loader::extractIntervals(digital_data, transition);
        
        std::cout << "DigitalIntervalLoader: Loaded " << intervals.size() 
                  << " intervals from binary file" << std::endl;
        
        return LoadResult(std::make_shared<DigitalIntervalSeries>(intervals));
        
    } catch (std::exception const& e) {
        return LoadResult("uint16 binary loading failed: " + std::string(e.what()));
    }
}

LoadResult DigitalIntervalLoader::loadCSV(std::string const& filepath, 
                                         nlohmann::json const& config) const {
    try {
        auto opts = Loader::CSVPairColumnOptions{.filename = filepath};
        
        // Configure CSV loading options from JSON
        if (config.contains("skip_header")) {
            opts.skip_header = config["skip_header"];
        } else {
            // Default to skipping header if not specified
            opts.skip_header = true;
        }
        
        if (config.contains("delimiter")) {
            opts.col_delimiter = config["delimiter"];
        }
        
        if (config.contains("flip_column_order")) {
            opts.flip_column_order = config["flip_column_order"];
        }
        
        auto intervals = Loader::loadPairColumnCSV(opts);
        
        std::cout << "DigitalIntervalLoader: Loaded " << intervals.size() 
                  << " intervals from CSV" << std::endl;
        
        return LoadResult(std::make_shared<DigitalIntervalSeries>(intervals));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV loading failed: " + std::string(e.what()));
    }
}

LoadResult DigitalIntervalLoader::loadMultiColumnBinary(std::string const& filepath,
                                                       nlohmann::json const& config) const {
    try {
        // Create options with filepath set
        MultiColumnBinaryCSVLoaderOptions opts;
        opts.filepath = filepath;
        
        // Parse optional fields from JSON
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
        if (config.contains("sampling_rate")) {
            opts.sampling_rate = rfl::Validator<double, rfl::Minimum<0.0>>(
                config["sampling_rate"].get<double>());
        }
        if (config.contains("binary_threshold")) {
            opts.binary_threshold = config["binary_threshold"].get<double>();
        }
        
        auto result = ::load(opts);
        
        if (result) {
            std::cout << "DigitalIntervalLoader: Loaded multi-column binary intervals from column " 
                      << opts.getDataColumn() << std::endl;
            return LoadResult(std::move(result));
        } else {
            return LoadResult("Failed to load multi-column binary CSV");
        }
        
    } catch (std::exception const& e) {
        return LoadResult("Multi-column binary loading failed: " + std::string(e.what()));
    }
}

LoadResult DigitalIntervalLoader::saveCSV(std::string const& filepath,
                                         nlohmann::json const& config,
                                         void const* data) const {
    try {
        auto const* interval_data = static_cast<DigitalIntervalSeries const*>(data);
        
        // Create save options from config
        CSVIntervalSaverOptions save_opts;
        
        // Parse directory and filename from filepath
        std::filesystem::path path(filepath);
        save_opts.parent_dir = path.parent_path().string();
        save_opts.filename = path.filename().string();
        
        // Override with config values if present
        if (config.contains("parent_dir")) {
            save_opts.parent_dir = config["parent_dir"];
        }
        if (config.contains("filename")) {
            save_opts.filename = config["filename"];
        }
        if (config.contains("delimiter")) {
            save_opts.delimiter = config["delimiter"];
        }
        if (config.contains("line_delim")) {
            save_opts.line_delim = config["line_delim"];
        }
        if (config.contains("save_header")) {
            save_opts.save_header = config["save_header"];
        }
        if (config.contains("header")) {
            save_opts.header = config["header"];
        }
        
        // Call save function
        ::save(interval_data, save_opts);
        
        LoadResult result;
        result.success = true;
        return result;
        
    } catch (std::exception const& e) {
        return LoadResult("CSV interval save failed: " + std::string(e.what()));
    }
}
