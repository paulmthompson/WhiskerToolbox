#include "analog_interval_peak.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

std::shared_ptr<DigitalEventSeries> find_interval_peaks(
        AnalogTimeSeries const * analog_time_series,
        IntervalPeakParams const & intervalPeakParams) {
    return find_interval_peaks(analog_time_series, intervalPeakParams, nullptr);
}

std::shared_ptr<DigitalEventSeries> find_interval_peaks(
        AnalogTimeSeries const * analog_time_series,
        IntervalPeakParams const & intervalPeakParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!analog_time_series) {
        std::cerr << "find_interval_peaks: Input AnalogTimeSeries is null" << std::endl;
        if (progressCallback) progressCallback(100);
        return std::make_shared<DigitalEventSeries>();
    }

    if (!intervalPeakParams.interval_series) {
        std::cerr << "find_interval_peaks: Interval series is null" << std::endl;
        if (progressCallback) progressCallback(100);
        return std::make_shared<DigitalEventSeries>();
    }

    auto const & intervals = intervalPeakParams.interval_series->getDigitalIntervalSeries();
    if (intervals.empty()) {
        if (progressCallback) progressCallback(100);
        return std::make_shared<DigitalEventSeries>();
    }

    if (progressCallback) progressCallback(5);

    // Get the interval series timeframe (may be nullptr if not set)
    auto interval_timeframe = intervalPeakParams.interval_series->getTimeFrame();

    // Build search ranges based on search mode
    // Note: intervals are in their own TimeFrameIndex coordinate system
    std::vector<std::pair<int64_t, int64_t>> search_ranges;
    
    if (intervalPeakParams.search_mode == IntervalPeakParams::SearchMode::WITHIN_INTERVALS) {
        // Search within each interval: [start, end]
        for (auto const & interval : intervals) {
            search_ranges.emplace_back(interval.start, interval.end);
        }
    } else {
        // Search between interval starts: [start_i, start_{i+1})
        for (size_t i = 0; i < intervals.size() - 1; ++i) {
            search_ranges.emplace_back(intervals[i].start, 
                                      intervals[i + 1].start - 1);
        }
        // For the last interval, search from its start to its end
        if (!intervals.empty()) {
            auto const & last_interval = intervals.back();
            search_ranges.emplace_back(last_interval.start, last_interval.end);
        }
    }

    if (progressCallback) progressCallback(10);

    // Check if analog data exists
    auto const & values = analog_time_series->getAnalogTimeSeries();
    if (values.empty()) {
        if (progressCallback) progressCallback(100);
        return std::make_shared<DigitalEventSeries>();
    }

    if (progressCallback) progressCallback(15);

    // Find peaks in each search range
    std::vector<TimeFrameIndex> peak_events;
    size_t const total_ranges = search_ranges.size();

    for (size_t range_idx = 0; range_idx < total_ranges; ++range_idx) {
        auto const & [range_start, range_end] = search_ranges[range_idx];

        TimeFrameIndex const start_index(range_start);
        TimeFrameIndex const end_index(range_end);

        // Get data and corresponding time indices in this range
        // If interval_timeframe is set, pass it for automatic conversion
        auto time_value_pair = interval_timeframe 
            ? analog_time_series->getTimeValueSpanInTimeFrameIndexRange(start_index, end_index, interval_timeframe.get())
            : analog_time_series->getTimeValueSpanInTimeFrameIndexRange(start_index, end_index);

        auto const & data_span = time_value_pair.values;
        if (data_span.empty()) {
            // No data in this range, skip it
            continue;
        }

        // Find the peak value and its index within the span
        size_t peak_idx_in_span = 0;

        if (intervalPeakParams.peak_type == IntervalPeakParams::PeakType::MAXIMUM) {
            auto max_it = std::ranges::max_element(data_span);
            peak_idx_in_span = static_cast<size_t>(std::distance(data_span.begin(), max_it));
        } else {
            auto min_it = std::ranges::min_element(data_span);
            peak_idx_in_span = static_cast<size_t>(std::distance(data_span.begin(), min_it));
        }

        // Get the actual TimeFrameIndex for the peak from the time_indices iterator
        // The time_indices correspond to positions in the data_span
        // Note: time_iter is a unique_ptr<TimeIndexIterator>, so we need to dereference it
        auto time_iter = time_value_pair.time_indices.begin();
        for (size_t i = 0; i < peak_idx_in_span; ++i) {
            ++(*time_iter);
        }
        TimeFrameIndex const peak_time_index = **time_iter;

        // Add event at the peak timestamp (in interval series coordinate system)
        peak_events.push_back(peak_time_index);

        // Report progress
        if (progressCallback && total_ranges > 0) {
            int const progress = 15 + static_cast<int>(((range_idx + 1) * 80.0) / static_cast<double>(total_ranges));
            progressCallback(progress);
        }
    }

    // Create the event series
    auto event_series = std::make_shared<DigitalEventSeries>(peak_events);

    if (progressCallback) progressCallback(100);

    return event_series;
}

///////////////////////////////////////////////////////////////////////////////

std::string AnalogIntervalPeakOperation::getName() const {
    return "Interval Peak Detection";
}

std::type_index AnalogIntervalPeakOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool AnalogIntervalPeakOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<AnalogTimeSeries>(dataVariant);
}

std::unique_ptr<TransformParametersBase> AnalogIntervalPeakOperation::getDefaultParameters() const {
    return std::make_unique<IntervalPeakParams>();
}

DataTypeVariant AnalogIntervalPeakOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant AnalogIntervalPeakOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "AnalogIntervalPeakOperation::execute called with incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    IntervalPeakParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams = dynamic_cast<IntervalPeakParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "Warning: AnalogIntervalPeakOperation received incompatible parameter type! Using default parameters." << std::endl;
        }
    }

    std::shared_ptr<DigitalEventSeries> result = find_interval_peaks(
            analog_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "AnalogIntervalPeakOperation::execute: 'find_interval_peaks' failed to produce a result." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    return result;
}
