#ifndef DIGITAL_INTERVAL_DATA_ADAPTER_H
#define DIGITAL_INTERVAL_DATA_ADAPTER_H

#include "utils/TableView/interfaces/IIntervalSource.h"

#include <memory>
#include <vector>

class DigitalIntervalSeries;

/**
 * @brief Adapter that exposes DigitalIntervalSeries as an IIntervalSource.
 * 
 * This adapter provides a bridge between the existing DigitalIntervalSeries class
 * and the IIntervalSource interface required by the TableView system.
 */
class DigitalIntervalDataAdapter : public IIntervalSource {
public:
    /**
     * @brief Constructs a DigitalIntervalDataAdapter.
     * @param digitalIntervalSeries Shared pointer to the DigitalIntervalSeries source.
     * @param timeFrame Shared pointer to the TimeFrame this data belongs to.
     * @param name The name of this data source.
     */
    DigitalIntervalDataAdapter(std::shared_ptr<DigitalIntervalSeries> digitalIntervalSeries,
                               std::shared_ptr<TimeFrame> timeFrame,
                               std::string name);

    // IIntervalSource interface implementation

    /**
     * @brief Gets the name of this data source.
     * 
     * This name is used for dependency tracking and ExecutionPlan caching
     * in the TableView system.
     * 
     * @return The name of the data source.
     */
    [[nodiscard]] auto getName() const -> std::string const & override;

    /**
     * @brief Gets the TimeFrame the data belongs to.
     * @return A shared pointer to the TimeFrame.
     */
    [[nodiscard]] auto getTimeFrame() const -> std::shared_ptr<TimeFrame> override;

    /**
     * @brief Gets the total number of intervals in the source.
     * @return The number of intervals.
     */
    [[nodiscard]] auto size() const -> size_t override;

    /**
     * @brief Gets all intervals in the source.
     * @return A vector of Intervals representing all intervals in the source.
     */
    std::vector<Interval> getIntervals() override;

    /**
     * @brief Gets the intervals within a specific time range.
     * 
     * This gets the intervals in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of Intervals representing the intervals in the specified range.
     */
    std::vector<Interval> getIntervalsInRange(TimeFrameIndex start,
                                              TimeFrameIndex end,
                                              TimeFrame const * target_timeFrame) override;

    [[nodiscard]] EntityId getEntityIdAt(size_t index) const override;

private:
    std::shared_ptr<DigitalIntervalSeries> m_digitalIntervalSeries;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
};

#endif// DIGITAL_INTERVAL_DATA_ADAPTER_H
