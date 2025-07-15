#ifndef IROW_SELECTOR_H
#define IROW_SELECTOR_H

#include "TimeFrame.hpp"
#include "DigitalTimeSeries/interval_data.hpp"

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
    virtual ~IRowSelector() = default;
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows.
     */
    virtual size_t getRowCount() const = 0;
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
     */
    explicit TimestampSelector(std::vector<double> timestamps);
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows (size of timestamps vector).
     */
    size_t getRowCount() const override;
    
    /**
     * @brief Gets the timestamps used for row selection.
     * @return Const reference to the timestamps vector.
     */
    const std::vector<double>& getTimestamps() const;

private:
    std::vector<double> m_timestamps;
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
     */
    explicit IntervalSelector(std::vector<TimeFrameInterval> intervals);
    
    /**
     * @brief Gets the total number of rows in the table.
     * @return The number of rows (size of intervals vector).
     */
    size_t getRowCount() const override;
    
    /**
     * @brief Gets the intervals used for row selection.
     * @return Const reference to the intervals vector.
     */
    const std::vector<TimeFrameInterval>& getIntervals() const;

private:
    std::vector<TimeFrameInterval> m_intervals;
};

#endif // IROW_SELECTOR_H
