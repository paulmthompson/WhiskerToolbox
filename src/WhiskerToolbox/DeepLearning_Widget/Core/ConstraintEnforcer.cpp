/// @file ConstraintEnforcer.cpp
/// @brief Implementation of pure constraint computation functions.

#include "DeepLearning_Widget/Core/ConstraintEnforcer.hpp"

#include "models_v2/TensorSlotDescriptor.hpp"// dl::isBatchLocked, FixedBatch, DynamicBatch

#include <algorithm>// std::any_of

namespace dl::constraints {

BatchSizeConstraint computeBatchSizeConstraint(
        ModelDisplayInfo const & info,
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

    if (auto const * f = std::get_if<dl::FixedBatch>(&mode)) {
        return BatchSizeConstraint{f->size, f->size, false};
    }

    // DynamicBatch: expose the model-reported range
    auto const & d = std::get<dl::DynamicBatch>(mode);
    return BatchSizeConstraint{d.min_size, d.max_size, false};
}

std::vector<std::string> validDecodersForModule(std::string const & module_type) {
    // Modules that collapse spatial dimensions ([B,C,H,W] → [B,C]) can only
    // be decoded by the feature-vector decoder.
    bool const spatial_dims_removed =
            (module_type == "global_avg_pool" || module_type == "spatial_point");

    if (spatial_dims_removed) {
        return {"FeatureVectorDecoderParams"};
    }

    return {"MaskDecoderParams", "PointDecoderParams", "LineDecoderParams",
            "FeatureVectorDecoderParams"};
}

}// namespace dl::constraints
