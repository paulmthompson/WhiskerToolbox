#include "DerivedTimeFrame.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <cmath>
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

    for (auto const & interval: intervals) {
        TimeFrameIndex const index = (options.edge == IntervalEdge::START)
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

    for (auto const & event: events) {
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

std::shared_ptr<TimeFrame> createUpsampledTimeFrame(DerivedTimeFrameFromUpsamplingOptions const & options) {
    if (!options.source_timeframe) {
        std::cerr << "Error: Source timeframe is null" << std::endl;
        return nullptr;
    }
    if (options.upsampling_factor <= 0) {
        std::cerr << "Error: Upsampling factor must be > 0, got "
                  << options.upsampling_factor << std::endl;
        return nullptr;
    }

    int const n = options.source_timeframe->getTotalFrameCount();

    if (n <= 1) {
        // Empty or single-entry: return a copy (no interpolation possible)
        std::vector<int> times;
        times.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            times.push_back(options.source_timeframe->getTimeAtIndex(TimeFrameIndex(i)));
        }
        return std::make_shared<TimeFrame>(times);
    }

    int const factor = options.upsampling_factor;
    auto const output_size = static_cast<size_t>((n - 1) * factor + 1);
    std::vector<int> upsampled_times;
    upsampled_times.reserve(output_size);

    for (int i = 0; i < n - 1; ++i) {
        int const t_curr = options.source_timeframe->getTimeAtIndex(TimeFrameIndex(i));
        int const t_next = options.source_timeframe->getTimeAtIndex(TimeFrameIndex(i + 1));

        for (int j = 0; j < factor; ++j) {
            // Linear interpolation: t_curr + j * (t_next - t_curr) / factor
            double const interpolated =
                    static_cast<double>(t_curr) +
                    static_cast<double>(j) * (static_cast<double>(t_next) - static_cast<double>(t_curr)) /
                            static_cast<double>(factor);
            upsampled_times.push_back(static_cast<int>(std::round(interpolated)));
        }
    }

    // Add the final time point
    upsampled_times.push_back(
            options.source_timeframe->getTimeAtIndex(TimeFrameIndex(n - 1)));

    return std::make_shared<TimeFrame>(upsampled_times);
}
