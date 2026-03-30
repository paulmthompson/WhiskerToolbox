#ifndef ANALOG_TIME_SERIES_LOADER_HPP
#define ANALOG_TIME_SERIES_LOADER_HPP

#include "datamanagerio_export.h"

#include "IO/core/LoaderOptionsConcepts.hpp"
#include "ParameterSchema/ParameterSchema.hpp"

#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class AnalogTimeSeries;

// Custom validator for binary_data_type (the storage format in the binary file)
struct ValidBinaryDataType {
    static constexpr auto valid_types = std::array{"int16", "float32", "int8", "uint16", "float64"};

    static rfl::Result<std::string> validate(std::string const & value) {
        for (auto const & type: valid_types) {
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
    std::string filepath;// Path to the binary file (consistent with DataManager JSON)
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

DATAMANAGERIO_EXPORT std::vector<std::shared_ptr<AnalogTimeSeries>> load(BinaryAnalogLoaderOptions const & opts);

template<>
struct ParameterUIHints<BinaryAnalogLoaderOptions> {
    /// @brief Annotate schema fields for AutoParamWidget (import UI).
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("filepath")) {
            f->tooltip = "Path to the binary file; if relative, it is resolved against parent_dir";
        }
        if (auto * f = schema.field("parent_dir")) {
            f->tooltip = "Base directory used when filepath is not absolute";
        }
        if (auto * f = schema.field("header_size")) {
            f->tooltip = "Number of bytes to skip at the start of the file before sample data";
        }
        if (auto * f = schema.field("num_channels")) {
            f->tooltip = "Number of interleaved channels in the binary file";
        }
        if (auto * f = schema.field("use_memory_mapped")) {
            f->tooltip =
                    "Use memory-mapped I/O for large files (required for offset, stride, sample type, and scaling options)";
        }
        if (auto * f = schema.field("offset")) {
            f->tooltip =
                    "Starting index in the interleaved stream; per-channel start is offset + channel index (memory-mapped path only)";
        }
        if (auto * f = schema.field("stride")) {
            f->tooltip =
                    "Stride between consecutive samples of the same channel in elements; combined with channel count for interleaved data (memory-mapped path)";
        }
        if (auto * f = schema.field("binary_data_type")) {
            f->tooltip = "Binary encoding of each sample on disk";
            f->allowed_values = {"int8", "int16", "uint16", "float32", "float64"};
        }
        if (auto * f = schema.field("scale_factor")) {
            f->tooltip = "Multiply each sample by this value after loading (memory-mapped path)";
        }
        if (auto * f = schema.field("offset_value")) {
            f->tooltip = "Add this value to each sample after scaling (memory-mapped path)";
        }
        if (auto * f = schema.field("num_samples")) {
            f->tooltip = "Maximum samples per channel to expose; 0 means load all available (memory-mapped path)";
        }
    }
};

#endif// ANALOG_TIME_SERIES_LOADER_HPP
