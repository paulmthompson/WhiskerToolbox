#include "DerivedTimeFrame.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"

#include <iostream>

std::shared_ptr<TimeFrame> createDerivedTimeFrame(DerivedTimeFrameFromIntervalsOptions const & options) {
    // Validate inputs
    if (!options.source_timeframe) {
        std::cerr << "Error: Source timeframe is null" << std::endl;
        return nullptr;
    }
    if (!options.interval_series) {
        std::cerr << "Error: Interval series is null" << std::endl;
        return nullptr;
    }
    if (options.interval_series->size() == 0) {
        std::cerr << "Error: Interval series is empty" << std::endl;
        return nullptr;
    }

    std::vector<int> derived_times;
    derived_times.reserve(options.interval_series->size());

    auto const & intervals = options.interval_series->view();
    
    for (auto const & interval : intervals) {
        TimeFrameIndex index = (options.edge == IntervalEdge::START) 
            ? TimeFrameIndex(interval.value().start) 
            : TimeFrameIndex(interval.value().end);
        
        // Get the actual time value from the source timeframe at this index
        int const time_value = options.source_timeframe->getTimeAtIndex(index);
        derived_times.push_back(time_value);
    }

    std::cout << "Created derived TimeFrame with " << derived_times.size() 
              << " time points from " << options.interval_series->size() 
              << " intervals (using " << (options.edge == IntervalEdge::START ? "start" : "end") 
              << " edge)" << std::endl;

    return std::make_shared<TimeFrame>(derived_times);
}

std::shared_ptr<TimeFrame> createDerivedTimeFrame(DerivedTimeFrameFromEventsOptions const & options) {
    // Validate inputs
    if (!options.source_timeframe) {
        std::cerr << "Error: Source timeframe is null" << std::endl;
        return nullptr;
    }
    if (!options.event_series) {
        std::cerr << "Error: Event series is null" << std::endl;
        return nullptr;
    }
    if (options.event_series->size() == 0) {
        std::cerr << "Error: Event series is empty" << std::endl;
        return nullptr;
    }

    std::vector<int> derived_times;
    derived_times.reserve(options.event_series->size());

    auto const & events = options.event_series->view();
    
    for (auto const & event : events) {
        // Get the actual time value from the source timeframe at this index
        //int const time_value = options.source_timeframe->getTimeAtIndex(event.time());
        int const time_value = event.time().getValue();
        derived_times.push_back(time_value);
    }

    std::cout << "Created derived TimeFrame with " << derived_times.size() 
              << " time points from " << options.event_series->size() 
              << " events" << std::endl;

    return std::make_shared<TimeFrame>(derived_times);
}
