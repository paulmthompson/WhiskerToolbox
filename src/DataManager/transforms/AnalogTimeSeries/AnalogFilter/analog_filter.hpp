#ifndef ANALOG_FILTER_HPP
#define ANALOG_FILTER_HPP

#include "transforms/data_transforms.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/IFilter.hpp"

#include <functional>
#include <memory>
#include <string>
#include <typeindex>

class AnalogTimeSeries;

/**
 * @brief Parameters for filtering analog time series data, configurable from JSON.
 */
struct AnalogFilterParams : public TransformParametersBase {
    enum class FilterType {
        Lowpass,
        Highpass,
        Bandpass,
        Bandstop
    };

    FilterType filter_type = FilterType::Lowpass;
    double cutoff_frequency = 10.0;
    double cutoff_frequency2 = 0.0;// Used for bandpass/bandstop
    int order = 4;
    double ripple = 0.0;// Used for Chebyshev filters
    bool zero_phase = false;
    double sampling_rate = 0.0;// Must be provided
};

/**
 * @brief Apply a filter to an analog time series
 *
 * @param analog_time_series Input time series to filter
 * @param filterParams Filter parameters
 * @return std::shared_ptr<AnalogTimeSeries> Filtered time series
 */
std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams);

/**
 * @brief Apply a filter to an analog time series with progress callback
 *
 * @param analog_time_series Input time series to filter
 * @param filterParams Filter parameters
 * @param progressCallback Callback function to report progress
 * @return std::shared_ptr<AnalogTimeSeries> Filtered time series
 */
std::shared_ptr<AnalogTimeSeries> filter_analog(
        AnalogTimeSeries const * analog_time_series,
        AnalogFilterParams const & filterParams,
        ProgressCallback progressCallback);

/**
 * @brief Transform operation for filtering analog time series
 */
class AnalogFilterOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * params) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * params,
                            ProgressCallback progressCallback) override;
};

#endif// ANALOG_FILTER_HPP