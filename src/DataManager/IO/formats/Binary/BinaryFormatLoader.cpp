#include "BinaryFormatLoader.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "analogtimeseries/Analog_Time_Series_Binary.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "loaders/binary_loaders.hpp"
#include "utils/json_reflection.hpp"

#include <iostream>

using namespace WhiskerToolbox::Reflection;

LoadResult BinaryFormatLoader::load(std::string const& filepath, 
                                   IODataType dataType, 
                                   nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Analog: {
            // For single-object load, return first channel only
            auto batch = loadAnalogBatch(filepath, config);
            if (batch.success && !batch.results.empty()) {
                return std::move(batch.results[0]);
            }
            return LoadResult(batch.error_message);
        }
        case IODataType::DigitalEvent:
            return loadDigitalEvent(filepath, config);
        case IODataType::DigitalInterval:
            return loadDigitalInterval(filepath, config);
        default:
            return LoadResult("BinaryFormatLoader does not support data type: " + 
                            std::to_string(static_cast<int>(dataType)));
    }
}

bool BinaryFormatLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    // Support "binary" format for all supported types
    if (format == "binary") {
        return dataType == IODataType::Analog ||
               dataType == IODataType::DigitalEvent ||
               dataType == IODataType::DigitalInterval;
    }
    
    // Support "uint16" format for digital types (legacy compatibility)
    if (format == "uint16") {
        return dataType == IODataType::DigitalEvent ||
               dataType == IODataType::DigitalInterval;
    }
    
    return false;
}

bool BinaryFormatLoader::supportsBatchLoading(std::string const& format, 
                                              IODataType dataType) const {
    // Batch loading is primarily useful for multi-channel analog binary files
    if (format == "binary" && dataType == IODataType::Analog) {
        return true;
    }
    return false;
}

BatchLoadResult BinaryFormatLoader::loadBatch(std::string const& filepath,
                                              IODataType dataType,
                                              nlohmann::json const& config) const {
    switch (dataType) {
        case IODataType::Analog:
            return loadAnalogBatch(filepath, config);
        case IODataType::DigitalEvent: {
            // Wrap single load in batch result
            auto result = loadDigitalEvent(filepath, config);
            if (result.success) {
                return BatchLoadResult::fromVector({std::move(result)});
            }
            return BatchLoadResult::error(result.error_message);
        }
        case IODataType::DigitalInterval: {
            // Wrap single load in batch result
            auto result = loadDigitalInterval(filepath, config);
            if (result.success) {
                return BatchLoadResult::fromVector({std::move(result)});
            }
            return BatchLoadResult::error(result.error_message);
        }
        default:
            return BatchLoadResult::error("BinaryFormatLoader does not support batch loading for data type: " + 
                                         std::to_string(static_cast<int>(dataType)));
    }
}

LoadResult BinaryFormatLoader::save(std::string const& /*filepath*/,
                                   IODataType /*dataType*/,
                                   nlohmann::json const& /*config*/,
                                   void const* /*data*/) const {
    return LoadResult("BinaryFormatLoader does not support saving");
}

std::string BinaryFormatLoader::getLoaderName() const {
    return "BinaryFormatLoader (Analog/DigitalEvent/DigitalInterval)";
}

BatchLoadResult BinaryFormatLoader::loadAnalogBatch(std::string const& filepath, 
                                                    nlohmann::json const& config) const {
    try {
        // Add filepath to config for reflection parsing
        nlohmann::json config_with_filepath = config;
        config_with_filepath["filepath"] = filepath;
        
        // Try reflected version with validation first
        auto opts_result = parseJson<BinaryAnalogLoaderOptions>(config_with_filepath);
        
        if (!opts_result) {
            return BatchLoadResult::error("BinaryAnalogLoader parsing failed: " + 
                                         std::string(opts_result.error()->what()));
        }
        
        // Successfully parsed with reflect-cpp, use validated options
        auto opts = opts_result.value();
        
        // Load all channels using existing multi-channel loading
        auto analog_series_vec = ::load(opts);
        
        if (analog_series_vec.empty()) {
            return BatchLoadResult::error("No data loaded from binary file: " + filepath);
        }
        
        // Convert to BatchLoadResult with channel names
        std::vector<LoadResult> results;
        results.reserve(analog_series_vec.size());
        
        for (size_t i = 0; i < analog_series_vec.size(); ++i) {
            LoadResult lr(std::move(analog_series_vec[i]));
            lr.name = std::to_string(i);  // Channel index as name
            results.push_back(std::move(lr));
        }
        
        std::cout << "BinaryFormatLoader: Loaded " << results.size() 
                  << " analog channel(s) from " << filepath << std::endl;
        
        return BatchLoadResult::fromVector(std::move(results));
        
    } catch (std::exception const& e) {
        return BatchLoadResult::error("Binary analog loading failed: " + std::string(e.what()));
    }
}

LoadResult BinaryFormatLoader::loadDigitalEvent(std::string const& filepath, 
                                               nlohmann::json const& config) const {
    try {
        // Validate required fields
        if (!config.contains("channel") || !config.contains("transition")) {
            return LoadResult("Missing required fields 'channel' and/or 'transition' for digital event binary format");
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
        auto events = Loader::extractEvents(digital_data, transition);
        
        std::cout << "BinaryFormatLoader: Loaded " << events.size() 
                  << " digital events from " << filepath << std::endl;
        
        return LoadResult(std::make_shared<DigitalEventSeries>(events));
        
    } catch (std::exception const& e) {
        return LoadResult("Digital event binary loading failed: " + std::string(e.what()));
    }
}

LoadResult BinaryFormatLoader::loadDigitalInterval(std::string const& filepath, 
                                                  nlohmann::json const& config) const {
    try {
        // Validate required fields
        if (!config.contains("channel") || !config.contains("transition")) {
            return LoadResult("Missing required fields 'channel' and/or 'transition' for digital interval binary format");
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
        
        std::cout << "BinaryFormatLoader: Loaded " << intervals.size() 
                  << " digital intervals from " << filepath << std::endl;
        
        return LoadResult(std::make_shared<DigitalIntervalSeries>(intervals));
        
    } catch (std::exception const& e) {
        return LoadResult("Digital interval binary loading failed: " + std::string(e.what()));
    }
}
