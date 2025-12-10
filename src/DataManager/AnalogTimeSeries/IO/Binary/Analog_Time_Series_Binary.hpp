#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include "utils/LoaderOptionsConcepts.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

class AnalogTimeSeries;

// Custom validator for binary_data_type (the storage format in the binary file)
struct ValidBinaryDataType {
    static constexpr auto valid_types = std::array{"int16", "float32", "int8", "uint16", "float64"};
    
    static rfl::Result<std::string> validate(const std::string& value) {
        for (const auto& type : valid_types) {
            if (value == type) {
                return value;
            }
        }
        return rfl::Error("Invalid binary_data_type: '" + value + "'. Must be one of: int16, float32, int8, uint16, float64");
    }
};

/**
 * @brief Binary analog data loader options with validation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization
 * and includes validators to ensure data integrity.
 * Optional fields can be omitted from JSON and will use default values.
 * 
 * @note This struct conforms to ValidLoaderOptions concept:
 *       - Uses `filepath` (not `filename`) for consistency with DataManager JSON
 *       - Uses `binary_data_type` (not `data_type`) to avoid conflict with DataManager's data_type field
 */
struct BinaryAnalogLoaderOptions {
    std::string filepath;  // Path to the binary file (consistent with DataManager JSON)
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
    
    // Binary storage type with custom validator (renamed from data_type to avoid conflict)
    std::optional<rfl::Validator<std::string, ValidBinaryDataType>> binary_data_type;
    
    std::optional<float> scale_factor;
    std::optional<float> offset_value;
    std::optional<size_t> num_samples;
    
    // Helper methods to get values with defaults
    std::string getParentDir() const { return parent_dir.value_or("."); }
    int getHeaderSize() const { return header_size.has_value() ? header_size.value().value() : 0; }
    int getNumChannels() const { return num_channels.has_value() ? num_channels.value().value() : 1; }
    bool getUseMemoryMapped() const { return use_memory_mapped.value_or(false); }
    size_t getOffset() const { return offset.has_value() ? offset.value().value() : 0; }
    size_t getStride() const { return stride.has_value() ? stride.value().value() : 1; }
    std::string getBinaryDataType() const { return binary_data_type.has_value() ? binary_data_type.value().value() : "int16"; }
    float getScaleFactor() const { return scale_factor.value_or(1.0f); }
    float getOffsetValue() const { return offset_value.value_or(0.0f); }
    size_t getNumSamples() const { return num_samples.value_or(0); }
};

// Compile-time validation that BinaryAnalogLoaderOptions conforms to loader requirements
static_assert(WhiskerToolbox::ValidLoaderOptions<BinaryAnalogLoaderOptions>,
    "BinaryAnalogLoaderOptions must have 'filepath' field and must not have 'data_type' or 'name' fields");

std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
