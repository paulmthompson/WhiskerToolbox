/**
 * @file MemoryMappedAnalogDataStorage.hpp
 * @brief Memory-mapped file analog data storage backend.
 */

#ifndef MEMORY_MAPPED_ANALOG_DATA_STORAGE_HPP
#define MEMORY_MAPPED_ANALOG_DATA_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"
#include "MmapAnalogConfig.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

/**
 * @brief Memory-mapped file analog data storage using CRTP.
 *
 * Provides efficient access to large binary files without loading entire dataset
 * into memory. Supports:
 * - Strided access (e.g., reading one channel from interleaved multi-channel data)
 * - Type conversion from various integer/float formats to float32
 * - Scale and offset transformations
 * - Cross-platform memory mapping (POSIX mmap and Windows MapViewOfFile)
 */
class MemoryMappedAnalogDataStorage : public AnalogDataStorageBase<MemoryMappedAnalogDataStorage> {
public:
    /**
     * @brief Construct memory-mapped storage from configuration.
     *
     * @param config Configuration specifying file path, layout, and conversion.
     * @throws std::runtime_error if file cannot be opened or mapped.
     */
    explicit MemoryMappedAnalogDataStorage(MmapStorageConfig config);

    /**
     * @brief Destructor - unmaps file and releases resources.
     */
    ~MemoryMappedAnalogDataStorage();

    // Disable copy (would require complex mapping logic)
    MemoryMappedAnalogDataStorage(MemoryMappedAnalogDataStorage const &) = delete;
    MemoryMappedAnalogDataStorage & operator=(MemoryMappedAnalogDataStorage const &) = delete;

    // Enable move
    MemoryMappedAnalogDataStorage(MemoryMappedAnalogDataStorage &&) noexcept;
    MemoryMappedAnalogDataStorage & operator=(MemoryMappedAnalogDataStorage &&) noexcept;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const;

    [[nodiscard]] std::size_t sizeImpl() const {
        return _num_samples;
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        // Memory-mapped data with stride or type conversion is not contiguous as float.
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(std::size_t /*start*/, std::size_t /*end*/) const {
        // Non-contiguous storage cannot provide spans.
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        // Only contiguous if stride=1 and native float32 (no conversion).
        return _config.stride == 1
               && _config.data_type == MmapDataType::Float32
               && _config.scale_factor == 1.0f
               && _config.offset_value == 0.0f;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::MemoryMapped;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        // Even when logically contiguous, data is not stored as float* directly here.
        return AnalogDataStorageCache{};
    }

    /**
     * @brief Get configuration used for this storage.
     */
    [[nodiscard]] MmapStorageConfig const & getConfig() const {
        return _config;
    }

private:
    void _openAndMapFile();
    void _closeAndUnmap();
    [[nodiscard]] std::size_t _getElementSize() const;
    [[nodiscard]] float _convertToFloat(void const * ptr) const;

    MmapStorageConfig _config;
    std::size_t _num_samples;
    std::size_t _element_size;

#ifdef _WIN32
    HANDLE _file_handle = INVALID_HANDLE_VALUE;
    HANDLE _map_handle = NULL;
    void * _mapped_data = nullptr;
#else
    int _file_descriptor = -1;
    void * _mapped_data = nullptr;
    std::size_t _mapped_size = 0;
#endif
};

#endif// MEMORY_MAPPED_ANALOG_DATA_STORAGE_HPP

