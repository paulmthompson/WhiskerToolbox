#include "new_filter_bridge.hpp"

FilterResult filterAnalogTimeSeriesNew(
    AnalogTimeSeries const * analog_time_series,
    TimeFrameIndex start_time,
    TimeFrameIndex end_time,
    FilterOptions const & options) {
    
    FilterResult result;

    // Validate inputs
    if (!analog_time_series) {
        result.error_message = "Input AnalogTimeSeries is null";
        return result;
    }

    if (!options.isValid()) {
        result.error_message = "Invalid filter options: " + options.getValidationError();
        return result;
    }

    try {
        // Extract data from the specified time range
        auto data_span = analog_time_series->getDataInTimeFrameIndexRange(start_time, end_time);
        auto time_value_range = analog_time_series->getTimeValueRangeInTimeFrameIndexRange(start_time, end_time);

        if (data_span.empty()) {
            result.error_message = "No data found in specified time range";
            return result;
        }

        // Convert span to vector for processing (we need mutable data)
        std::vector<float> filtered_data(data_span.begin(), data_span.end());
        std::vector<TimeFrameIndex> filtered_times;
        filtered_times.reserve(time_value_range.size());

        for (auto const & point: time_value_range) {
            filtered_times.push_back(point.time_frame_index);
        }

        // Create filter using the new interface
        auto filter = FilterFactory::createFromOptions(options);
        
        // Process the data using the new interface
        std::span<float> data_span_mutable(filtered_data);
        filter->process(data_span_mutable);

        result.samples_processed = filtered_data.size();
        result.segments_processed = 1;

        // Create new AnalogTimeSeries with filtered data
        result.filtered_data = std::make_shared<AnalogTimeSeries>(
            std::move(filtered_data),
            std::move(filtered_times));

        result.success = true;

    } catch (std::exception const & e) {
        result.error_message = "Filtering failed: " + std::string(e.what());
    }

    return result;
}

FilterResult filterAnalogTimeSeriesNew(
    AnalogTimeSeries const * analog_time_series,
    FilterOptions const & options) {
    
    if (!analog_time_series) {
        FilterResult result;
        result.error_message = "Input AnalogTimeSeries is null";
        return result;
    }

    // Get the full time range
    auto time_series = analog_time_series->getTimeSeries();
    if (time_series.empty()) {
        FilterResult result;
        result.error_message = "AnalogTimeSeries contains no data";
        return result;
    }

    TimeFrameIndex start_time = time_series.front();
    TimeFrameIndex end_time = time_series.back();

    return filterAnalogTimeSeriesNew(analog_time_series, start_time, end_time, options);
}
