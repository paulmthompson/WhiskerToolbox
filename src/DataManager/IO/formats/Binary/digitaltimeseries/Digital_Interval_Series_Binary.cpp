#include "Digital_Interval_Series_Binary.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "loaders/binary_loaders.hpp"

#include <iostream>
#include <stdexcept>

namespace {
    // Helper function to load and extract intervals based on data type
    template<typename T>
    std::vector<Interval> loadIntervalsFromBinary(BinaryIntervalLoaderOptions const & options) {
        // Set up binary loading options
        Loader::BinaryAnalogOptions binary_opts;
        binary_opts.file_path = options.filepath;
        binary_opts.header_size_bytes = options.header_size_bytes;
        binary_opts.num_channels = 1; // We read as single channel, then extract bits
        
        // Read binary data
        auto raw_data = readBinaryFile<T>(binary_opts);
        if (raw_data.empty()) {
            std::cerr << "Error: No data loaded from binary file: " << options.filepath << std::endl;
            return {};
        }
        
        // Extract digital data for the specified channel
        auto digital_data = Loader::extractDigitalData(raw_data, options.channel);
        if (digital_data.empty()) {
            std::cerr << "Error: No digital data extracted for channel " << options.channel << std::endl;
            return {};
        }
        
        // Extract intervals based on transition type
        auto interval_pairs = Loader::extractIntervals(digital_data, options.transition_type);
        
        // Convert to Interval objects
        std::vector<Interval> intervals;
        intervals.reserve(interval_pairs.size());
        for (auto const & pair : interval_pairs) {
            intervals.emplace_back(Interval{
                static_cast<int64_t>(pair.first), 
                static_cast<int64_t>(pair.second)
            });
        }
        
        return intervals;
    }
}

std::vector<Interval> load(BinaryIntervalLoaderOptions const & options) {
    // Validate options
    if (options.filepath.empty()) {
        std::cerr << "Error: Filepath cannot be empty" << std::endl;
        return {};
    }
    
    if (options.channel < 0) {
        std::cerr << "Error: Channel must be non-negative, got: " << options.channel << std::endl;
        return {};
    }
    
    if (options.transition_type != "rising" && options.transition_type != "falling") {
        std::cerr << "Error: Invalid transition type '" << options.transition_type 
                  << "'. Must be 'rising' or 'falling'" << std::endl;
        return {};
    }
    
    // Validate data type and channel range
    int max_channels = 0;
    switch (options.data_type_bytes) {
        case 1: max_channels = 8; break;   // uint8_t: 8 bits
        case 2: max_channels = 16; break;  // uint16_t: 16 bits  
        case 4: max_channels = 32; break;  // uint32_t: 32 bits
        case 8: max_channels = 64; break;  // uint64_t: 64 bits
        default:
            std::cerr << "Error: Invalid data_type_bytes '" << options.data_type_bytes 
                      << "'. Must be 1, 2, 4, or 8" << std::endl;
            return {};
    }
    
    if (options.channel >= max_channels) {
        std::cerr << "Error: Channel " << options.channel << " is out of range for " 
                  << options.data_type_bytes << "-byte data type (max: " << (max_channels - 1) << ")" << std::endl;
        return {};
    }
    
    std::cout << "Loading binary interval data from: " << options.filepath << std::endl;
    std::cout << "  Header size: " << options.header_size_bytes << " bytes" << std::endl;
    std::cout << "  Data type: " << options.data_type_bytes << " bytes (" << max_channels << " channels)" << std::endl;
    std::cout << "  Channel: " << options.channel << std::endl;
    std::cout << "  Transition type: " << options.transition_type << std::endl;
    
    // Load intervals based on data type
    try {
        switch (options.data_type_bytes) {
            case 1:
                return loadIntervalsFromBinary<uint8_t>(options);
            case 2:
                return loadIntervalsFromBinary<uint16_t>(options);
            case 4:
                return loadIntervalsFromBinary<uint32_t>(options);
            case 8:
                return loadIntervalsFromBinary<uint64_t>(options);
            default:
                // This should never happen due to validation above
                std::cerr << "Error: Unexpected data type bytes: " << options.data_type_bytes << std::endl;
                return {};
        }
    } catch (std::exception const & e) {
        std::cerr << "Error loading binary interval data: " << e.what() << std::endl;
        return {};
    }
}

std::shared_ptr<DigitalIntervalSeries> loadIntoDigitalIntervalSeries(BinaryIntervalLoaderOptions const & options) {
    auto intervals = load(options);
    
    if (!intervals.empty()) {
        return std::make_shared<DigitalIntervalSeries>(intervals);
        std::cout << "Successfully loaded " << intervals.size() << " intervals into DigitalIntervalSeries" << std::endl;
    } else {
        std::cout << "Warning: No intervals loaded from binary file" << std::endl;
    }
    
    return std::make_shared<DigitalIntervalSeries>();
}
