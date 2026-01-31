#include "AnalogLoader.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/IO/Binary/Analog_Time_Series_Binary.hpp"
#include "AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

using namespace WhiskerToolbox::Reflection;

LoadResult AnalogLoader::load(std::string const& filepath, 
                             IODataType dataType, 
                             nlohmann::json const& config) const {
    if (dataType != IODataType::Analog) {
        return LoadResult("AnalogLoader only supports Analog data type");
    }
    
    std::string format = config.value("format", "binary");
    
    if (format == "binary") {
        return loadBinary(filepath, config);
    } else if (format == "csv") {
        return loadCSV(filepath, config);
    }
    
    return LoadResult("AnalogLoader does not support format: " + format);
}

bool AnalogLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    if (dataType != IODataType::Analog) {
        return false;
    }
    
    return format == "binary" || format == "csv";
}

LoadResult AnalogLoader::save(std::string const& filepath,
                             IODataType dataType,
                             nlohmann::json const& config,
                             void const* data) const {
    if (dataType != IODataType::Analog) {
        return LoadResult("AnalogLoader only supports saving Analog data type");
    }
    
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "csv") {
        return saveCSV(filepath, config, data);
    }
    
    return LoadResult("AnalogLoader does not support saving format: " + format);
}

std::string AnalogLoader::getLoaderName() const {
    return "AnalogLoader (Binary/CSV)";
}

LoadResult AnalogLoader::loadBinary(std::string const& filepath, 
                                   nlohmann::json const& config) const {
    try {
        // Try reflected version with validation first
        auto opts_result = parseJson<BinaryAnalogLoaderOptions>(config);
        
        if (opts_result) {
            // Successfully parsed with reflect-cpp, use validated options
            auto opts = opts_result.value();
            opts.filepath = filepath;
            
            // For multi-channel binary files, defer to legacy loader
            // This ensures backward compatibility with existing multi-channel handling
            if (opts.getNumChannels() > 1) {
                return LoadResult("Multi-channel binary files should use legacy loader for full channel extraction");
            }
            
            auto analog_series_vec = ::load(opts);
            
            if (analog_series_vec.empty()) {
                return LoadResult("No data loaded from binary file: " + filepath);
            }
            
            // Return single channel
            return LoadResult(std::move(analog_series_vec[0]));
        } else {
            return LoadResult("BinaryAnalogLoader parsing failed: " + 
                            std::string(opts_result.error()->what()));
        }
    } catch (std::exception const& e) {
        return LoadResult("Binary analog loading failed: " + std::string(e.what()));
    }
}

LoadResult AnalogLoader::loadCSV(std::string const& filepath, 
                                nlohmann::json const& config) const {
    try {
        // Try reflected version with validation first
        auto opts_result = parseJson<CSVAnalogLoaderOptions>(config);
        
        if (opts_result) {
            // Successfully parsed with reflect-cpp, use validated options
            auto opts = opts_result.value();
            opts.filepath = filepath;
            
            auto single_series = ::load(opts);
            
            if (!single_series) {
                return LoadResult("No data loaded from CSV file: " + filepath);
            }
            
            return LoadResult(std::move(single_series));
        } else {
            return LoadResult("CSVAnalogLoader parsing failed: " + 
                            std::string(opts_result.error()->what()));
        }
    } catch (std::exception const& e) {
        return LoadResult("CSV analog loading failed: " + std::string(e.what()));
    }
}

LoadResult AnalogLoader::saveCSV(std::string const& filepath,
                                nlohmann::json const& config,
                                void const* data) const {
    try {
        auto const* analog_data = static_cast<AnalogTimeSeries const*>(data);
        
        // Create save options from config
        CSVAnalogSaverOptions save_opts;
        
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
        if (config.contains("precision")) {
            save_opts.precision = config["precision"];
        }
        
        // Call save function (it takes non-const pointer but doesn't modify)
        ::save(const_cast<AnalogTimeSeries*>(analog_data), save_opts);
        
        LoadResult result;
        result.success = true;
        return result;
        
    } catch (std::exception const& e) {
        return LoadResult("CSV analog save failed: " + std::string(e.what()));
    }
}
