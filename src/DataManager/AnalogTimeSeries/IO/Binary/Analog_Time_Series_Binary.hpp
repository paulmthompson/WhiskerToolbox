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

/**
 * @brief Reflected version of BinaryAnalogLoaderOptions with validation
 * 
 * This version uses reflect-cpp for automatic JSON serialization/deserialization
 * and includes validators to ensure data integrity.
 * Optional fields can be omitted from JSON and will use default values.
 */
struct BinaryAnalogLoaderOptionsReflected {
    std::string filename;
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
    
    // Data type with custom validator
    std::optional<rfl::Validator<std::string, ValidDataType>> data_type;
    
    std::optional<float> scale_factor;
    std::optional<float> offset_value;
    std::optional<size_t> num_samples;
    
    /**
     * @brief Convert reflected options to legacy format
     * 
     * This allows gradual migration by supporting both formats.
     */
    BinaryAnalogLoaderOptions toLegacy() const {
        BinaryAnalogLoaderOptions legacy;
        legacy.filename = filename;
        legacy.parent_dir = parent_dir.value_or(".");
        legacy.header_size = header_size.has_value() ? header_size.value().value() : 0;
        legacy.num_channels = num_channels.has_value() ? num_channels.value().value() : 1;
        legacy.use_memory_mapped = use_memory_mapped.value_or(false);
        legacy.offset = offset.has_value() ? offset.value().value() : 0;
        legacy.stride = stride.has_value() ? stride.value().value() : 1;
        legacy.data_type = data_type.has_value() ? data_type.value().value() : "int16";
        legacy.scale_factor = scale_factor.value_or(1.0f);
        legacy.offset_value = offset_value.value_or(0.0f);
        legacy.num_samples = num_samples.value_or(0);
        return legacy;
    }
    
    /**
     * @brief Convert legacy options to reflected format
     * 
     * Uses JSON round-trip to handle Literal type conversion properly
     */
    static BinaryAnalogLoaderOptionsReflected fromLegacy(BinaryAnalogLoaderOptions const& legacy) {
        // Create JSON from legacy struct
        nlohmann::json j;
        j["filename"] = legacy.filename;
        j["parent_dir"] = legacy.parent_dir;
        j["header_size"] = legacy.header_size;
        j["num_channels"] = legacy.num_channels;
        j["use_memory_mapped"] = legacy.use_memory_mapped;
        j["offset"] = legacy.offset;
        j["stride"] = legacy.stride;
        j["data_type"] = legacy.data_type;
        j["scale_factor"] = legacy.scale_factor;
        j["offset_value"] = legacy.offset_value;
        j["num_samples"] = legacy.num_samples;
        
        // Parse into reflected struct (this handles Literal validation)
        auto result = rfl::json::read<BinaryAnalogLoaderOptionsReflected>(j.dump());
        if (!result) {
            // Fallback with default data_type if invalid
            BinaryAnalogLoaderOptionsReflected reflected;
            reflected.filename = legacy.filename;
            reflected.parent_dir = legacy.parent_dir;
            reflected.header_size = legacy.header_size;
            reflected.num_channels = legacy.num_channels;
            reflected.use_memory_mapped = legacy.use_memory_mapped;
            reflected.offset = legacy.offset;
            reflected.stride = legacy.stride;
            // data_type stays default (int16)
            reflected.scale_factor = legacy.scale_factor;
            reflected.offset_value = legacy.offset_value;
            reflected.num_samples = legacy.num_samples;
            return reflected;
        }
        return result.value();
    }
};

std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

#endif// ANALOG_TIME_SERIES_LOADER_HPP
