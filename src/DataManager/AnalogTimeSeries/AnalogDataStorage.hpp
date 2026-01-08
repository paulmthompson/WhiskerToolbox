#ifndef ANALOG_DATA_STORAGE_HPP
#define ANALOG_DATA_STORAGE_HPP

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ABSOLUTE//In some analog transforms
#undef ABSOLUTE
#endif
#ifdef IGNORE// in parameters
#undef IGNORE
#endif
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/**
 * @brief Storage type enumeration for runtime type identification
 */
enum class AnalogStorageType {
    Vector,
    MemoryMapped,
    LazyView,// View-based lazy storage
    Custom
};

/**
 * @brief CRTP base class for analog data storage strategies
 * 
 * Uses Curiously Recurring Template Pattern to eliminate virtual function overhead
 * while maintaining a polymorphic interface. Derived classes implement the actual
 * storage strategy (contiguous vector, memory-mapped file, etc.).
 * 
 * @tparam Derived The concrete storage implementation type
 */
template<typename Derived>
class AnalogDataStorageBase {
public:
    /**
     * @brief Get the value at a specific index using CRTP dispatch
     * 
     * Zero-overhead call to derived implementation. The compiler fully inlines
     * this when the type is known at compile time.
     * 
     * @param index The index to access
     * @return float value at the given index
     */
    [[nodiscard]] float getValueAt(size_t index) const {
        return static_cast<Derived const *>(this)->getValueAtImpl(index);
    }

    /**
     * @brief Get the number of samples using CRTP dispatch
     * 
     * @return size_t number of samples in storage
     */
    [[nodiscard]] size_t size() const {
        return static_cast<Derived const *>(this)->sizeImpl();
    }

    /**
     * @brief Get a span over all data (if contiguous) using CRTP dispatch
     * 
     * Returns a non-empty span only if data is stored contiguously in memory.
     * For non-contiguous storage (e.g., strided memory-mapped files), returns empty span.
     * 
     * @return std::span<float const> view over contiguous data, or empty span
     */
    [[nodiscard]] std::span<float const> getSpan() const {
        return static_cast<Derived const *>(this)->getSpanImpl();
    }

    /**
     * @brief Get a span over a range of data (if contiguous)
     * 
     * @param start Starting index (inclusive)
     * @param end Ending index (exclusive)
     * @return std::span<float const> view over the range, or empty span if not contiguous
     */
    [[nodiscard]] std::span<float const> getSpanRange(size_t start, size_t end) const {
        return static_cast<Derived const *>(this)->getSpanRangeImpl(start, end);
    }

    /**
     * @brief Check if data is stored contiguously in memory
     * 
     * @return true if data is contiguous (allows span access and pointer arithmetic)
     * @return false if data is non-contiguous (requires indexed access)
     */
    [[nodiscard]] bool isContiguous() const {
        return static_cast<Derived const *>(this)->isContiguousImpl();
    }

    /**
     * @brief Get the storage type identifier
     * 
     * Pure virtual to require implementation. Used for runtime type identification
     * when storage is accessed through type-erased interface.
     * 
     * @return AnalogStorageType identifying the concrete storage type
     */
    [[nodiscard]] virtual AnalogStorageType getStorageType() const = 0;

protected:
    // Protected destructor prevents polymorphic deletion through base pointer
    ~AnalogDataStorageBase() = default;
};

/**
 * @brief Contiguous vector-based analog data storage using CRTP
 * 
 * High-performance implementation for in-memory contiguous data.
 * Provides zero-overhead access when type is known at compile time,
 * and efficient span-based operations for contiguous data access.
 */
class VectorAnalogDataStorage : public AnalogDataStorageBase<VectorAnalogDataStorage> {
public:
    /**
     * @brief Construct storage from a vector of float values
     * 
     * @param data Vector of analog data values (moved into storage)
     */
    explicit VectorAnalogDataStorage(std::vector<float> data)
        : _data(std::move(data)) {}

    virtual ~VectorAnalogDataStorage() = default;

    // CRTP implementation methods (called by base class)

    [[nodiscard]] float getValueAtImpl(size_t index) const {
        return _data[index];
    }

    [[nodiscard]] size_t sizeImpl() const {
        return _data.size();
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        return _data;
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(size_t start, size_t end) const {
        if (start >= end || end > _data.size()) {
            return {};
        }
        return std::span<float const>(_data.data() + start, end - start);
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return true;
    }

    [[nodiscard]] AnalogStorageType getStorageType() const override {
        return AnalogStorageType::Vector;
    }

    // Direct access methods for specific optimizations

    /**
     * @brief Get direct access to underlying vector
     * 
     * Use with caution - bypasses abstraction. Useful for specific algorithms
     * that need to work with std::vector directly.
     * 
     * @return const reference to the underlying std::vector<float>
     */
    [[nodiscard]] std::vector<float> const & getVector() const {
        return _data;
    }

    /**
     * @brief Get raw pointer to contiguous data
     * 
     * Enables maximum performance for pointer-based algorithms.
     * 
     * @return const pointer to first element
     */
    [[nodiscard]] float const * data() const {
        return _data.data();
    }

private:
    std::vector<float> _data;
};

/**
 * @brief Type conversion strategies for memory-mapped data
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
 * @brief Configuration for memory-mapped analog data storage
 */
struct MmapStorageConfig {
    std::filesystem::path file_path;               ///< Path to binary data file
    size_t header_size = 0;                        ///< Bytes to skip at file start
    size_t offset = 0;                             ///< Sample offset within data region
    size_t stride = 1;                             ///< Stride between samples (in elements, not bytes)
    size_t num_samples = 0;                        ///< Number of samples to read (0 = auto-detect)
    MmapDataType data_type = MmapDataType::Float32;///< Underlying data type
    float scale_factor = 1.0f;                     ///< Multiplicative scale for conversion
    float offset_value = 0.0f;                     ///< Additive offset for conversion
};

/**
 * @brief Memory-mapped file analog data storage using CRTP
 * 
 * Provides efficient access to large binary files without loading entire dataset
 * into memory. Supports:
 * - Strided access (e.g., reading one channel from interleaved multi-channel data)
 * - Type conversion from various integer/float formats to float32
 * - Scale and offset transformations
 * - Cross-platform memory mapping (POSIX mmap and Windows MapViewOfFile)
 * 
 * Example use case: 384-channel electrophysiology data stored as int16
 * with channels interleaved - can efficiently access a single channel.
 */
class MemoryMappedAnalogDataStorage : public AnalogDataStorageBase<MemoryMappedAnalogDataStorage> {
public:
    /**
     * @brief Construct memory-mapped storage from configuration
     * 
     * @param config Configuration specifying file path, layout, and conversion
     * @throws std::runtime_error if file cannot be opened or mapped
     */
    explicit MemoryMappedAnalogDataStorage(MmapStorageConfig config);

