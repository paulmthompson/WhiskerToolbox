
#include "analog_hilbert_phase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

std::shared_ptr<AnalogTimeSeries> hilbert_phase(
        AnalogTimeSeries const * analog_time_series,
        HilbertPhaseParams const & phaseParams) {
    auto phase_series = std::make_shared<AnalogTimeSeries>();

    return phase_series;
}

std::string HilbertPhaseOperation::getName() const {
    return "Hilbert Phase";
}

std::type_index HilbertPhaseOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<AnalogTimeSeries>);
}

bool HilbertPhaseOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    // Return true only if get_if succeeded AND the contained shared_ptr is not null.
    return ptr_ptr && *ptr_ptr;
}

DataTypeVariant HilbertPhaseOperation::execute(DataTypeVariant const & dataVariant, TransformParametersBase const * transformParameters) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<AnalogTimeSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "HilbertPhaseOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};// Return empty
    }

    AnalogTimeSeries const * analog_raw_ptr = (*ptr_ptr).get();

    HilbertPhaseParams currentParams;

    if (transformParameters != nullptr) {
        HilbertPhaseParams const * specificParams = dynamic_cast<HilbertPhaseParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            std::cout << "Using parameters provided by UI." << std::endl;
        } else {
            std::cerr << "Warning: IntervalThresholdOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
        }
    } else {
        // No parameter object was provided (nullptr). This might be expected if the
        // operation can run with defaults or was called programmatically.
        std::cout << "HilbertPhaseOperation received null parameters. Using default parameters." << std::endl;
    }

    std::shared_ptr<AnalogTimeSeries> result = hilbert_phase(analog_raw_ptr,
                                                             currentParams);

    if (!result) {
        std::cerr << "HilbertPhaseOperation::execute: 'calculate_intervals' failed to produce a result." << std::endl;
        return {};// Return empty
    }

    std::cout << "HilbertPhaseOperation executed successfully using variant input." << std::endl;
    return result;
}
