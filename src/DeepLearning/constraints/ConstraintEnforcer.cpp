/// @file ConstraintEnforcer.cpp
/// @brief Implementation of session constraint computation functions.

#include "constraints/ConstraintEnforcer.hpp"

#include "models_v2/ModelInfo.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"
#include "post_encoder/PostEncoderModuleRegistry.hpp"

namespace dl::constraints {

namespace {

[[nodiscard]] std::vector<std::string> allDecoderAlternatives() {
    return {"MaskDecoderParams", "PointDecoderParams", "LineDecoderParams",
            "FeatureVectorDecoderParams"};
}

}// namespace

BatchSizeConstraint computeBatchSizeConstraint(
        dl::ModelInfo const & info,
        std::vector<MemoryFrameBinding> const & memory_frames) {
    auto const & mode = info.batch_mode;
    bool const model_locked = dl::isBatchLocked(mode);

    bool const has_recurrent = hasActiveRecurrentBindings(memory_frames);

    if (has_recurrent || model_locked) {
        return BatchSizeConstraint{1, 1, has_recurrent};
    }

    return BatchSizeConstraint{
            dl::minBatchSizeFromMode(mode),
            dl::maxBatchSizeFromMode(mode),
            false};
}

std::vector<std::string> validDecodersForPostEncoder(
        dl::PostEncoderStepDescriptor const & params) {
    if (dl::PostEncoderModuleRegistry::instance().collapsesSpatialDims(
                params.module_key)) {
        return {"FeatureVectorDecoderParams"};
    }
    return allDecoderAlternatives();
}

}// namespace dl::constraints
