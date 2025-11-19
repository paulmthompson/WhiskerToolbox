#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include <memory>
#include <string>
#include <vector>

class AnalogTimeSeries;

struct BinaryAnalogLoaderOptions {
    std::string filename;
    std::string parent_dir = ".";
    int header_size = 0;
    int num_channels = 1;
    
    // Memory-mapped options
    bool use_memory_mapped = false;      // Enable memory-mapped loading
    size_t offset = 0;                   // Sample offset within data (after header)
    size_t stride = 1;                   // Stride between samples (for multi-channel)
    std::string data_type = "int16";     // Data type: "int16", "float32", "int8", etc.
    float scale_factor = 1.0f;           // Multiplicative scale for conversion
    float offset_value = 0.0f;           // Additive offset for conversion
    size_t num_samples = 0;              // Number of samples (0 = auto-detect)
};

std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
