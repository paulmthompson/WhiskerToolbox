#ifndef ANALOG_FILTER_HPP
#define ANALOG_FILTER_HPP

#include "transforms/data_transforms.hpp"
#include "utils/filter/FilterFactory.hpp"
#include "utils/filter/IFilter.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <vector>

class AnalogTimeSeriesInMemory;
using AnalogTimeSeries = AnalogTimeSeriesInMemory;

/**
 * @brief Enumeration of supported filter families
 */
enum class FilterFamily {
    Butterworth,
    ChebyshevI,
    ChebyshevII,
    RBJ
};

/**
 * @brief Enumeration of supported filter responses
 */
enum class FilterResponse {
    Lowpass,
    Highpass,
    Bandpass,
    Bandstop
};

/**
 * @brief Complete specification for creating a filter
 * 
 * This structure stores all parameters needed to create a filter and can be
 * serialized to/from JSON for pipeline configuration.
 */
struct FilterSpecification {
    FilterFamily family = FilterFamily::Butterworth;
    FilterResponse response = FilterResponse::Lowpass;
    int order = 4;
    double cutoff_hz = 10.0;                    // For lowpass/highpass
    double cutoff_low_hz = 5.0;                 // For bandpass/bandstop
    double cutoff_high_hz = 15.0;               // For bandpass/bandstop
    double sampling_rate_hz = 1000.0;
    bool zero_phase = false;
    double ripple_db = 0.5;                     // For Chebyshev filters
    double q_factor = 10.0;                     // For RBJ filters
    
    /**
     * @brief Validate the filter specification
     * 
     * @return std::vector<std::string> List of validation errors (empty if valid)
     */
    [[nodiscard]] std::vector<std::string> validate() const;
    
    /**
     * @brief Check if the specification is valid
     * 
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool isValid() const {
        return validate().empty();
    }
    
    /**
     * @brief Create a filter from this specification
     * 
     * @return std::unique_ptr<IFilter> Created filter instance
     * @throws std::invalid_argument if specification is invalid
     */
    [[nodiscard]] std::unique_ptr<IFilter> createFilter() const;
    
    /**
     * @brief Convert specification to JSON
     * 
     * @return nlohmann::json JSON representation
     */
    [[nodiscard]] nlohmann::json toJson() const;
    
    /**
     * @brief Create specification from JSON
     * 
     * @param json JSON object
     * @return FilterSpecification Created specification
     * @throws std::invalid_argument if JSON is malformed
     */
    static FilterSpecification fromJson(nlohmann::json const& json);
    
    /**
     * @brief Get a descriptive name for this filter configuration
     * 
     * @return std::string Human-readable filter description
     */
    [[nodiscard]] std::string getName() const;
};

/**
 * @brief Modern parameters for filtering analog time series data
 * 
 * This structure uses the new modular filter interface for efficient
 * and flexible filter configuration. Supports three modes:
 * 1. Direct filter instance (for programmatic use)
 * 2. Factory function (for deferred creation)
 * 3. Filter specification (for JSON serialization/deserialization)
 */
struct AnalogFilterParams : public TransformParametersBase {
    // Primary approach: Use a pre-created filter instance
    std::shared_ptr<IFilter> filter_instance;
    
    // Alternative: Use a factory function to create the filter when needed
    std::function<std::unique_ptr<IFilter>()> filter_factory;
    
    // JSON-compatible approach: Use a filter specification
    std::optional<FilterSpecification> filter_specification;
    
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
     * @brief Create parameters with a filter specification (for JSON pipelines)
     */
    static AnalogFilterParams withSpecification(FilterSpecification spec) {
        AnalogFilterParams params;
        params.filter_specification = std::move(spec);
        return params;
    }
    
    /**
     * @brief Create default filter parameters (4th order Butterworth lowpass, 10 Hz, 1000 Hz sampling)
     */
    static AnalogFilterParams createDefault() {
        return AnalogFilterParams(); // Uses the default constructor
    }
    
    /**
     * @brief Create default filter parameters with custom sampling rate
     */
    static AnalogFilterParams createDefault(double sampling_rate_hz, double cutoff_hz = 10.0) {
        AnalogFilterParams params;
        params.filter_factory = [=]() {
            return FilterFactory::createButterworthLowpass<4>(cutoff_hz, sampling_rate_hz, false);
        };
        return params;
    }
    
    /**
     * @brief Default constructor - creates a default 4th order Butterworth lowpass filter
     */
    AnalogFilterParams() {
        // Create default filter: 4th order Butterworth lowpass at 10 Hz, 1000 Hz sampling rate
        filter_factory = []() {
            return FilterFactory::createButterworthLowpass<4>(10.0, 1000.0, false);
        };
    }
    
    /**
     * @brief Check if parameters are valid
     */
    [[nodiscard]] bool isValid() const {
        return filter_instance != nullptr || filter_factory != nullptr || 
               (filter_specification.has_value() && filter_specification->isValid());
    }
    
    /**
     * @brief Get a descriptive name for the configured filter
     */
    [[nodiscard]] std::string getFilterName() const {
        if (filter_instance) {
            return filter_instance->getName();
        } else if (filter_specification.has_value()) {
            return filter_specification->getName();
        } else if (filter_factory) {
            // For factory functions, we can try to create a temporary instance to get the name
            try {
                auto temp_filter = filter_factory();
                return temp_filter ? temp_filter->getName() : "Factory-created filter";
            } catch (...) {
                return "Custom filter factory";
            }
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

#endif// ANALOG_FILTER_HPP