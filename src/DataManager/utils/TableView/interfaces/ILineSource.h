#ifndef ILINE_SOURCE_H
#define ILINE_SOURCE_H

#include "TimeFrame/TimeFrame.hpp"
#include "CoreGeometry/lines.hpp"
#include "utils/TableView/interfaces/IEntityProvider.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Interface for data sources that consist of 2D lines.
 * 
 * This interface is designed for data that represents 2D lines in time,
 * such as whisker traces, trajectories, or other spatial paths. Each line
 * is defined by a sequence of 2D points.
 */
class ILineSource : public IEntityProvider {
public:
    virtual ~ILineSource() = default;
    
    // Make this class non-copyable and non-movable since it's a pure interface
    ILineSource(const ILineSource&) = delete;
    ILineSource& operator=(const ILineSource&) = delete;
    ILineSource(ILineSource&&) = delete;
    ILineSource& operator=(ILineSource&&) = delete;

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
     * @brief Gets the total number of lines in the source.
     * @return The number of lines.
     */
    virtual size_t size() const = 0;

    /**
     * @brief Gets all lines in the source.
     * @return A vector of Line2D representing all lines in the source.
     */
    virtual std::vector<Line2D> getLines() = 0;

    /**
     * @brief Gets the lines within a specific time range.
     * 
     * This gets the lines in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of Line2D representing the lines in the specified range.
     */
    virtual std::vector<Line2D> getLinesInRange(TimeFrameIndex start,
                                                 TimeFrameIndex end,
                                                 TimeFrame const * target_timeFrame) = 0;

    /**
     * @brief Checks if this source has multiple samples (lines) at any timestamp.
     * 
     * This method determines if the source contains more than one line at any given
     * timestamp. This is important for TableView construction because having multiple
     * multi-sample sources leads to undefined row expansion behavior.
     * 
     * @return True if any timestamp has more than one line, false otherwise.
     */
    virtual bool hasMultiSamples() const = 0;

    // IEntityProvider
    [[nodiscard]] auto getEntityCountAt(TimeFrameIndex t) const -> size_t override = 0;
    [[nodiscard]] auto getLineAt(TimeFrameIndex t, int entityIndex) const -> Line2D const* override = 0;

protected:
    // Protected constructor to prevent direct instantiation
    ILineSource() = default;
};

#endif // ILINE_SOURCE_H 