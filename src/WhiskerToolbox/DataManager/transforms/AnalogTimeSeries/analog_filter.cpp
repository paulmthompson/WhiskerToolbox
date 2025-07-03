#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

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
    if (!filterParams.filter_options.isValid()) {
        throw std::invalid_argument("Invalid filter parameters: " + 
                                  filterParams.filter_options.getValidationError());
    }

    // Report initial progress
    progressCallback(0);

    // Apply the filter
    auto result = filterAnalogTimeSeries(analog_time_series, filterParams.filter_options);

    if (!result.success) {
        throw std::runtime_error("Filtering failed: " + result.error_message);
    }

    // Report completion
    progressCallback(100);

    return result.filtered_data;
}

std::string AnalogFilterOperation::getName() const {
    return "Filter";
}

std::type_index AnalogFilterOperation::getTargetInputTypeIndex() const {
    return typeid(AnalogTimeSeries);
}

std::unique_ptr<TransformParametersBase> AnalogFilterOperation::getDefaultParameters() const {
    auto params = std::make_unique<AnalogFilterParams>();
    
    // Set default filter options (4th order Butterworth lowpass at 100 Hz)
    params->filter_options = FilterDefaults::lowpass(100.0, 1000.0, 4);
    
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