    /**
     * @brief Destructor - unmaps file and releases resources
     */
    virtual ~MemoryMappedAnalogDataStorage();

    // Disable copy (would require complex mapping logic)
    MemoryMappedAnalogDataStorage(MemoryMappedAnalogDataStorage const &) = delete;
    MemoryMappedAnalogDataStorage & operator=(MemoryMappedAnalogDataStorage const &) = delete;

    // Enable move
    MemoryMappedAnalogDataStorage(MemoryMappedAnalogDataStorage &&) noexcept;
    MemoryMappedAnalogDataStorage & operator=(MemoryMappedAnalogDataStorage &&) noexcept;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(size_t index) const;

    [[nodiscard]] size_t sizeImpl() const {
        return _num_samples;
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        // Memory-mapped data with stride or type conversion is not contiguous as float
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(size_t start, size_t end) const {
        // Non-contiguous storage cannot provide spans
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        // Only contiguous if stride=1 and native float32 (no conversion)
        return _config.stride == 1 && _config.data_type == MmapDataType::Float32 && _config.scale_factor == 1.0f && _config.offset_value == 0.0f;
    }

    [[nodiscard]] AnalogStorageType getStorageType() const override {
        return AnalogStorageType::MemoryMapped;
    }

    /**
     * @brief Get configuration used for this storage
     */
    [[nodiscard]] MmapStorageConfig const & getConfig() const {
        return _config;
    }

private:
    void _openAndMapFile();
    void _closeAndUnmap();
    [[nodiscard]] size_t _getElementSize() const;
    [[nodiscard]] float _convertToFloat(void const * ptr) const;

    MmapStorageConfig _config;
    size_t _num_samples;
    size_t _element_size;

#ifdef _WIN32
    HANDLE _file_handle = INVALID_HANDLE_VALUE;
    HANDLE _map_handle = NULL;
    void * _mapped_data = nullptr;
#else
    int _file_descriptor = -1;
    void * _mapped_data = nullptr;
    size_t _mapped_size = 0;
#endif
};

/**
 * @brief Lazy view-based analog data storage
 * 
 * Stores a computation pipeline as a random-access view that transforms data
 * on-demand. Enables efficient composition of transforms without materializing
 * intermediate results. Works with any random-access range that yields float values.
 * 
 * @tparam ViewType Type of the random-access range view
 * 
 * @example Z-score normalization applied lazily:
 * @code
 * auto base_series = ;
 * auto view = base_series->view() 
 *     | std::views::transform([mean, std](auto tv) {
 *         return AnalogTimeSeries::TimeValuePoint{tv.time_frame_index, (tv.value() - mean) / std};
 *     });
 * auto normalized = AnalogTimeSeries::createFromView(view, base_series->getTimeStorage());
 * @endcode
 */
template<typename ViewType>
class LazyViewStorage : public AnalogDataStorageBase<LazyViewStorage<ViewType>> {
public:
    /**
     * @brief Construct lazy storage from a random-access view
     * 
     * @param view Random-access range view (must support operator[])
     * @param num_samples Number of samples (must match view size)
     * 
     * @throws std::invalid_argument if view is not random-access
     */
    explicit LazyViewStorage(ViewType view, size_t num_samples)
        : _view(std::move(view))
        , _num_samples(num_samples) {
        static_assert(std::ranges::random_access_range<ViewType>,
                      "LazyViewStorage requires random access range");
    }

    virtual ~LazyViewStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(size_t index) const {
        auto element = _view[index];

        // Support both TimeValuePoint (with value() method) and std::pair<TimeFrameIndex, float>
        if constexpr (requires { element.value(); }) {
            return element.value();// TimeValuePoint
        } else {
            return element.second;// std::pair
        }
    }

    [[nodiscard]] size_t sizeImpl() const {
        return _num_samples;
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        // Lazy transforms are never contiguous in memory
        return {};
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(size_t start, size_t end) const {
        // Non-contiguous storage cannot provide spans
        return {};
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return false;// Lazy transforms are not stored contiguously
    }

    [[nodiscard]] AnalogStorageType getStorageType() const override {
        return AnalogStorageType::LazyView;
    }

    /**
     * @brief Get reference to underlying view (for advanced use)
     */
    [[nodiscard]] ViewType const & getView() const {
        return _view;
    }

private:
    ViewType _view;
    size_t _num_samples;
};

#endif// ANALOG_DATA_STORAGE_HPP
