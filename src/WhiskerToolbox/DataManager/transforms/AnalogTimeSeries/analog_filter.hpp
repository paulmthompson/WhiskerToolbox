#ifndef ANALOG_FILTER_HPP
#define ANALOG_FILTER_HPP

#include "transforms/data_transforms.hpp"
#include "utils/filter/filter.hpp"

#include <memory>
#include <string>
#include <typeindex>

class AnalogTimeSeries;

/**
 * @brief Parameters for filtering analog time series data
 */
struct AnalogFilterParams : public TransformParametersBase {
    FilterOptions filter_options;
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

#endif // ANALOG_FILTER_HPP 