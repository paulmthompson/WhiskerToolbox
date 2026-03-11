/**
 * @file MmapAnalogConfig.hpp
 * @brief Shared configuration types for memory-mapped analog data storage.
 */

#ifndef MMAP_ANALOG_CONFIG_HPP
#define MMAP_ANALOG_CONFIG_HPP

#include <cstddef>
#include <filesystem>

/**
 * @brief Type conversion strategies for memory-mapped analog data.
 */
enum class MmapDataType {
    Float32,// 32-bit floating point (no conversion needed)
    Float64,// 64-bit floating point (double)
    Int8,   // 8-bit signed integer
    UInt8,  // 8-bit unsigned integer
    Int16,  // 16-bit signed integer
    UInt16, // 16-bit unsigned integer
    Int32,  // 32-bit signed integer
    UInt32  // 32-bit unsigned integer
};

/**
 * @brief Configuration for memory-mapped analog data storage.
 *
 * Shared between `MemoryMappedAnalogDataStorage` and higher-level APIs such as
 * `AnalogTimeSeries::createMemoryMapped`.
 */
struct MmapStorageConfig {
    std::filesystem::path file_path;               ///< Path to binary data file
    std::size_t header_size = 0;                   ///< Bytes to skip at file start
    std::size_t offset = 0;                        ///< Sample offset within data region
    std::size_t stride = 1;                        ///< Stride between samples (in elements, not bytes)
    std::size_t num_samples = 0;                   ///< Number of samples to read (0 = auto-detect)
    MmapDataType data_type = MmapDataType::Float32;///< Underlying data type
    float scale_factor = 1.0f;                     ///< Multiplicative scale for conversion
    float offset_value = 0.0f;                     ///< Additive offset for conversion
};

#endif// MMAP_ANALOG_CONFIG_HPP

