#ifndef IROW_SELECTOR_H
#define IROW_SELECTOR_H

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "utils/TableView/core/RowDescriptor.h"

#include <cstddef>
#include <vector>

/**
 * @brief Defines the source and number of rows for the table.
 * 
 * This interface defines what constitutes a "row" in the table.
 * Different implementations provide different ways to define rows.
 */
class IRowSelector {
public:
    virtual ~IRowSelector();
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows.
     */
    virtual size_t getRowCount() const = 0;
    
    /**
     * @brief Gets a descriptor containing the source information for a given row index.
     * 
     * This method provides reverse lookup capability, allowing clients to trace
     * a row back to its original source definition (e.g., timestamp, interval).
     * 
     * @param row_index The index of the row to get the descriptor for.
     * @return RowDescriptor containing the source information for the row.
     */
    virtual RowDescriptor getDescriptor(size_t row_index) const = 0;
};

/**
 * @brief Row selector that uses explicit indices.
 * 
 * This implementation defines rows based on a vector of indices.
 */
class IndexSelector : public IRowSelector {
public:
    /**
     * @brief Constructs an IndexSelector with the given indices.
     * @param indices Vector of indices to use for row selection.
     */
    explicit IndexSelector(std::vector<size_t> indices);
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows (size of indices vector).
     */
    size_t getRowCount() const override;
    
    /**
     * @brief Gets a descriptor for the specified row index.
     * @param row_index The index of the row to get the descriptor for.
     * @return RowDescriptor containing the source index for the row.
     */
    RowDescriptor getDescriptor(size_t row_index) const override;
    
    /**
     * @brief Gets the indices used for row selection.
     * @return Const reference to the indices vector.
     */
    const std::vector<size_t>& getIndices() const;

private:
    std::vector<size_t> m_indices;
};

/**
 * @brief Row selector that uses timestamps.
 * 
 * This implementation defines rows based on a vector of timestamps.
 */
class TimestampSelector : public IRowSelector {
public:
    /**
     * @brief Constructs a TimestampSelector with the given timestamps.
     * @param timestamps Vector of timestamps to use for row selection.
     * @param timeFrame Shared pointer to the TimeFrame object that provides the mapping between timestamps and indices.
     */
    explicit TimestampSelector(std::vector<TimeFrameIndex> timestamps, std::shared_ptr<TimeFrame> timeFrame);
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows (size of timestamps vector).
     */
    size_t getRowCount() const override;
    
    /**
     * @brief Gets a descriptor for the specified row index.
     * @param row_index The index of the row to get the descriptor for.
     * @return RowDescriptor containing the source timestamp for the row.
     */
    RowDescriptor getDescriptor(size_t row_index) const override;
    
    /**
     * @brief Gets the timestamps used for row selection.
     * @return Const reference to the timestamps vector.
     */
    const std::vector<TimeFrameIndex>& getTimestamps() const;

    /**
     * @brief Gets the TimeFrame key used for this selector.
     * @return Shared pointer to the TimeFrame object.
     */
    std::shared_ptr<TimeFrame> getTimeFrame() const {
        return m_timeFrame;
    }

private:
    std::vector<TimeFrameIndex> m_timestamps;
    std::shared_ptr<TimeFrame> m_timeFrame;
};

/**
 * @brief Row selector that uses intervals.
 * 
 * This implementation defines rows based on a vector of TimeFrameInterval objects.
 */
class IntervalSelector : public IRowSelector {
public:
    /**
     * @brief Constructs an IntervalSelector with the given intervals.
     * @param intervals Vector of TimeFrameInterval objects to use for row selection.
     * @param timeFrame Shared pointer to the TimeFrame object that provides the mapping between intervals and indices.
     */
    explicit IntervalSelector(std::vector<TimeFrameInterval> intervals, std::shared_ptr<TimeFrame> timeFrame);

    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows (size of intervals vector).
     */
    size_t getRowCount() const override;
    
    /**
     * @brief Gets a descriptor for the specified row index.
     * @param row_index The index of the row to get the descriptor for.
     * @return RowDescriptor containing the source interval for the row.
     */
    RowDescriptor getDescriptor(size_t row_index) const override;
    
    /**
     * @brief Gets the intervals used for row selection.
     * @return Const reference to the intervals vector.
     */
    const std::vector<TimeFrameInterval>& getIntervals() const;

    /**
     * @brief Gets the TimeFrame key used for this selector.
     * @return Shared pointer to the TimeFrame object.
     */
    std::shared_ptr<TimeFrame> getTimeFrame() const {
        return m_timeFrame;
    }

private:
    std::vector<TimeFrameInterval> m_intervals;
    std::shared_ptr<TimeFrame> m_timeFrame;
};

#endif // IROW_SELECTOR_H
