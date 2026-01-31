#include "DigitalEventLoader.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Event_Series_CSV.hpp"
#include "loaders/binary_loaders.hpp"

#include <filesystem>
#include <iostream>

LoadResult DigitalEventLoader::load(std::string const& filepath, 
                                   IODataType dataType, 
                                   nlohmann::json const& config) const {
    if (dataType != IODataType::DigitalEvent) {
        return LoadResult("DigitalEventLoader only supports DigitalEvent data type");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "uint16") {
        return loadUint16Binary(filepath, config);
    } else if (format == "csv") {
        return loadCSV(filepath, config);
    }
    
    return LoadResult("DigitalEventLoader does not support format: " + format);
}

bool DigitalEventLoader::supportsFormat(std::string const& format, IODataType dataType) const {
    if (dataType != IODataType::DigitalEvent) {
        return false;
    }
    
    return format == "uint16" || format == "csv";
}

LoadResult DigitalEventLoader::save(std::string const& filepath,
                                   IODataType dataType,
                                   nlohmann::json const& config,
                                   void const* data) const {
    if (dataType != IODataType::DigitalEvent) {
        return LoadResult("DigitalEventLoader only supports saving DigitalEvent data type");
    }
    
    if (!data) {
        return LoadResult("Data pointer is null");
    }
    
    std::string format = config.value("format", "csv");
    
    if (format == "csv") {
        return saveCSV(filepath, config, data);
    }
    
    return LoadResult("DigitalEventLoader does not support saving format: " + format);
}

std::string DigitalEventLoader::getLoaderName() const {
    return "DigitalEventLoader (uint16/CSV)";
}

LoadResult DigitalEventLoader::loadUint16Binary(std::string const& filepath, 
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
        auto events = Loader::extractEvents(digital_data, transition);
        
        std::cout << "DigitalEventLoader: Loaded " << events.size() << " events from binary file" << std::endl;
        
        return LoadResult(std::make_shared<DigitalEventSeries>(events));
        
    } catch (std::exception const& e) {
        return LoadResult("uint16 binary loading failed: " + std::string(e.what()));
    }
}

LoadResult DigitalEventLoader::loadCSV(std::string const& filepath, 
                                      nlohmann::json const& config) const {
    try {
        // Create CSV options from JSON configuration
        CSVEventLoaderOptions opts;
        opts.filepath = filepath;
        opts.delimiter = config.value("delimiter", ",");
        opts.has_header = config.value("has_header", false);
        opts.event_column = config.value("event_column", 0);
        
        // Check for multi-column configuration
        if (config.contains("identifier_column") || config.contains("label_column")) {
            // Multi-column case: defer to legacy loader for full multi-series handling
            // This ensures backward compatibility with multi-series extraction
            return LoadResult("Multi-series CSV files should use legacy loader for full series extraction");
        }
        
        // Single column case
        opts.identifier_column = -1;
        
        // Set base name for series naming
        opts.base_name = config.value("name", "events");
        
        // Load using CSV system
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
            
            // Apply scaling
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
        
        std::cout << "DigitalEventLoader: Loaded digital event series from CSV" << std::endl;
        
        // Return single series
        return LoadResult(std::move(loaded_series[0]));
        
    } catch (std::exception const& e) {
        return LoadResult("CSV loading failed: " + std::string(e.what()));
    }
}

LoadResult DigitalEventLoader::saveCSV(std::string const& filepath,
                                      nlohmann::json const& config,
                                      void const* data) const {
    try {
        auto const* event_data = static_cast<DigitalEventSeries const*>(data);
        
        // Create save options from config
        CSVEventSaverOptions save_opts;
        
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
        
        // Call save function
        ::save(event_data, save_opts);
        
        LoadResult result;
        result.success = true;
        return result;
        
    } catch (std::exception const& e) {
        return LoadResult("CSV event save failed: " + std::string(e.what()));
    }
}
