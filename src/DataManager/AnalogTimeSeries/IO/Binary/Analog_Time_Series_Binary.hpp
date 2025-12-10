#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

class AnalogTimeSeries;

// Custom validator for data_type
struct ValidDataType {
    static constexpr auto valid_types = std::array{"int16", "float32", "int8", "uint16", "float64"};
    
    static rfl::Result<std::string> validate(const std::string& value) {
        for (const auto& type : valid_types) {
            if (value == type) {
                return value;
            }
        }
        return rfl::Error("Invalid data_type: '" + value + "'. Must be one of: int16, float32, int8, uint16, float64");
    }
};

/**
 * @brief Binary analog data loader options with validation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization
 * and includes validators to ensure data integrity.
 * Optional fields can be omitted from JSON and will use default values.
 */
struct BinaryAnalogLoaderOptions {
    std::optional<std::string> filename;  // Made optional since it's set by the loader
    std::optional<std::string> parent_dir;
    
    // Validate header_size is non-negative
    std::optional<rfl::Validator<int, rfl::Minimum<0>>> header_size;
    
    // Validate num_channels is at least 1
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> num_channels;
    
    std::optional<bool> use_memory_mapped;
    
    // Validate offset is non-negative
    std::optional<rfl::Validator<size_t, rfl::Minimum<0>>> offset;
    
    // Validate stride is at least 1
    std::optional<rfl::Validator<size_t, rfl::Minimum<1>>> stride;
    
    std::optional<float> scale_factor;
    std::optional<float> offset_value;
    std::optional<size_t> num_samples;
    
    // Binary data type - set programmatically by the loader, not from JSON
    // This avoids conflicts with DataManager's "data_type" field
    rfl::Skip<std::string> binary_data_type = "int16";
    
    // Helper methods to get values with defaults
    std::string getFilename() const { return filename.value_or(""); }
    std::string getParentDir() const { return parent_dir.value_or("."); }
    int getHeaderSize() const { return header_size.has_value() ? header_size.value().value() : 0; }
    int getNumChannels() const { return num_channels.has_value() ? num_channels.value().value() : 1; }
    bool getUseMemoryMapped() const { return use_memory_mapped.value_or(false); }
    size_t getOffset() const { return offset.has_value() ? offset.value().value() : 0; }
    size_t getStride() const { return stride.has_value() ? stride.value().value() : 1; }
    std::string getDataType() const { return binary_data_type.value(); }
    float getScaleFactor() const { return scale_factor.value_or(1.0f); }
    float getOffsetValue() const { return offset_value.value_or(0.0f); }
    size_t getNumSamples() const { return num_samples.value_or(0); }
};

std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
