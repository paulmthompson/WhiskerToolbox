#ifndef ANALOG_FILTER_HPP
#define ANALOG_FILTER_HPP

#include "transforms/data_transforms.hpp"
#include "utils/filter/filter.hpp"
#include "utils/filter/IFilter.hpp"
#include "utils/filter/FilterFactory.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>

class AnalogTimeSeries;

/**
 * @brief Modern parameters for filtering analog time series data
 * 
 * This structure supports both the new filter interface and legacy FilterOptions
 * for backward compatibility during the transition period.
 */
struct AnalogFilterParams : public TransformParametersBase {
    // Primary approach: Use a pre-created filter instance
    std::shared_ptr<IFilter> filter_instance;
    
    // Alternative: Use a factory function to create the filter when needed
    std::function<std::unique_ptr<IFilter>()> filter_factory;
    
    // Legacy fallback: Use FilterOptions (will be converted internally)
    std::optional<FilterOptions> legacy_options;
    
    /**
     * @brief Create parameters with a pre-created filter instance
     */
    static AnalogFilterParams withFilter(std::shared_ptr<IFilter> filter) {
        AnalogFilterParams params;
        params.filter_instance = std::move(filter);
        return params;
    }
    
    /**
     * @brief Create parameters with a filter factory function
     */
    static AnalogFilterParams withFactory(std::function<std::unique_ptr<IFilter>()> factory) {
        AnalogFilterParams params;
        params.filter_factory = std::move(factory);
        return params;
    }
    
    /**
     * @brief Create parameters from legacy FilterOptions (backward compatibility)
     */
    static AnalogFilterParams withLegacyOptions(FilterOptions options) {
        AnalogFilterParams params;
        params.legacy_options = std::move(options);
        return params;
    }
    
    /**
     * @brief Legacy constructor for backward compatibility
     */
    AnalogFilterParams(FilterOptions const& options) : legacy_options(options) {}
    
    /**
     * @brief Default constructor
     */
    AnalogFilterParams() = default;
    
    /**
     * @brief Check if parameters are valid
     */
    [[nodiscard]] bool isValid() const {
        return filter_instance != nullptr || 
               filter_factory != nullptr || 
               (legacy_options.has_value() && legacy_options->isValid());
    }
    
    /**
     * @brief Get a descriptive name for the configured filter
     */
    [[nodiscard]] std::string getFilterName() const {
        if (filter_instance) {
            return filter_instance->getName();
        } else if (legacy_options.has_value()) {
            return "Legacy filter configuration";
        } else if (filter_factory) {
            return "Custom filter factory";
        }
        return "No filter configured";
    }
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
 * @brief Helper function to apply a filter instance directly to analog time series
 * 
 * @param analog_time_series Input time series to filter
 * @param filter Pre-created filter instance
 * @return std::shared_ptr<AnalogTimeSeries> Filtered time series
 */
std::shared_ptr<AnalogTimeSeries> filterWithInstance(
        AnalogTimeSeries const * analog_time_series,
        std::shared_ptr<IFilter> filter);

/**
 * @brief Helper function to apply a filter instance directly to analog time series
 * 
 * @param analog_time_series Input time series to filter
 * @param filter Pre-created filter instance (unique_ptr version)
 * @return std::shared_ptr<AnalogTimeSeries> Filtered time series
 */
std::shared_ptr<AnalogTimeSeries> filterWithInstance(
        AnalogTimeSeries const * analog_time_series,
        std::unique_ptr<IFilter> filter);

// Convenience factory functions for common filter types

/**
 * @brief Create parameters for a Butterworth lowpass filter
 */
template<int Order>
AnalogFilterParams createButterworthLowpass(double cutoff_hz, double sampling_rate_hz, bool zero_phase = false) {
    return AnalogFilterParams::withFactory([=]() {
        return FilterFactory::createButterworthLowpass<Order>(cutoff_hz, sampling_rate_hz, zero_phase);
    });
}

/**
 * @brief Create parameters for a Butterworth highpass filter
 */
template<int Order>
AnalogFilterParams createButterworthHighpass(double cutoff_hz, double sampling_rate_hz, bool zero_phase = false) {
    return AnalogFilterParams::withFactory([=]() {
        return FilterFactory::createButterworthHighpass<Order>(cutoff_hz, sampling_rate_hz, zero_phase);
    });
}

/**
 * @brief Create parameters for a Butterworth bandpass filter
 */
template<int Order>
AnalogFilterParams createButterworthBandpass(double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, bool zero_phase = false) {
    return AnalogFilterParams::withFactory([=]() {
        return FilterFactory::createButterworthBandpass<Order>(low_cutoff_hz, high_cutoff_hz, sampling_rate_hz, zero_phase);
    });
}

/**
 * @brief Create parameters for an RBJ notch filter
 */
inline AnalogFilterParams createRBJNotch(double center_freq_hz, double sampling_rate_hz, double q_factor = 10.0, bool zero_phase = false) {
    return AnalogFilterParams::withFactory([=]() {
        return FilterFactory::createRBJBandstop(center_freq_hz, sampling_rate_hz, q_factor, zero_phase);
    });
}

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