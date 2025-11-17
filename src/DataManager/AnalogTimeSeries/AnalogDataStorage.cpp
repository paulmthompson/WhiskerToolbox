#include "AnalogDataStorage.hpp"

#include <algorithm>
#include <cmath>

// ============================================================================
// MemoryMappedAnalogDataStorage Implementation
// ============================================================================

MemoryMappedAnalogDataStorage::MemoryMappedAnalogDataStorage(MmapStorageConfig config)
    : _config(std::move(config))
    , _num_samples(0)
    , _element_size(0)
{
    if (!std::filesystem::exists(_config.file_path)) {
        throw std::runtime_error("Memory-mapped file does not exist: " + _config.file_path.string());
    }
    
    _element_size = _getElementSize();
    _openAndMapFile();
    
    // Calculate number of samples if not specified
    if (_config.num_samples == 0) {
        size_t file_size = std::filesystem::file_size(_config.file_path);
        size_t data_size = file_size - _config.header_size;
        size_t total_elements = data_size / _element_size;
        
        // Account for offset and stride
        if (total_elements > _config.offset) {
            size_t available = total_elements - _config.offset;
            _num_samples = (available + _config.stride - 1) / _config.stride;
        } else {
            _num_samples = 0;
        }
    } else {
        _num_samples = _config.num_samples;
    }
    
    // Validate that requested data fits in file
    if (_num_samples > 0) {
        size_t last_element_index = _config.offset + (_num_samples - 1) * _config.stride;
        size_t file_size = std::filesystem::file_size(_config.file_path);
        size_t data_size = file_size - _config.header_size;
        size_t total_elements = data_size / _element_size;
        
        if (last_element_index >= total_elements) {
            throw std::runtime_error(
                "Requested data range exceeds file size. Last element: " + 
                std::to_string(last_element_index) + ", available: " + 
                std::to_string(total_elements));
        }
    }
}

MemoryMappedAnalogDataStorage::~MemoryMappedAnalogDataStorage() {
    _closeAndUnmap();
}

MemoryMappedAnalogDataStorage::MemoryMappedAnalogDataStorage(MemoryMappedAnalogDataStorage&& other) noexcept
    : _config(std::move(other._config))
    , _num_samples(other._num_samples)
    , _element_size(other._element_size)
#ifdef _WIN32
    , _file_handle(other._file_handle)
    , _map_handle(other._map_handle)
    , _mapped_data(other._mapped_data)
#else
    , _file_descriptor(other._file_descriptor)
    , _mapped_data(other._mapped_data)
    , _mapped_size(other._mapped_size)
#endif
{
#ifdef _WIN32
    other._file_handle = INVALID_HANDLE_VALUE;
    other._map_handle = NULL;
    other._mapped_data = nullptr;
#else
    other._file_descriptor = -1;
    other._mapped_data = nullptr;
    other._mapped_size = 0;
#endif
}

MemoryMappedAnalogDataStorage& MemoryMappedAnalogDataStorage::operator=(MemoryMappedAnalogDataStorage&& other) noexcept {
    if (this != &other) {
        _closeAndUnmap();
        
        _config = std::move(other._config);
        _num_samples = other._num_samples;
        _element_size = other._element_size;
        
#ifdef _WIN32
        _file_handle = other._file_handle;
        _map_handle = other._map_handle;
        _mapped_data = other._mapped_data;
        
        other._file_handle = INVALID_HANDLE_VALUE;
        other._map_handle = NULL;
        other._mapped_data = nullptr;
#else
        _file_descriptor = other._file_descriptor;
        _mapped_data = other._mapped_data;
        _mapped_size = other._mapped_size;
        
        other._file_descriptor = -1;
        other._mapped_data = nullptr;
        other._mapped_size = 0;
#endif
    }
    return *this;
}

