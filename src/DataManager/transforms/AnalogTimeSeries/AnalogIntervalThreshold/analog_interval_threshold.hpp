#ifndef WHISKERTOOLBOX_ANALOG_INTERVAL_THRESHOLD_HPP
#define WHISKERTOOLBOX_ANALOG_INTERVAL_THRESHOLD_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeries;
class DigitalIntervalSeries;

struct IntervalThresholdParams : public TransformParametersBase {
    double thresholdValue = 1.0;
    enum class ThresholdDirection { POSITIVE,
                                    NEGATIVE,
                                    ABSOLUTE } direction = ThresholdDirection::POSITIVE;
    double lockoutTime = 0.0;
    double minDuration = 0.0;
    enum class MissingDataMode { IGNORE,                                                          // Current behavior: skip missing time points
                                 TREAT_AS_ZERO } missingDataMode = MissingDataMode::TREAT_AS_ZERO;// Default: treat missing as zero
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Detects intervals in an AnalogTimeSeries based on a threshold.
 * 
 * This function analyzes an analog time series and identifies continuous intervals
 * where the signal meets specified threshold criteria. It supports positive, negative,
 * and absolute value thresholding with configurable lockout time and minimum duration
 * requirements.
 * 
 * Missing data handling: When time indices are not consecutive (indicating missing samples),
 * the behavior depends on the missingDataMode parameter:
 * - TREAT_AS_ZERO (default): Missing time points are treated as having zero values
 * - IGNORE: Missing time points are skipped (original behavior)
 *
 * @param analog_time_series The AnalogTimeSeries to process. Must not be null.
 * @param thresholdParams Parameters containing threshold value, direction, lockout time, 
 *                        minimum duration, and missing data handling mode.
 * @return A new DigitalIntervalSeries containing detected intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams);

/**
 * @brief Detects intervals in an AnalogTimeSeries based on a threshold with progress reporting.
 * 
 * This function analyzes an analog time series and identifies continuous intervals
 * where the signal meets specified threshold criteria. It supports positive, negative,
 * and absolute value thresholding with configurable lockout time and minimum duration
 * requirements. Progress is reported through the provided callback.
 * 
 * Missing data handling: When time indices are not consecutive (indicating missing samples),
 * the behavior depends on the missingDataMode parameter:
 * - TREAT_AS_ZERO (default): Missing time points are treated as having zero values
 * - IGNORE: Missing time points are skipped (original behavior)
 *
 * @param analog_time_series The AnalogTimeSeries to process. Must not be null.
 * @param thresholdParams Parameters containing threshold value, direction, lockout time, 
 *                        minimum duration, and missing data handling mode.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new DigitalIntervalSeries containing detected intervals.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<DigitalIntervalSeries> interval_threshold(
        AnalogTimeSeries const * analog_time_series,
        IntervalThresholdParams const & thresholdParams,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class IntervalThresholdOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null AnalogTimeSeries shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Executes the interval detection using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the interval detection (IntervalThresholdParams).
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the interval detection with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the interval detection (IntervalThresholdParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalIntervalSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_ANALOG_INTERVAL_THRESHOLD_HPP
