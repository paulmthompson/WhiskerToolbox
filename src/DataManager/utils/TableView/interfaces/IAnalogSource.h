#ifndef IANALOG_SOURCE_H
#define IANALOG_SOURCE_H

#include "TimeFrame/TimeFrame.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

/**
 * @brief Interface for any data source that can be viewed as an analog signal.
 * 
 * This abstract base class provides a common interface for any data that can be
 * treated as a simple array of doubles. Implementations may represent physical
 * data (like AnalogData) or virtual data (like PointComponentAdapter).
 */
class IAnalogSource {
public:
    virtual ~IAnalogSource() = default;

    /**
     * @brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    virtual std::string const & getName() const = 0;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    virtual std::shared_ptr<TimeFrame> getTimeFrame() const = 0;

    /**
     * @brief Gets the total number of samples in the source.
     * @return The number of samples.
     */
    virtual size_t size() const = 0;

    /**
     * @brief Gets the data within a specific time range.
     * 
     * This gets the data in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of floats representing the data in the specified range.
     */
    virtual std::vector<float> getDataInRange(TimeFrameIndex start,
                                              TimeFrameIndex end,
                                              TimeFrame const * target_timeFrame) = 0;
};

#endif// IANALOG_SOURCE_H
