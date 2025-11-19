#include "analog_filter.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/utils/variant_type_check.hpp"
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
    } else if (filterParams.filter_specification.has_value()) {
        // Create filter from specification
        auto filter = filterParams.filter_specification->createFilter();
        auto result = filterWithInstance(analog_time_series, std::move(filter));
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
    auto data_span = analog_time_series->getAnalogTimeSeries();
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
    return canApplyToType<AnalogTimeSeries>(dataVariant);
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

// ============================================================================
// FilterSpecification Implementation
// ============================================================================

std::vector<std::string> FilterSpecification::validate() const {
    std::vector<std::string> errors;
    
    // Validate order
    if (order < 1 || order > 8) {
        errors.push_back("Filter order must be between 1 and 8, got " + std::to_string(order));
    }
    
    // Validate sampling rate
    if (sampling_rate_hz <= 0.0) {
        errors.push_back("Sampling rate must be positive, got " + std::to_string(sampling_rate_hz));
    }
    
    // Validate cutoff frequencies based on response type
    if (response == FilterResponse::Lowpass || response == FilterResponse::Highpass) {
        if (cutoff_hz <= 0.0) {
            errors.push_back("Cutoff frequency must be positive, got " + std::to_string(cutoff_hz));
        }
        if (cutoff_hz >= sampling_rate_hz / 2.0) {
            errors.push_back("Cutoff frequency (" + std::to_string(cutoff_hz) + 
                           " Hz) must be less than Nyquist frequency (" + 
                           std::to_string(sampling_rate_hz / 2.0) + " Hz)");
        }
    } else {  // Bandpass or Bandstop
        if (cutoff_low_hz <= 0.0) {
            errors.push_back("Low cutoff frequency must be positive, got " + std::to_string(cutoff_low_hz));
        }
        if (cutoff_high_hz <= 0.0) {
            errors.push_back("High cutoff frequency must be positive, got " + std::to_string(cutoff_high_hz));
        }
        if (cutoff_low_hz >= cutoff_high_hz) {
            errors.push_back("Low cutoff (" + std::to_string(cutoff_low_hz) + 
                           " Hz) must be less than high cutoff (" + std::to_string(cutoff_high_hz) + " Hz)");
        }
        if (cutoff_high_hz >= sampling_rate_hz / 2.0) {
            errors.push_back("High cutoff frequency (" + std::to_string(cutoff_high_hz) + 
                           " Hz) must be less than Nyquist frequency (" + 
                           std::to_string(sampling_rate_hz / 2.0) + " Hz)");
        }
    }
    
    // Validate ripple for Chebyshev filters
    if (family == FilterFamily::ChebyshevI || family == FilterFamily::ChebyshevII) {
        if (ripple_db <= 0.0) {
            errors.push_back("Ripple must be positive for Chebyshev filters, got " + std::to_string(ripple_db));
        }
    }
    
    // Validate Q factor for RBJ filters
    if (family == FilterFamily::RBJ) {
        if (q_factor <= 0.0) {
            errors.push_back("Q factor must be positive for RBJ filters, got " + std::to_string(q_factor));
        }
        if (response != FilterResponse::Bandstop) {
            errors.push_back("RBJ filter family only supports bandstop (notch) response");
        }
    }
    
    return errors;
}

std::unique_ptr<IFilter> FilterSpecification::createFilter() const {
    // Validate first
    auto validation_errors = validate();
    if (!validation_errors.empty()) {
        std::string error_msg = "Invalid filter specification:\n";
        for (auto const& error : validation_errors) {
            error_msg += "  - " + error + "\n";
        }
        throw std::invalid_argument(error_msg);
    }
    
    // Helper lambda to create filter with runtime order dispatch
    auto createByOrder = [this](auto createFunc) -> std::unique_ptr<IFilter> {
        switch (order) {
            case 1: return createFunc.template operator()<1>();
            case 2: return createFunc.template operator()<2>();
            case 3: return createFunc.template operator()<3>();
            case 4: return createFunc.template operator()<4>();
            case 5: return createFunc.template operator()<5>();
            case 6: return createFunc.template operator()<6>();
            case 7: return createFunc.template operator()<7>();
            case 8: return createFunc.template operator()<8>();
            default: throw std::invalid_argument("Invalid filter order: " + std::to_string(order));
        }
    };
    
    // Create filter based on family and response
    if (family == FilterFamily::Butterworth) {
        if (response == FilterResponse::Lowpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createButterworthLowpass<Order>(
                    cutoff_hz, sampling_rate_hz, zero_phase);
            });
        } else if (response == FilterResponse::Highpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createButterworthHighpass<Order>(
                    cutoff_hz, sampling_rate_hz, zero_phase);
            });
        } else if (response == FilterResponse::Bandpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createButterworthBandpass<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, zero_phase);
            });
        } else {  // Bandstop
            return createByOrder([this]<int Order>() {
                return FilterFactory::createButterworthBandstop<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, zero_phase);
            });
        }
    } else if (family == FilterFamily::ChebyshevI) {
        if (response == FilterResponse::Lowpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevILowpass<Order>(
                    cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else if (response == FilterResponse::Highpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIHighpass<Order>(
                    cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else if (response == FilterResponse::Bandpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIBandpass<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else {  // Bandstop
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIBandstop<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        }
    } else if (family == FilterFamily::ChebyshevII) {
        if (response == FilterResponse::Lowpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIILowpass<Order>(
                    cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else if (response == FilterResponse::Highpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIIHighpass<Order>(
                    cutoff_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else if (response == FilterResponse::Bandpass) {
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIIBandpass<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        } else {  // Bandstop
            return createByOrder([this]<int Order>() {
                return FilterFactory::createChebyshevIIBandstop<Order>(
                    cutoff_low_hz, cutoff_high_hz, sampling_rate_hz, ripple_db, zero_phase);
            });
        }
    } else if (family == FilterFamily::RBJ) {
        // RBJ only supports bandstop (validated above)
        return FilterFactory::createRBJBandstop(cutoff_hz, sampling_rate_hz, q_factor, zero_phase);
    }
    
    throw std::invalid_argument("Unknown filter family");
}

nlohmann::json FilterSpecification::toJson() const {
    nlohmann::json j;
    
    // Convert family enum to string
    switch (family) {
        case FilterFamily::Butterworth: j["filter_family"] = "butterworth"; break;
        case FilterFamily::ChebyshevI: j["filter_family"] = "chebyshev_i"; break;
        case FilterFamily::ChebyshevII: j["filter_family"] = "chebyshev_ii"; break;
        case FilterFamily::RBJ: j["filter_family"] = "rbj"; break;
    }
    
    // Convert response enum to string
    switch (response) {
        case FilterResponse::Lowpass: j["filter_response"] = "lowpass"; break;
        case FilterResponse::Highpass: j["filter_response"] = "highpass"; break;
        case FilterResponse::Bandpass: j["filter_response"] = "bandpass"; break;
        case FilterResponse::Bandstop: j["filter_response"] = "bandstop"; break;
    }
    
    // RBJ filters don't use order
    if (family != FilterFamily::RBJ) {
        j["order"] = order;
    }
    j["sampling_rate_hz"] = sampling_rate_hz;
    j["zero_phase"] = zero_phase;
    
    // Add frequency parameters based on response type and family
    // RBJ filters use a single center frequency even for bandstop
    if (family == FilterFamily::RBJ) {
        j["cutoff_hz"] = cutoff_hz;  // Center frequency for RBJ notch
    } else if (response == FilterResponse::Lowpass || response == FilterResponse::Highpass) {
        j["cutoff_hz"] = cutoff_hz;
    } else {
        j["cutoff_low_hz"] = cutoff_low_hz;
        j["cutoff_high_hz"] = cutoff_high_hz;
    }
    
    // Add family-specific parameters
    if (family == FilterFamily::ChebyshevI || family == FilterFamily::ChebyshevII) {
        j["ripple_db"] = ripple_db;
    }
    if (family == FilterFamily::RBJ) {
        j["q_factor"] = q_factor;
    }
    
    return j;
}

FilterSpecification FilterSpecification::fromJson(nlohmann::json const& json) {
    FilterSpecification spec;
    
    // Parse filter family (required)
    if (!json.contains("filter_family") || !json["filter_family"].is_string()) {
        throw std::invalid_argument("Missing or invalid 'filter_family' field");
    }
    std::string family_str = json["filter_family"];
    if (family_str == "butterworth") {
        spec.family = FilterFamily::Butterworth;
    } else if (family_str == "chebyshev_i") {
        spec.family = FilterFamily::ChebyshevI;
    } else if (family_str == "chebyshev_ii") {
        spec.family = FilterFamily::ChebyshevII;
    } else if (family_str == "rbj") {
        spec.family = FilterFamily::RBJ;
    } else {
        throw std::invalid_argument("Unknown filter family: " + family_str);
    }
    
    // Parse filter response (required)
    if (!json.contains("filter_response") || !json["filter_response"].is_string()) {
        throw std::invalid_argument("Missing or invalid 'filter_response' field");
    }
    std::string response_str = json["filter_response"];
    if (response_str == "lowpass") {
        spec.response = FilterResponse::Lowpass;
    } else if (response_str == "highpass") {
        spec.response = FilterResponse::Highpass;
    } else if (response_str == "bandpass") {
        spec.response = FilterResponse::Bandpass;
    } else if (response_str == "bandstop") {
        spec.response = FilterResponse::Bandstop;
    } else {
        throw std::invalid_argument("Unknown filter response: " + response_str);
    }
    
    // Parse order (required for non-RBJ filters)
    if (spec.family != FilterFamily::RBJ) {
        if (!json.contains("order") || !json["order"].is_number_integer()) {
            throw std::invalid_argument("Missing or invalid 'order' field");
        }
        spec.order = json["order"];
    }
    // RBJ filters don't use order, keep the default
    
    // Parse sampling rate (required)
    if (!json.contains("sampling_rate_hz") || !json["sampling_rate_hz"].is_number()) {
        throw std::invalid_argument("Missing or invalid 'sampling_rate_hz' field");
    }
    spec.sampling_rate_hz = json["sampling_rate_hz"];
    
    // Parse zero phase (optional, defaults to false)
    if (json.contains("zero_phase") && json["zero_phase"].is_boolean()) {
        spec.zero_phase = json["zero_phase"];
    }
    
    // Parse frequency parameters - RBJ is a special case
    if (spec.family == FilterFamily::RBJ) {
        // RBJ uses a single center frequency even for bandstop
        if (!json.contains("cutoff_hz") || !json["cutoff_hz"].is_number()) {
            throw std::invalid_argument("Missing or invalid 'cutoff_hz' field for RBJ filter");
        }
        spec.cutoff_hz = json["cutoff_hz"];
        
        if (json.contains("q_factor") && json["q_factor"].is_number()) {
            spec.q_factor = json["q_factor"];
        }
    } else if (spec.response == FilterResponse::Lowpass || spec.response == FilterResponse::Highpass) {
        if (!json.contains("cutoff_hz") || !json["cutoff_hz"].is_number()) {
            throw std::invalid_argument("Missing or invalid 'cutoff_hz' field for lowpass/highpass filter");
        }
        spec.cutoff_hz = json["cutoff_hz"];
    } else {  // Bandpass or Bandstop (non-RBJ)
        if (!json.contains("cutoff_low_hz") || !json["cutoff_low_hz"].is_number()) {
            throw std::invalid_argument("Missing or invalid 'cutoff_low_hz' field for bandpass/bandstop filter");
        }
        if (!json.contains("cutoff_high_hz") || !json["cutoff_high_hz"].is_number()) {
            throw std::invalid_argument("Missing or invalid 'cutoff_high_hz' field for bandpass/bandstop filter");
        }
        spec.cutoff_low_hz = json["cutoff_low_hz"];
        spec.cutoff_high_hz = json["cutoff_high_hz"];
    }
    
    // Parse family-specific parameters (Chebyshev only at this point, RBJ handled above)
    if (spec.family == FilterFamily::ChebyshevI || spec.family == FilterFamily::ChebyshevII) {
        if (!json.contains("ripple_db") || !json["ripple_db"].is_number()) {
            throw std::invalid_argument("Missing or invalid 'ripple_db' field for Chebyshev filter");
        }
        spec.ripple_db = json["ripple_db"];
    }
    
    // Validate the specification
    auto validation_errors = spec.validate();
    if (!validation_errors.empty()) {
        std::string error_msg = "Invalid filter specification from JSON:\n";
        for (auto const& error : validation_errors) {
            error_msg += "  - " + error + "\n";
        }
        throw std::invalid_argument(error_msg);
    }
    
    return spec;
}

std::string FilterSpecification::getName() const {
    std::string name;
    
    // Add family
    switch (family) {
        case FilterFamily::Butterworth: name += "Butterworth "; break;
        case FilterFamily::ChebyshevI: name += "Chebyshev I "; break;
        case FilterFamily::ChebyshevII: name += "Chebyshev II "; break;
        case FilterFamily::RBJ: name += "RBJ "; break;
    }
    
    // Add response
    switch (response) {
        case FilterResponse::Lowpass: name += "Lowpass"; break;
        case FilterResponse::Highpass: name += "Highpass"; break;
        case FilterResponse::Bandpass: name += "Bandpass"; break;
        case FilterResponse::Bandstop: name += "Bandstop"; break;
    }
    
    // Add order (not for RBJ)
    if (family != FilterFamily::RBJ) {
        name += " Order " + std::to_string(order);
    }
    
    // Add zero phase indicator
    if (zero_phase) {
        name += " (Zero-Phase)";
    }
    
    return name;
} 