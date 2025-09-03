#ifndef ANALOG_HILBERT_PHASE_HPP
#define ANALOG_HILBERT_PHASE_HPP


#include "transforms/data_transforms.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index

class AnalogTimeSeries;

struct HilbertPhaseParams : public TransformParametersBase {
    double lowFrequency = 5.0;           // Low cutoff frequency in Hz
    double highFrequency = 15.0;         // High cutoff frequency in Hz
    size_t discontinuityThreshold = 1000;// Gap size (in samples) above which to split processing into chunks
};

/**
 * @brief Calculates the instantaneous phase of an analog time series using the Hilbert transform.
 * 
 * This function computes the analytic signal of the input time series using the Hilbert transform,
 * then extracts the instantaneous phase. The phase values are returned in radians, ranging from
 * -π to π. The function uses FFT-based computation for efficiency and automatically detects
 * discontinuities to process continuous segments separately for better performance.
 *
 * @param analog_time_series The AnalogTimeSeries to process. Must not be null.
 * @param phaseParams Parameters containing frequency band limits and discontinuity threshold.
 * @return A new AnalogTimeSeries containing the instantaneous phase values in radians.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams);

/**
 * @brief Calculates the instantaneous phase of an analog time series using the Hilbert transform with progress reporting.
 * 
 * This function computes the analytic signal of the input time series using the Hilbert transform,
 * then extracts the instantaneous phase. The phase values are returned in radians, ranging from
 * -π to π. The function uses FFT-based computation for efficiency, reports progress through
 * the provided callback, and automatically detects discontinuities to process continuous segments 
 * separately for better performance.
 *
 * @param analog_time_series The AnalogTimeSeries to process. Must not be null.
 * @param phaseParams Parameters containing frequency band limits and discontinuity threshold.
 * @param progressCallback Function called with progress percentage (0-100) during computation.
 * @return A new AnalogTimeSeries containing the instantaneous phase values in radians.
 *         Returns an empty series if input is null or empty.
 */
std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams,
        ProgressCallback progressCallback);


class HilbertPhaseOperation final : public TransformOperation {

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null AnalogTimeSeries shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the Hilbert phase calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the phase calculation (HilbertPhaseParams).
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the Hilbert phase calculation with progress reporting.
     * @param dataVariant The variant holding a non-null shared_ptr to the AnalogTimeSeries object.
     * @param transformParameters Parameters for the phase calculation (HilbertPhaseParams).
     * @param progressCallback Function called with progress percentage (0-100) during computation.
     * @return DataTypeVariant containing a std::shared_ptr<AnalogTimeSeries> on success,
     *         or an empty variant on failure (e.g., type mismatch, null pointer, calculation failure).
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};


#endif// ANALOG_HILBERT_PHASE_HPP
