#ifndef WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_HPP
#define WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_HPP

#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeries;
class DigitalEventSeries;

struct ThresholdParams : public TransformParametersBase {
    double thresholdValue = 1.0;
    enum class ThresholdDirection { POSITIVE, NEGATIVE, ABSOLUTE } direction = ThresholdDirection::POSITIVE;
    double lockoutTime = 0.0;
};

/**
 * @brief Detects events in an AnalogTimeSeries based on a threshold.
 *
 * @param analog_time_series The AnalogTimeSeries to process.
 * @param threshold The threshold value for event detection.
 * @return A new DigitalEventSeries containing detected events.
 */
std::shared_ptr<DigitalEventSeries> event_threshold(
        AnalogTimeSeries const * analog_time_series,
        ThresholdParams const & thresholdParams);


class EventThresholdOperation final : public TransformOperation {

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null AnalogTimeSeries shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Executes the mask area calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @return DataTypeVariant containing a std::shared_ptr<DigitalEventSeries> on success,
     * or an empty on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
};


#endif//WHISKERTOOLBOX_ANALOG_EVENT_THRESHOLD_HPP
