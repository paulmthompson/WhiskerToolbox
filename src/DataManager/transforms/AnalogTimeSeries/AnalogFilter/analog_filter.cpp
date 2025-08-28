#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/FilterImplementations.hpp"
#include "utils/filter/ZeroPhaseDecorator.hpp"

#include <stdexcept>

// Helper function to create a filter with a runtime order
template<template<int> class Filter, typename... Args>
std::unique_ptr<IFilter> create_filter_with_order(int order, Args... args) {
    switch (order) {
        case 1:
            return std::make_unique<Filter<1>>(args...);
        case 2:
            return std::make_unique<Filter<2>>(args...);
        case 3:
            return std::make_unique<Filter<3>>(args...);
        case 4:
            return std::make_unique<Filter<4>>(args...);
        case 5:
            return std::make_unique<Filter<5>>(args...);
        case 6:
            return std::make_unique<Filter<6>>(args...);
        case 7:
            return std::make_unique<Filter<7>>(args...);
        case 8:
            return std::make_unique<Filter<8>>(args...);
        default:
            throw std::invalid_argument("Unsupported filter order");
    }
}

std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams,
        ProgressCallback progressCallback) {
    if (!analog_time_series) {
        throw std::invalid_argument("Input analog time series is null");
    }

    progressCallback(0);

    if (filterParams.sampling_rate <= 0) {
        throw std::invalid_argument("Sampling rate must be positive.");
    }

    double sampling_rate = filterParams.sampling_rate;
    std::unique_ptr<IFilter> filter;

    switch (filterParams.filter_type) {
        case AnalogFilterParams::FilterType::Lowpass:
            filter = create_filter_with_order<ButterworthLowpassFilter>(filterParams.order, filterParams.cutoff_frequency, sampling_rate);
            break;
        case AnalogFilterParams::FilterType::Highpass:
            filter = create_filter_with_order<ButterworthHighpassFilter>(filterParams.order, filterParams.cutoff_frequency, sampling_rate);
            break;
        case AnalogFilterParams::FilterType::Bandpass:
            filter = create_filter_with_order<ButterworthBandpassFilter>(filterParams.order, filterParams.cutoff_frequency, filterParams.cutoff_frequency2, sampling_rate);
            break;
        case AnalogFilterParams::FilterType::Bandstop:
            filter = create_filter_with_order<ButterworthBandstopFilter>(filterParams.order, filterParams.cutoff_frequency, filterParams.cutoff_frequency2, sampling_rate);
            break;
    }

    if (filterParams.zero_phase) {
        filter = std::make_unique<ZeroPhaseDecorator>(std::move(filter));
    }

    if (!filter) {
        throw std::runtime_error("Failed to create filter");
    }

    auto & data_span = analog_time_series->getAnalogTimeSeries();
    auto time_series = analog_time_series->getTimeSeries();

    if (data_span.empty()) {
        throw std::invalid_argument("No data found in time series");
    }

    std::vector<float> filtered_data(data_span.begin(), data_span.end());
    std::vector<TimeFrameIndex> filtered_times(time_series.begin(), time_series.end());

    std::span<float> data_span_mutable(filtered_data);
    filter->process(data_span_mutable);

    progressCallback(100);

    return std::make_shared<AnalogTimeSeries>(
            std::move(filtered_data),
            std::move(filtered_times));
}


std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams) {
    return filter_analog(analog_time_series, filterParams, [](int) {});
}

std::string AnalogFilterOperation::getName() const {
    return "Analog Filter";
}

std::type_index AnalogFilterOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

std::unique_ptr<TransformParametersBase> AnalogFilterOperation::getDefaultParameters() const {
    return std::make_unique<AnalogFilterParams>();
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
