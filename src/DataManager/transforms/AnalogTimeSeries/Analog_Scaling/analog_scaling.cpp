#include "analog_scaling.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

AnalogStatistics calculate_analog_statistics(AnalogTimeSeries const * analog_time_series) {
    AnalogStatistics stats;
    
    if (!analog_time_series) {
        return stats;
    }
    
    auto const & data = analog_time_series->getAnalogTimeSeries();
    if (data.empty()) {
        return stats;
    }
    
    stats.sample_count = data.size();
    
    // Calculate basic statistics
    stats.mean = calculate_mean(*analog_time_series);
    stats.std_dev = calculate_std_dev(*analog_time_series);
    stats.min_val = calculate_min(*analog_time_series);
    stats.max_val = calculate_max(*analog_time_series);
    
    // Calculate median and quartiles
    std::vector<float> sorted_data = data;
    std::sort(sorted_data.begin(), sorted_data.end());
    
    size_t n = sorted_data.size();
    if (n % 2 == 0) {
        stats.median = (sorted_data[n/2 - 1] + sorted_data[n/2]) / 2.0;
    } else {
        stats.median = sorted_data[n/2];
    }
    
    // Calculate quartiles
    size_t q1_idx = n / 4;
    size_t q3_idx = 3 * n / 4;
    
    if (q1_idx < n) {
        stats.q1 = sorted_data[q1_idx];
    }
    if (q3_idx < n) {
        stats.q3 = sorted_data[q3_idx];
    }
    
    stats.iqr = stats.q3 - stats.q1;
    
    return stats;
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<AnalogTimeSeries> scale_analog_time_series(
        AnalogTimeSeries const * analog_time_series,
        AnalogScalingParams const & params) {
    
    if (!analog_time_series) {
        return nullptr;
    }
    
    auto const & original_data = analog_time_series->getAnalogTimeSeries();
    auto const & time_data = analog_time_series->getTimeSeries();
    
    if (original_data.empty()) {
        return std::make_shared<AnalogTimeSeries>();
    }
    
    std::vector<float> scaled_data = original_data;
    AnalogStatistics stats = calculate_analog_statistics(analog_time_series);
    
    switch (params.method) {
        case ScalingMethod::FixedGain: {
            for (auto & value : scaled_data) {
                value *= static_cast<float>(params.gain_factor);
            }
            break;
        }
        
        case ScalingMethod::ZScore: {
            if (stats.std_dev > 0) {
                for (auto & value : scaled_data) {
                    value = static_cast<float>((value - stats.mean) / stats.std_dev);
                }
            }
            break;
        }
        
        case ScalingMethod::StandardDeviation: {
            if (stats.std_dev > 0) {
                double scale_factor = 1.0 / (params.std_dev_target * stats.std_dev);
                for (auto & value : scaled_data) {
                    value = static_cast<float>((value - stats.mean) * scale_factor);
                }
            }
            break;
        }
        
        case ScalingMethod::MinMax: {
            double range = stats.max_val - stats.min_val;
            if (range > 0) {
                double target_range = params.max_target - params.min_target;
                for (auto & value : scaled_data) {
                    value = static_cast<float>(
                        params.min_target + ((value - stats.min_val) / range) * target_range
                    );
                }
            }
            break;
        }
        
        case ScalingMethod::RobustScaling: {
            if (stats.iqr > 0) {
                for (auto & value : scaled_data) {
                    value = static_cast<float>((value - stats.median) / stats.iqr);
                }
            }
            break;
        }
        
        case ScalingMethod::UnitVariance: {
            if (stats.std_dev > 0) {
                for (auto & value : scaled_data) {
                    value = static_cast<float>(value / stats.std_dev);
                }
            }
            break;
        }
        
        case ScalingMethod::Centering: {
            for (auto & value : scaled_data) {
                value = static_cast<float>(value - stats.mean);
            }
            break;
        }
    }
    
    return std::make_shared<AnalogTimeSeries>(scaled_data, time_data);
}

///////////////////////////////////////////////////////////////////////////////

std::string AnalogScalingOperation::getName() const {
    return "Scale and Normalize";
}

std::type_index AnalogScalingOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool AnalogScalingOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant)) {
        return false;
    }
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> AnalogScalingOperation::getDefaultParameters() const {
    return std::make_unique<AnalogScalingParams>();
}

DataTypeVariant AnalogScalingOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int){});
}

DataTypeVariant AnalogScalingOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters,
                                               ProgressCallback progressCallback) {
    
    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);
    
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "AnalogScalingOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }
    
    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();
    
    AnalogScalingParams currentParams;
    
    if (transformParameters != nullptr) {
        AnalogScalingParams const * specificParams = dynamic_cast<AnalogScalingParams const *>(transformParameters);
        
        if (specificParams) {
            currentParams = *specificParams;
            std::cout << "Using scaling parameters provided by UI." << std::endl;
        } else {
            std::cerr << "Warning: AnalogScalingOperation received incompatible parameter type! Using default parameters." << std::endl;
        }
    } else {
        std::cout << "AnalogScalingOperation received null parameters. Using default parameters." << std::endl;
    }
    
    progressCallback(25);
    
    std::shared_ptr<AnalogTimeSeries> result = scale_analog_time_series(analog_raw_ptr, currentParams);
    
    progressCallback(75);
    
    if (!result) {
        std::cerr << "AnalogScalingOperation::execute: scaling failed to produce a result." << std::endl;
        return {};
    }
    
    progressCallback(100);
    
    std::cout << "AnalogScalingOperation executed successfully." << std::endl;
    return result;
} 
