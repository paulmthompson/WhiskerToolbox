#ifndef WHISKERTOOLBOX_ANALOG_INTERVAL_PEAK_HPP
#define WHISKERTOOLBOX_ANALOG_INTERVAL_PEAK_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;

struct IntervalPeakParams : public TransformParametersBase {
    enum class PeakType {
        MINIMUM,  // Find minimum value within each interval
        MAXIMUM   // Find maximum value within each interval
    };

    enum class SearchMode {
        WITHIN_INTERVALS,      // Search from interval start to interval end
        BETWEEN_INTERVAL_STARTS // Search from one interval start to the next interval start
    };

    PeakType peak_type = PeakType::MAXIMUM;
    SearchMode search_mode = SearchMode::WITHIN_INTERVALS;
    std::shared_ptr<DigitalIntervalSeries> interval_series;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Finds peak (min/max) values in an AnalogTimeSeries within intervals.
 *
 * This function searches for minimum or maximum values in the analog signal
 * within time ranges defined by a DigitalIntervalSeries. The result is a
 * DigitalEventSeries where each event marks the timestamp of a detected peak.
 *
 * The function automatically handles timeframe conversion: if the DigitalIntervalSeries
 * has a timeframe set, it will be used to convert the interval coordinates to the
 * AnalogTimeSeries coordinate system. The returned events are in the interval series'
 * coordinate system.
 *
 * @param analog_time_series The AnalogTimeSeries to search for peaks.
 * @param intervalPeakParams Parameters specifying peak type, search mode, and intervals.
 * @return A new DigitalEventSeries containing the timestamps of detected peaks.
 *         Returns an empty series if inputs are null or no peaks are found.
 */
std::shared_ptr<DigitalEventSeries> find_interval_peaks(
        AnalogTimeSeries const * analog_time_series,
        IntervalPeakParams const & intervalPeakParams);

/**
 * @brief Finds peak values with progress reporting.
 *
 * @param analog_time_series The AnalogTimeSeries to search for peaks.
 * @param intervalPeakParams Parameters specifying peak type, search mode, and intervals.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new DigitalEventSeries containing the timestamps of detected peaks.
 */
std::shared_ptr<DigitalEventSeries> find_interval_peaks(
        AnalogTimeSeries const * analog_time_series,
        IntervalPeakParams const & intervalPeakParams,
        ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

class AnalogIntervalPeakOperation final : public TransformOperation {
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
     * @brief Gets default parameters for the interval peak operation.
     * @return Default IntervalPeakParams with peak_type = MAXIMUM, search_mode = WITHIN_INTERVALS.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the interval peak detection using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the operation (IntervalPeakParams).
     * @return DataTypeVariant containing a std::shared_ptr<DigitalEventSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the interval peak detection with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the operation (IntervalPeakParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalEventSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif//WHISKERTOOLBOX_ANALOG_INTERVAL_PEAK_HPP
