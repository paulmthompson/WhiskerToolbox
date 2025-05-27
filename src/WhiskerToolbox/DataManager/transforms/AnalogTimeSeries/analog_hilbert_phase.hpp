#ifndef ANALOG_HILBERT_PHASE_HPP
#define ANALOG_HILBERT_PHASE_HPP


#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeries;

struct HilbertPhaseParams : public TransformParametersBase {
    double lowFrequency = 5.0;  // Low cutoff frequency in Hz
    double highFrequency = 15.0; // High cutoff frequency in Hz
};

/**
 * @brief Calculates phase of analog time series by hilbert transform
 *
 * @param analog_time_series The AnalogTimeSeries to process.
 * @param threshold The threshold value for event detection.
 * @return A new AnalogTimeSeries containing detected events.
 */
std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams);


class HilbertPhaseOperation final : public TransformOperation {

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null AnalogTimeSeries shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Executes the phase calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     * or an empty on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;
};


#endif// ANALOG_HILBERT_PHASE_HPP
