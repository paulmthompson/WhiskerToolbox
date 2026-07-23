#include "AnalogHilbertPhase.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "core/ComputeContext.hpp"
#include "transforms/AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"// V1 implementation

namespace Neuralyzer::Transforms::V2::Examples {

std::shared_ptr<AnalogTimeSeries> analogHilbertPhase(
        AnalogTimeSeries const & input,
        AnalogHilbertPhaseParams const & params,
        ComputeContext const & ctx) {

    HilbertPhaseParams v1Params;

    if (params.output_type == AnalogHilbertPhaseParams::OutputType::amplitude) {
        v1Params.outputType = HilbertPhaseParams::OutputType::Amplitude;
    } else {
        v1Params.outputType = HilbertPhaseParams::OutputType::Phase;
    }

    v1Params.discontinuityThreshold = params.discontinuity_threshold.value();
    v1Params.maxChunkSize = params.max_chunk_size;
    v1Params.overlapFraction = params.overlap_fraction.value();
    v1Params.useWindowing = params.use_windowing;
    v1Params.applyBandpassFilter = params.apply_bandpass_filter;
    v1Params.filterLowFreq = params.filter_low_freq.value();
    v1Params.filterHighFreq = params.filter_high_freq.value();
    v1Params.filterOrder = params.filter_order.value();
    v1Params.samplingRate = params.sampling_rate.value();

    ProgressCallback progressCallback = [&ctx](int progress) {
        ctx.reportProgress(progress);
    };

    if (ctx.shouldCancel()) {
        return std::make_shared<AnalogTimeSeries>();
    }

    auto result = hilbert_phase(&input, v1Params, progressCallback);

    ctx.reportProgress(100);

    return result;
}

}// namespace Neuralyzer::Transforms::V2::Examples
