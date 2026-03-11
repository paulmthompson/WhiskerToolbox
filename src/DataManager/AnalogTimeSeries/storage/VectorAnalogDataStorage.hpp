/**
 * @file VectorAnalogDataStorage.hpp
 * @brief Contiguous in-memory analog data storage backend.
 */

#ifndef VECTOR_ANALOG_DATA_STORAGE_HPP
#define VECTOR_ANALOG_DATA_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"

#include <span>
#include <vector>

/**
 * @brief Contiguous vector-based analog data storage using CRTP.
 *
 * High-performance implementation for in-memory contiguous data.
 * Provides zero-overhead access when the concrete type is known at
 * compile time and efficient span-based operations for contiguous data.
 */
class VectorAnalogDataStorage : public AnalogDataStorageBase<VectorAnalogDataStorage> {
public:
    /**
     * @brief Construct storage from a vector of float values.
     *
     * @param data Vector of analog data values (moved into storage).
     */
    explicit VectorAnalogDataStorage(std::vector<float> data)
        : _data(std::move(data)) {}

    ~VectorAnalogDataStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const {
        return _data[index];
    }

    [[nodiscard]] std::size_t sizeImpl() const {
        return _data.size();
    }

    [[nodiscard]] std::span<float const> getSpanImpl() const {
        return _data;
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(std::size_t start, std::size_t end) const {
        if (start >= end || end > _data.size()) {
            return {};
        }
        return std::span<float const>(_data.data() + start, end - start);
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return true;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::Vector;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        return AnalogDataStorageCache{
                .data_ptr = _data.data(),
                .cache_size = _data.size(),
                .is_contiguous = !_data.empty()};
    }

    // Direct access methods for specific optimizations

    /**
     * @brief Get direct access to the underlying vector.
     */
    [[nodiscard]] std::vector<float> const & getVector() const {
        return _data;
    }

    /**
     * @brief Get raw pointer to contiguous data.
     */
    [[nodiscard]] float const * data() const {
        return _data.data();
    }

private:
    std::vector<float> _data;
};

#endif// VECTOR_ANALOG_DATA_STORAGE_HPP

