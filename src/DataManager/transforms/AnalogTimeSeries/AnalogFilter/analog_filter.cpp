#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/FilterImplementations.hpp"
#include "utils/filter/ZeroPhaseDecorator.hpp"

#include <stdexcept>

std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams) {
    return filter_analog(analog_time_series, filterParams, [](int) {});
}

std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams,
        ProgressCallback progressCallback) {
    if (!analog_time_series) {
        throw std::invalid_argument("Input analog time series is null");
    }

    // Validate filter parameters
    if (!filterParams.isValid()) {
        throw std::invalid_argument("Invalid filter parameters");
    }

    // Report initial progress
    progressCallback(0);

    // Handle different parameter types
    if (filterParams.filter_instance) {
        // Use pre-created filter instance
        auto result = filterWithInstance(analog_time_series, filterParams.filter_instance);
        progressCallback(100);
        return result;
    } else if (filterParams.filter_factory) {
        // Create filter from factory
        auto filter = filterParams.filter_factory();
        auto result = filterWithInstance(analog_time_series, std::move(filter));
        progressCallback(100);
        return result;
    } else {
        throw std::invalid_argument("No valid filter configuration provided");
    }
}

// Helper function to filter with a direct filter instance
std::shared_ptr<AnalogTimeSeries> filterWithInstance(
        AnalogTimeSeries const * analog_time_series,
        std::shared_ptr<IFilter> filter) {
    if (!analog_time_series || !filter) {
        throw std::invalid_argument("Input analog time series or filter is null");
    }

    // Get all data from the time series
    auto & data_span = analog_time_series->getAnalogTimeSeries();
    auto time_series = analog_time_series->getTimeSeries();

    if (data_span.empty()) {
        throw std::invalid_argument("No data found in time series");
    }

    // Convert to mutable vector for processing
    std::vector<float> filtered_data(data_span.begin(), data_span.end());
    std::vector<TimeFrameIndex> filtered_times(time_series.begin(), time_series.end());

    // Apply filter
    std::span<float> data_span_mutable(filtered_data);
    filter->process(data_span_mutable);

    // Create new AnalogTimeSeries with filtered data
    return std::make_shared<AnalogTimeSeries>(
        std::move(filtered_data),
        std::move(filtered_times));
}

// Helper function to filter with a unique_ptr filter instance
std::shared_ptr<AnalogTimeSeries> filterWithInstance(
        AnalogTimeSeries const * analog_time_series,
        std::unique_ptr<IFilter> filter) {
    return filterWithInstance(analog_time_series, std::shared_ptr<IFilter>(std::move(filter)));
}

std::string AnalogFilterOperation::getName() const {
    return "Filter";
}

std::type_index AnalogFilterOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

std::unique_ptr<TransformParametersBase> AnalogFilterOperation::getDefaultParameters() const {
    // Create default parameters with 4th order Butterworth lowpass filter
    auto params = std::make_unique<AnalogFilterParams>();
    // The default constructor automatically creates the default filter
    return params;
}

bool AnalogFilterOperation::canApply(DataTypeVariant const & dataVariant) const {
    return std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant);
}

DataTypeVariant AnalogFilterOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * params) {
    return execute(dataVariant, params, [](int) {});
}

DataTypeVariant AnalogFilterOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * params,
        ProgressCallback progressCallback) {
    if (!params) {
        throw std::invalid_argument("Filter parameters are null");
    }

    auto const * filterParams = dynamic_cast<AnalogFilterParams const *>(params);
    if (!filterParams) {
        throw std::invalid_argument("Invalid parameter type for filter operation");
    }

    auto const * analog_time_series = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);
    if (!analog_time_series || !*analog_time_series) {
        throw std::invalid_argument("Invalid input data type or null pointer");
    }

    auto filtered_data = filter_analog(analog_time_series->get(), *filterParams, progressCallback);
    return DataTypeVariant(filtered_data);
} 