#ifndef DIGITAL_EVENT_DATA_ADAPTER_H
#define DIGITAL_EVENT_DATA_ADAPTER_H

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "utils/TableView/interfaces/IEventSource.h"

#include <memory>
#include <vector>

/**
 * @brief Adapter that exposes DigitalEventSeries as an IEventSource.
 * 
 * This adapter provides a bridge between the existing DigitalEventSeries class
 * and the IEventSource interface required by the TableView system.
 */
class DigitalEventDataAdapter : public IEventSource {
public:
    /**
     * @brief Constructs a DigitalEventDataAdapter.
     * @param digitalEventSeries Shared pointer to the DigitalEventSeries source.
     * @param timeFrame Shared pointer to the TimeFrame this data belongs to.
     * @param name The name of this data source.
     */
    DigitalEventDataAdapter(std::shared_ptr<DigitalEventSeries> digitalEventSeries,
                            std::shared_ptr<TimeFrame> timeFrame,
                            std::string name);

    // IEventSource interface implementation

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
     * @brief Gets the total number of events in the source.
     * @return The number of events.
     */
    [[nodiscard]] auto size() const -> size_t override;

    /**
     * @brief Gets the event data within a specific time range.
     * 
     * This gets the events in the range [start, end] (inclusive) from the source timeframe
     * 
     * @param start The start index of the time range.
     * @param end The end index of the time range.
     * @param target_timeFrame The target time frame (from the caller) for the data.
     * @return A vector of floats representing the event times in the specified range.
     */
    std::vector<float> getDataInRange(TimeFrameIndex start,
                                      TimeFrameIndex end,
                                      TimeFrame const * target_timeFrame) override;

private:
    std::shared_ptr<DigitalEventSeries> m_digitalEventSeries;
    std::shared_ptr<TimeFrame> m_timeFrame;
    std::string m_name;
};

#endif// DIGITAL_EVENT_DATA_ADAPTER_H
