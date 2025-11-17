#ifndef TIME_INDEX_STORAGE_HPP
#define TIME_INDEX_STORAGE_HPP

#include "TimeFrame.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

/**
 * @brief Abstract base class for time index storage strategies
 * 
 * TimeIndexStorage provides a mapping from logical array positions (0, 1, 2, ...)
 * to TimeFrameIndex values. Different implementations can optimize storage based
 * on data characteristics (dense/consecutive vs sparse/irregular sampling).
 * 
 * This abstraction allows multiple data series to share the same time base
 * (e.g., multi-channel electrophysiology recordings) and supports future
 * extensions like memory-mapped or calculated time indices.
 */
class TimeIndexStorage {
public:
    virtual ~TimeIndexStorage() = default;

    /**
     * @brief Get the TimeFrameIndex at a specific array position
     * 
     * @param array_position The logical position in the data array (0-based)
     * @return The TimeFrameIndex corresponding to that position
     * @throws std::out_of_range if array_position is out of bounds
     */
    [[nodiscard]] virtual TimeFrameIndex getTimeFrameIndexAt(size_t array_position) const = 0;

    /**
     * @brief Get the total number of time points stored
     * 
     * @return The count of time indices
     */
    [[nodiscard]] virtual size_t size() const = 0;

    /**
     * @brief Find the array position for a specific TimeFrameIndex
     * 
     * @param time_index The TimeFrameIndex to search for
     * @return Optional array position if found, nullopt otherwise
     */
    [[nodiscard]] virtual std::optional<size_t> findArrayPositionForTimeIndex(TimeFrameIndex time_index) const = 0;

    /**
     * @brief Find array position for the smallest TimeFrameIndex >= target
     * 
     * @param target_time The target TimeFrameIndex
     * @return Optional array position, or nullopt if no such index exists
     */
    [[nodiscard]] virtual std::optional<size_t> findArrayPositionGreaterOrEqual(TimeFrameIndex target_time) const = 0;

    /**
     * @brief Find array position for the largest TimeFrameIndex <= target
     * 
     * @param target_time The target TimeFrameIndex
     * @return Optional array position, or nullopt if no such index exists
     */
    [[nodiscard]] virtual std::optional<size_t> findArrayPositionLessOrEqual(TimeFrameIndex target_time) const = 0;

    /**
     * @brief Get all time indices as a vector
     * 
     * @return Vector containing all TimeFrameIndex values
     * @note For dense storage, this generates the vector on-demand
     */
    [[nodiscard]] virtual std::vector<TimeFrameIndex> getAllTimeIndices() const = 0;

    /**
     * @brief Clone this storage (for copy operations)
     * 
     * @return A new shared_ptr to a copy of this storage
     */
    [[nodiscard]] virtual std::shared_ptr<TimeIndexStorage> clone() const = 0;
};

/**
 * @brief Dense time index storage for consecutive, regularly-sampled data
 * 
 * Memory-efficient representation for time series with consecutive indices:
 * start, start+1, start+2, ..., start+count-1
 * 
 * This is ideal for uniformly sampled data where time indices form a continuous sequence.
 */
class DenseTimeIndexStorage : public TimeIndexStorage {
public:
    /**
     * @brief Construct dense storage
     * 
     * @param start_index The first TimeFrameIndex in the sequence
     * @param count The number of consecutive indices
     */
    DenseTimeIndexStorage(TimeFrameIndex start_index, size_t count);

    [[nodiscard]] TimeFrameIndex getTimeFrameIndexAt(size_t array_position) const override;
    [[nodiscard]] size_t size() const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionForTimeIndex(TimeFrameIndex time_index) const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionGreaterOrEqual(TimeFrameIndex target_time) const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionLessOrEqual(TimeFrameIndex target_time) const override;
    [[nodiscard]] std::vector<TimeFrameIndex> getAllTimeIndices() const override;
    [[nodiscard]] std::shared_ptr<TimeIndexStorage> clone() const override;

    // Accessors for the underlying representation
    [[nodiscard]] TimeFrameIndex getStartIndex() const { return _start_index; }
    [[nodiscard]] size_t getCount() const { return _count; }

private:
    TimeFrameIndex _start_index;
    size_t _count;
};

/**
 * @brief Sparse time index storage for irregularly-sampled data
 * 
 * Explicit storage of time indices for each sample point.
 * This is necessary when time points are not consecutive or uniformly spaced.
 * 
 * Indices are kept sorted to enable efficient binary search operations.
 */
class SparseTimeIndexStorage : public TimeIndexStorage {
public:
    /**
     * @brief Construct sparse storage
     * 
     * @param time_indices Vector of TimeFrameIndex values (will be sorted if not already)
     */
    explicit SparseTimeIndexStorage(std::vector<TimeFrameIndex> time_indices);

    [[nodiscard]] TimeFrameIndex getTimeFrameIndexAt(size_t array_position) const override;
    [[nodiscard]] size_t size() const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionForTimeIndex(TimeFrameIndex time_index) const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionGreaterOrEqual(TimeFrameIndex target_time) const override;
    [[nodiscard]] std::optional<size_t> findArrayPositionLessOrEqual(TimeFrameIndex target_time) const override;
    [[nodiscard]] std::vector<TimeFrameIndex> getAllTimeIndices() const override;
    [[nodiscard]] std::shared_ptr<TimeIndexStorage> clone() const override;

    // Accessor for the underlying vector
    [[nodiscard]] std::vector<TimeFrameIndex> const & getTimeIndices() const { return _time_indices; }

private:
    std::vector<TimeFrameIndex> _time_indices;
};

/**
 * @brief Factory functions for creating appropriate storage
 */
namespace TimeIndexStorageFactory {

/**
 * @brief Create storage from a vector of time indices
 * 
 * Automatically chooses DenseTimeIndexStorage if indices are consecutive,
 * otherwise uses SparseTimeIndexStorage.
 * 
 * @param time_indices Vector of TimeFrameIndex values
 * @return Shared pointer to the appropriate storage implementation
 */
std::shared_ptr<TimeIndexStorage> createFromTimeIndices(std::vector<TimeFrameIndex> time_indices);

/**
 * @brief Create dense storage for consecutive indices starting from 0
 * 
 * @param count Number of consecutive indices
 * @return Shared pointer to DenseTimeIndexStorage
 */
std::shared_ptr<TimeIndexStorage> createDenseFromZero(size_t count);

/**
 * @brief Create dense storage for consecutive indices starting from a specific index
 * 
 * @param start_index First TimeFrameIndex in the sequence
 * @param count Number of consecutive indices
 * @return Shared pointer to DenseTimeIndexStorage
 */
std::shared_ptr<TimeIndexStorage> createDense(TimeFrameIndex start_index, size_t count);

}// namespace TimeIndexStorageFactory

#endif// TIME_INDEX_STORAGE_HPP
