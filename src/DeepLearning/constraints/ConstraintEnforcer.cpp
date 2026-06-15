/// @file ConstraintEnforcer.cpp
/// @brief Implementation of session constraint computation functions.

#include "constraints/ConstraintEnforcer.hpp"

#include "models_v2/ModelInfo.hpp"                    // dl::ModelInfo
#include "models_v2/TensorSlotDescriptor.hpp"         // dl::isBatchLocked, min/maxBatchSizeFromMode
#include "post_encoder/PostEncoderModuleRegistry.hpp" // dl::PostEncoderModuleRegistry

#include <algorithm> // std::any_of

namespace dl::constraints {

namespace {

[[nodiscard]] std::vector<std::string> allDecoderAlternatives() {
    return {"MaskDecoderParams", "PointDecoderParams", "LineDecoderParams",
            "FeatureVectorDecoderParams"};
}

}// namespace

BatchSizeConstraint computeBatchSizeConstraint(
        dl::ModelInfo const & info,
        std::vector<RecurrentBindingData> const & active_recurrent_bindings) {
    auto const & mode = info.batch_mode;
    bool const model_locked = dl::isBatchLocked(mode);

    bool const has_recurrent = std::any_of(
            active_recurrent_bindings.begin(),
            active_recurrent_bindings.end(),
            [](RecurrentBindingData const & rb) {
                return !rb.output_slot_name.empty();
            });

    if (has_recurrent || model_locked) {
        // Lock to batch=1; record whether it was due to recurrent bindings or
        // an inherent model constraint.
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