float MemoryMappedAnalogDataStorage::getValueAtImpl(size_t index) const {
    if (index >= _num_samples) {
        throw std::out_of_range("Index out of range in memory-mapped storage");
    }
    
    // Calculate actual position in file
    size_t element_index = _config.offset + index * _config.stride;
    size_t byte_offset = _config.header_size + element_index * _element_size;
    
    // Get pointer to data
    auto const* base_ptr = static_cast<char const*>(_mapped_data);
    void const* data_ptr = base_ptr + byte_offset;
    
    // Convert to float and apply scale/offset
    float value = _convertToFloat(data_ptr);
    return value * _config.scale_factor + _config.offset_value;
}

void MemoryMappedAnalogDataStorage::_openAndMapFile() {
#ifdef _WIN32
    // Windows implementation using MapViewOfFile
    _file_handle = CreateFileW(
        _config.file_path.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (_file_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file for memory mapping: " + _config.file_path.string());
    }
    
    _map_handle = CreateFileMappingW(
        _file_handle,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    
    if (_map_handle == NULL) {
        CloseHandle(_file_handle);
        _file_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("Failed to create file mapping: " + _config.file_path.string());
    }
    
    _mapped_data = MapViewOfFile(
        _map_handle,
        FILE_MAP_READ,
        0,
        0,
        0
    );
    
    if (_mapped_data == nullptr) {
        CloseHandle(_map_handle);
        CloseHandle(_file_handle);
        _map_handle = NULL;
        _file_handle = INVALID_HANDLE_VALUE;
        throw std::runtime_error("Failed to map view of file: " + _config.file_path.string());
    }
#else
    // POSIX implementation using mmap
    _file_descriptor = open(_config.file_path.c_str(), O_RDONLY);
    
    if (_file_descriptor == -1) {
        throw std::runtime_error("Failed to open file for memory mapping: " + _config.file_path.string());
    }
    
    struct stat sb;
    if (fstat(_file_descriptor, &sb) == -1) {
        close(_file_descriptor);
        _file_descriptor = -1;
        throw std::runtime_error("Failed to get file size: " + _config.file_path.string());
    }
    
    _mapped_size = sb.st_size;
    
    _mapped_data = mmap(
        nullptr,
        _mapped_size,
        PROT_READ,
        MAP_PRIVATE,
        _file_descriptor,
        0
    );
    
    if (_mapped_data == MAP_FAILED) {
        close(_file_descriptor);
        _file_descriptor = -1;
        _mapped_data = nullptr;
        throw std::runtime_error("Failed to memory map file: " + _config.file_path.string());
    }
#endif
}

void MemoryMappedAnalogDataStorage::_closeAndUnmap() {
#ifdef _WIN32
    if (_mapped_data != nullptr) {
        UnmapViewOfFile(_mapped_data);
        _mapped_data = nullptr;
    }
    if (_map_handle != NULL) {
        CloseHandle(_map_handle);
        _map_handle = NULL;
    }
    if (_file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(_file_handle);
        _file_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (_mapped_data != nullptr && _mapped_data != MAP_FAILED) {
        munmap(_mapped_data, _mapped_size);
        _mapped_data = nullptr;
    }
    if (_file_descriptor != -1) {
        close(_file_descriptor);
        _file_descriptor = -1;
    }
#endif
}

size_t MemoryMappedAnalogDataStorage::_getElementSize() const {
    switch (_config.data_type) {
        case MmapDataType::Float32: return sizeof(float);
        case MmapDataType::Float64: return sizeof(double);
        case MmapDataType::Int8:    return sizeof(int8_t);
        case MmapDataType::UInt8:   return sizeof(uint8_t);
        case MmapDataType::Int16:   return sizeof(int16_t);
        case MmapDataType::UInt16:  return sizeof(uint16_t);
        case MmapDataType::Int32:   return sizeof(int32_t);
        case MmapDataType::UInt32:  return sizeof(uint32_t);
        default:
            throw std::runtime_error("Unknown data type");
    }
}

float MemoryMappedAnalogDataStorage::_convertToFloat(void const* ptr) const {
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
            throw std::runtime_error("Unknown data type in conversion");
    }
}
