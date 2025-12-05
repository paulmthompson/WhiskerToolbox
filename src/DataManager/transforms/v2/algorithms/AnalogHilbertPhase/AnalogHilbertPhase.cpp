#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"
#include "transforms/v2/core/ComputeContext.hpp"

#include <iostream>

namespace WhiskerToolbox::Transforms::V2::Examples {

std::shared_ptr<AnalogTimeSeries> analogHilbertPhase(
    AnalogTimeSeries const& input,
    AnalogHilbertPhaseParams const& params,
    ComputeContext const& ctx) {
    
    // Validate parameters
    if (!params.isValidOutputType()) {
        std::cerr << "Invalid output type parameter: " << params.getOutputType() << std::endl;
        return std::make_shared<AnalogTimeSeries>();
    }
    
    // Convert v2 params to v1 params
    HilbertPhaseParams v1_params;
    v1_params.discontinuityThreshold = params.getDiscontinuityThreshold();
    v1_params.maxChunkSize = params.getMaxChunkSize();
    v1_params.overlapFraction = params.getOverlapFraction();
    v1_params.useWindowing = params.getUseWindowing();
    v1_params.applyBandpassFilter = params.getApplyBandpassFilter();
    v1_params.filterLowFreq = params.getFilterLowFreq();
    v1_params.filterHighFreq = params.getFilterHighFreq();
    v1_params.filterOrder = params.getFilterOrder();
    v1_params.samplingRate = params.getSamplingRate();
    
    // Set output type
    if (params.getOutputType() == "amplitude") {
        v1_params.outputType = HilbertPhaseParams::OutputType::Amplitude;
    } else {
        v1_params.outputType = HilbertPhaseParams::OutputType::Phase;
    }
    
    // Create progress callback that integrates with v2 context
    ProgressCallback progress_callback = [&ctx](int progress) {
        ctx.reportProgress(progress);
    };
    
    // Check for cancellation before starting
    if (ctx.shouldCancel()) {
        return std::make_shared<AnalogTimeSeries>();
    }
    
    // Call the v1 implementation with progress callback
    auto result = hilbert_phase(&input, v1_params, progress_callback);
    
    // Copy TimeFrame from input to output if available
    if (result && input.getTimeFrame()) {
        result->setTimeFrame(input.getTimeFrame());
    }
    
    return result;
}

} // namespace WhiskerToolbox::Transforms::V2::Examples
