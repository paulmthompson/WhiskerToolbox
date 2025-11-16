#ifndef ANALOG_SCALING_HPP
#define ANALOG_SCALING_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeriesInMemory;
using AnalogTimeSeries = AnalogTimeSeriesInMemory;

enum class ScalingMethod {
    FixedGain,           // Multiply by constant factor
    ZScore,              // (x - mean) / std
    StandardDeviation,   // Scale so X std devs = 1.0
    MinMax,              // Scale to [0, 1] range
    RobustScaling,       // (x - median) / IQR
    UnitVariance,        // Scale to unit variance (std = 1)
    Centering            // Subtract mean (center around 0)
};

struct AnalogScalingParams : public TransformParametersBase {
    ScalingMethod method = ScalingMethod::ZScore;
    
    // For FixedGain
    double gain_factor = 1.0;
    
    // For StandardDeviation scaling
    double std_dev_target = 3.0;  // Scale so this many std devs = 1.0
    
    // For MinMax scaling
    double min_target = 0.0;
    double max_target = 1.0;
    
    // For RobustScaling
    double quantile_low = 0.25;   // First quartile
    double quantile_high = 0.75;  // Third quartile
};

///////////////////////////////////////////////////////////////////////////////

struct AnalogStatistics {
    double mean = 0.0;
    double std_dev = 0.0;
    double min_val = 0.0;
    double max_val = 0.0;
    double median = 0.0;
    double q1 = 0.0;  // First quartile
    double q3 = 0.0;  // Third quartile
    double iqr = 0.0; // Interquartile range
    size_t sample_count = 0;
};

/**
 * @brief Calculate comprehensive statistics for an AnalogTimeSeries
 * @param analog_time_series The AnalogTimeSeries to analyze
 * @return AnalogStatistics structure with computed statistics
 */
AnalogStatistics calculate_analog_statistics(AnalogTimeSeries const * analog_time_series);

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Apply scaling/normalization to an AnalogTimeSeries
 * @param analog_time_series The AnalogTimeSeries to scale
 * @param params The scaling parameters
 * @return A new scaled AnalogTimeSeries
 */
std::shared_ptr<AnalogTimeSeries> scale_analog_time_series(
        AnalogTimeSeries const * analog_time_series,
        AnalogScalingParams const & params);

///////////////////////////////////////////////////////////////////////////////

class AnalogScalingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters) override;
                           
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                           TransformParametersBase const * transformParameters,
                           ProgressCallback progressCallback) override;
};

#endif // ANALOG_SCALING_HPP 