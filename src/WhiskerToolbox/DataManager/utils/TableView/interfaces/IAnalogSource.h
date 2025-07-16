#ifndef IANALOG_SOURCE_H
#define IANALOG_SOURCE_H

#include "TimeFrame.hpp"

#include <cstddef>
#include <span>
#include <string>

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
    virtual const std::string& getName() const = 0;

    /**
     * @brief Gets the ID of the TimeFrame the data belongs to.
     * @return The TimeFrame ID as an integer.
     */
    virtual int getTimeFrameId() const = 0;
    
    /**
     * @brief Gets the total number of samples in the source.
     * @return The number of samples.
     */
    virtual size_t size() const = 0;

    /**
     * @brief Provides a view over the data.
     * 
     * This may trigger a one-time lazy materialization for non-contiguous sources.
     * @return A span over the data as const doubles.
     */
    virtual std::span<const double> getDataSpan() = 0;

    
    virtual std::vector<double> getDataInRange(TimeFrameIndex start,
                                               TimeFrameIndex end,
                                               TimeFrame const & source_timeFrame,
                                               TimeFrame const & target_timeFrame) = 0;

};

#endif // IANALOG_SOURCE_H
