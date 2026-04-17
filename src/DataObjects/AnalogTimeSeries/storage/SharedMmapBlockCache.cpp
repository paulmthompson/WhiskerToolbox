/**
 * @file SharedMmapBlockCache.cpp
 * @brief Implementation of SharedMmapBlockCache.
 */

#include "SharedMmapBlockCache.hpp"

#include <algorithm>
#include <cassert>
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

// ============================================================================
// Platform-specific mmap state (opaque in header)
// ============================================================================

#ifdef _WIN32
struct SharedMmapBlockCache::PlatformMmapState {
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    HANDLE map_handle = NULL;
    void * mapped_data = nullptr;
};
#else
struct SharedMmapBlockCache::PlatformMmapState {
    int file_descriptor = -1;
    void * mapped_data = nullptr;
    std::size_t mapped_size = 0;
};
#endif

// ============================================================================
// Construction / Destruction
// ============================================================================

SharedMmapBlockCache::SharedMmapBlockCache(SharedMmapBlockCacheConfig config)
    : _config(std::move(config)),
      _platform(std::make_unique<PlatformMmapState>()) {
    assert(_config.num_channels >= 1 && "SharedMmapBlockCache: num_channels must be >= 1");
    assert(_config.block_size_samples >= 1 && "SharedMmapBlockCache: block_size_samples must be >= 1");
    assert(_config.max_cached_blocks >= 1 && "SharedMmapBlockCache: max_cached_blocks must be >= 1");

    if (!std::filesystem::exists(_config.file_path)) {
        throw std::runtime_error(
                "SharedMmapBlockCache: file does not exist: " + _config.file_path.string());
    }

    _element_size = _getElementSize();
    _openAndMapFile();

    // Calculate number of samples per channel from file size
    auto const file_size = std::filesystem::file_size(_config.file_path);
    auto const data_size = file_size - _config.header_size;
    auto const total_elements = data_size / _element_size;
    _num_samples_per_channel = total_elements / _config.num_channels;

    // Total number of blocks
    _total_blocks = (_num_samples_per_channel + _config.block_size_samples - 1) /
                    _config.block_size_samples;

    // Initialize cache slots
    auto const slots = std::min(_config.max_cached_blocks, _total_blocks);
    _cache.resize(slots);
    for (auto & block: _cache) {
        block.data.resize(_config.block_size_samples * _config.num_channels, 0.0f);
    }
}

SharedMmapBlockCache::~SharedMmapBlockCache() {
    _closeAndUnmap();
}

// ============================================================================
// Public API
// ============================================================================

float SharedMmapBlockCache::getValue(std::size_t time_sample, std::size_t channel) const {
    assert(time_sample < _num_samples_per_channel);
    assert(channel < _config.num_channels);

    std::size_t const block_index = time_sample / _config.block_size_samples;
    std::size_t const offset_in_block = time_sample % _config.block_size_samples;

    std::lock_guard<std::mutex> const lock(_mutex);
    auto const & block = _getOrLoadBlock(block_index);
    return block.data[channel * _config.block_size_samples + offset_in_block];
}

std::span<float const> SharedMmapBlockCache::getChannelBlockSpan(
        std::size_t channel,
        std::size_t start_sample,
        std::size_t max_count) const {
    assert(channel < _config.num_channels);
    assert(start_sample < _num_samples_per_channel);

    std::size_t const block_index = start_sample / _config.block_size_samples;
    std::size_t const offset_in_block = start_sample % _config.block_size_samples;

    std::lock_guard<std::mutex> const lock(_mutex);
    auto const & block = _getOrLoadBlock(block_index);

    // How many samples are available in this block from offset_in_block?
    std::size_t const actual_size = _actualBlockSize(block_index);
    std::size_t const available = actual_size - offset_in_block;
    std::size_t const count = std::min(max_count, available);

    float const * ptr = block.data.data() + channel * _config.block_size_samples + offset_in_block;
    return {ptr, count};
}

// ============================================================================
// Cache Management
// ============================================================================

SharedMmapBlockCache::CachedBlock const &
SharedMmapBlockCache::_getOrLoadBlock(std::size_t block_index) const {
    // Linear scan for cache hit (OK for small max_cached_blocks, typically 8-16)
    for (auto const & slot: _cache) {
        if (slot.block_index == block_index) {
            return slot;
        }
    }

    // Cache miss — evict and load
    auto & victim = _cache[_next_evict];
    _next_evict = (_next_evict + 1) % _cache.size();
    _loadBlock(victim, block_index);
    return victim;
}

void SharedMmapBlockCache::_loadBlock(CachedBlock & block, std::size_t block_index) const {
    block.block_index = block_index;

    std::size_t const block_start_sample = block_index * _config.block_size_samples;
    std::size_t const actual_size = _actualBlockSize(block_index);

    auto const * base_ptr = static_cast<char const *>(_platform->mapped_data);

    // Decode interleaved data into column-major float buffer
    for (std::size_t t = 0; t < actual_size; ++t) {
        std::size_t const global_sample = block_start_sample + t;
        for (std::size_t ch = 0; ch < _config.num_channels; ++ch) {
            std::size_t const element_index = global_sample * _config.num_channels + ch;
            std::size_t const byte_offset = _config.header_size + element_index * _element_size;
            void const * data_ptr = base_ptr + byte_offset;

            float value = _convertToFloat(data_ptr);
            value = value * _config.scale_factor + _config.offset_value;

            block.data[ch * _config.block_size_samples + t] = value;
        }
    }

    // Zero-fill remainder of last block (if partial)
    if (actual_size < _config.block_size_samples) {
        for (std::size_t ch = 0; ch < _config.num_channels; ++ch) {
            std::size_t const start = ch * _config.block_size_samples + actual_size;
            std::size_t const end = (ch + 1) * _config.block_size_samples;
            std::fill(block.data.begin() + static_cast<long>(start),
                      block.data.begin() + static_cast<long>(end),
                      0.0f);
        }
    }
}

