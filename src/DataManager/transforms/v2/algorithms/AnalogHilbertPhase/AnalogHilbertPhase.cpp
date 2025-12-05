#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"  // V1 implementation
#include "transforms/v2/core/ComputeContext.hpp"

#include <iostream>

namespace WhiskerToolbox::Transforms::V2::Examples {

std::shared_ptr<AnalogTimeSeries> analogHilbertPhase(
    AnalogTimeSeries const& input,
    AnalogHilbertPhaseParams const& params,
    ComputeContext const& ctx) {
    
    // Convert V2 params to V1 HilbertPhaseParams
    HilbertPhaseParams v1Params;
    
    // Map output type
    if (params.isAmplitudeOutput()) {
        v1Params.outputType = HilbertPhaseParams::OutputType::Amplitude;
    } else {
        v1Params.outputType = HilbertPhaseParams::OutputType::Phase;
    }
    
    // Map all other parameters
    v1Params.discontinuityThreshold = params.getDiscontinuityThreshold();
    v1Params.maxChunkSize = params.getMaxChunkSize();
    v1Params.overlapFraction = params.getOverlapFraction();
    v1Params.useWindowing = params.getUseWindowing();
    v1Params.applyBandpassFilter = params.getApplyBandpassFilter();
    v1Params.filterLowFreq = params.getFilterLowFreq();
    v1Params.filterHighFreq = params.getFilterHighFreq();
    v1Params.filterOrder = params.getFilterOrder();
    v1Params.samplingRate = params.getSamplingRate();
    
    // Create progress callback that respects cancellation
    ProgressCallback progressCallback = [&ctx](int progress) {
        ctx.reportProgress(progress);
    };
    
    // Check for cancellation before starting
    if (ctx.shouldCancel()) {
        return std::make_shared<AnalogTimeSeries>();
    }
    
    // Call the V1 implementation
    auto result = hilbert_phase(&input, v1Params, progressCallback);
    
    // Ensure 100% is reported at the end
    ctx.reportProgress(100);
    
    return result;
}

} // namespace WhiskerToolbox::Transforms::V2::Examples
