/**
 * @file ViewAnalogDataStorage.hpp
 * @brief View-based analog data storage that references vector storage.
 */

#ifndef VIEW_ANALOG_DATA_STORAGE_HPP
#define VIEW_ANALOG_DATA_STORAGE_HPP

#include "AnalogDataStorageBase.hpp"
#include "VectorAnalogDataStorage.hpp"

#include <memory>
#include <span>

/**
 * @brief View-based analog data storage that references another storage.
 *
 * Holds a shared_ptr to a source VectorAnalogDataStorage and start/end indices
 * into that source. Enables zero-copy filtered views for range queries.
 */
class ViewAnalogDataStorage : public AnalogDataStorageBase<ViewAnalogDataStorage> {
public:
    /**
     * @brief Construct a view referencing source storage.
     *
     * @param source Shared pointer to source vector storage.
     * @param start_index Starting index (inclusive).
     * @param end_index Ending index (exclusive).
     */
    ViewAnalogDataStorage(
            std::shared_ptr<VectorAnalogDataStorage const> source,
            std::size_t start_index,
            std::size_t end_index)
        : _source(std::move(source)),
          _start_index(start_index),
          _end_index(end_index) {
        if (_start_index > _end_index) {
            throw std::invalid_argument("ViewAnalogDataStorage: start_index > end_index");
        }
        if (_end_index > _source->size()) {
            throw std::out_of_range("ViewAnalogDataStorage: end_index exceeds source size");
        }
    }

    ~ViewAnalogDataStorage() = default;

    // CRTP implementation methods

    [[nodiscard]] float getValueAtImpl(std::size_t index) const {
        return _source->getValueAt(_start_index + index);
    }

    [[nodiscard]] std::size_t sizeImpl() const {
        return _end_index - _start_index;
    }

    /**
     * @brief Get span over the view's data range.
     *
     * Since source is contiguous, we can provide efficient span access.
     */
    [[nodiscard]] std::span<float const> getSpanImpl() const {
        return std::span<float const>(
                _source->data() + _start_index,
                _end_index - _start_index);
    }

    [[nodiscard]] std::span<float const> getSpanRangeImpl(std::size_t start, std::size_t end) const {
        if (start > end || end > sizeImpl()) {
            throw std::out_of_range("ViewAnalogDataStorage: invalid range");
        }
        return std::span<float const>(
                _source->data() + _start_index + start,
                end - start);
    }

    [[nodiscard]] bool isContiguousImpl() const {
        return true;
    }

    [[nodiscard]] AnalogStorageType getStorageTypeImpl() const {
        return AnalogStorageType::View;
    }

    [[nodiscard]] AnalogDataStorageCache tryGetCacheImpl() const {
        return AnalogDataStorageCache{
                .data_ptr = _source->data() + _start_index,
                .cache_size = _end_index - _start_index,
                .is_contiguous = _end_index > _start_index};
    }

    /**
     * @brief Get the source storage.
     */
    [[nodiscard]] std::shared_ptr<VectorAnalogDataStorage const> const & source() const {
        return _source;
    }

    /**
     * @brief Get raw pointer to contiguous data.
     */
    [[nodiscard]] float const * data() const {
        return _source->data() + _start_index;
    }

    /**
     * @brief Get the start index in source storage.
     */
    [[nodiscard]] std::size_t startIndex() const { return _start_index; }

    /**
     * @brief Get the end index in source storage.
     */
    [[nodiscard]] std::size_t endIndex() const { return _end_index; }

private:
    std::shared_ptr<VectorAnalogDataStorage const> _source;
    std::size_t _start_index;
    std::size_t _end_index;
};

#endif// VIEW_ANALOG_DATA_STORAGE_HPP