std::size_t SharedMmapBlockCache::_actualBlockSize(std::size_t block_index) const {
    std::size_t const block_start = block_index * _config.block_size_samples;
    return std::min(_config.block_size_samples, _num_samples_per_channel - block_start);
}

// ============================================================================
// Type Conversion
// ============================================================================

float SharedMmapBlockCache::_convertToFloat(void const * ptr) const {
    switch (_config.data_type) {
        case MmapDataType::Float32: {
            float value;
            std::memcpy(&value, ptr, sizeof(float));
            return value;
        }
        case MmapDataType::Float64: {
            double value;
            std::memcpy(&value, ptr, sizeof(double));
            return static_cast<float>(value);
        }
        case MmapDataType::Int8: {
            int8_t value;
            std::memcpy(&value, ptr, sizeof(int8_t));
            return static_cast<float>(value);
        }
        case MmapDataType::UInt8: {
            uint8_t value;
            std::memcpy(&value, ptr, sizeof(uint8_t));
            return static_cast<float>(value);
        }
        case MmapDataType::Int16: {
            int16_t value;
            std::memcpy(&value, ptr, sizeof(int16_t));
            return static_cast<float>(value);
        }
        case MmapDataType::UInt16: {
            uint16_t value;
            std::memcpy(&value, ptr, sizeof(uint16_t));
            return static_cast<float>(value);
        }
        case MmapDataType::Int32: {
            int32_t value;
            std::memcpy(&value, ptr, sizeof(int32_t));
            return static_cast<float>(value);
        }
        case MmapDataType::UInt32: {
            uint32_t value;
            std::memcpy(&value, ptr, sizeof(uint32_t));
            return static_cast<float>(value);
        }
        default:
            throw std::runtime_error("SharedMmapBlockCache: unknown data type");
    }
}

std::size_t SharedMmapBlockCache::_getElementSize() const {
    switch (_config.data_type) {
        case MmapDataType::Float32:
            return sizeof(float);
        case MmapDataType::Float64:
            return sizeof(double);
        case MmapDataType::Int8:
            return sizeof(int8_t);
        case MmapDataType::UInt8:
            return sizeof(uint8_t);
        case MmapDataType::Int16:
            return sizeof(int16_t);
        case MmapDataType::UInt16:
            return sizeof(uint16_t);
        case MmapDataType::Int32:
            return sizeof(int32_t);
        case MmapDataType::UInt32:
            return sizeof(uint32_t);
        default:
            throw std::runtime_error("SharedMmapBlockCache: unknown data type");
    }
}

// ============================================================================
// Memory Mapping (Platform-Specific)
// ============================================================================

void SharedMmapBlockCache::_openAndMapFile() {
#ifdef _WIN32
    _platform->file_handle = CreateFileW(
            _config.file_path.wstring().c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (_platform->file_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to open file: " + _config.file_path.string());
    }

    _platform->map_handle = CreateFileMappingW(_platform->file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (_platform->map_handle == NULL) {
        CloseHandle(_platform->file_handle);
        _platform->file_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to create file mapping: " + _config.file_path.string());
    }

    _platform->mapped_data = MapViewOfFile(_platform->map_handle, FILE_MAP_READ, 0, 0, 0);
    if (_platform->mapped_data == nullptr) {
        CloseHandle(_platform->map_handle);
        CloseHandle(_platform->file_handle);
        _platform->map_handle = NULL;
        _platform->file_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to map view: " + _config.file_path.string());
    }
#else
    _platform->file_descriptor = open(_config.file_path.c_str(), O_RDONLY);
    if (_platform->file_descriptor == -1) {
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to open file: " + _config.file_path.string());
    }

    struct stat sb {};
    if (fstat(_platform->file_descriptor, &sb) == -1) {
        close(_platform->file_descriptor);
        _platform->file_descriptor = -1;
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to stat file: " + _config.file_path.string());
    }

    _platform->mapped_size = static_cast<std::size_t>(sb.st_size);
    _platform->mapped_data = mmap(nullptr, _platform->mapped_size, PROT_READ, MAP_PRIVATE, _platform->file_descriptor, 0);

    if (_platform->mapped_data == MAP_FAILED) {
        close(_platform->file_descriptor);
        _platform->file_descriptor = -1;
        _platform->mapped_data = nullptr;
        throw std::runtime_error(
                "SharedMmapBlockCache: failed to mmap file: " + _config.file_path.string());
    }
#endif
}

void SharedMmapBlockCache::_closeAndUnmap() {
#ifdef _WIN32
    if (_platform->mapped_data != nullptr) {
        UnmapViewOfFile(_platform->mapped_data);
        _platform->mapped_data = nullptr;
    }
    if (_platform->map_handle != NULL) {
        CloseHandle(_platform->map_handle);
        _platform->map_handle = NULL;
    }
    if (_platform->file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(_platform->file_handle);
        _platform->file_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (_platform->mapped_data != nullptr && _platform->mapped_data != MAP_FAILED) {
        munmap(_platform->mapped_data, _platform->mapped_size);
        _platform->mapped_data = nullptr;
    }
    if (_platform->file_descriptor != -1) {
        close(_platform->file_descriptor);
        _platform->file_descriptor = -1;
    }
#endif
}
