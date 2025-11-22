#ifndef WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP

#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/examples/MaskAreaTransform.hpp"
#include "transforms/v2/examples/SumReductionTransform.hpp"

#include "CoreGeometry/masks.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Compile-Time Transform Registration
// ============================================================================

namespace {

// Register MaskAreaTransform
inline auto const register_mask_area = RegisterTransform<Mask2D, float, MaskAreaParams>(
    "CalculateMaskArea",
    calculateMaskArea,
    TransformMetadata{
        .name = "CalculateMaskArea",
        .description = "Calculate the area of a mask in pixels",
        .category = "Image Processing",
        .input_type = typeid(Mask2D),
        .output_type = typeid(float),
        .params_type = typeid(MaskAreaParams),
        .input_type_name = "Mask2D",
        .output_type_name = "float",
        .params_type_name = "MaskAreaParams",
        .is_expensive = false,
        .is_deterministic = true,
        .supports_cancellation = false
    }
);

// Register context-aware version
inline auto const register_mask_area_ctx = RegisterContextTransform<Mask2D, std::vector<float>, MaskAreaParams>(
    "CalculateMaskAreaWithContext",
    calculateMaskAreaWithContext,
    TransformMetadata{
        .name = "CalculateMaskAreaWithContext",
        .description = "Calculate the area of a mask with progress reporting",
        .category = "Image Processing",
        .input_type = typeid(Mask2D),
        .output_type = typeid(std::vector<float>),
        .params_type = typeid(MaskAreaParams),
        .input_type_name = "Mask2D",
        .output_type_name = "std::vector<float>",
        .params_type_name = "MaskAreaParams",
        .is_expensive = false,
        .is_deterministic = true,
        .supports_cancellation = true
    }
);

// Register SumReductionTransform (time-grouped)
inline auto const register_sum_reduction = RegisterTimeGroupedTransform<float, float, SumReductionParams>(
    "SumReduction",
    sumReduction,
    TransformMetadata{
        .name = "SumReduction",
        .description = "Sum all float values at a time point into a single value",
        .category = "Statistics",
        .input_type = typeid(float),
        .output_type = typeid(float),
        .params_type = typeid(SumReductionParams),
        .is_time_grouped = true,
        .input_type_name = "float",
        .output_type_name = "float",
        .params_type_name = "SumReductionParams",
        .is_expensive = false,
        .is_deterministic = true,
        .supports_cancellation = false
    }
);

// Register context-aware version
inline auto const register_sum_reduction_ctx = RegisterContextTimeGroupedTransform<float, float, SumReductionParams>(
    "SumReductionWithContext",
    sumReductionWithContext,
    TransformMetadata{
        .name = "SumReductionWithContext",
        .description = "Sum all float values with progress reporting",
        .category = "Statistics",
        .input_type = typeid(float),
        .output_type = typeid(float),
        .params_type = typeid(SumReductionParams),
        .is_time_grouped = true,
        .input_type_name = "float",
        .output_type_name = "float",
        .params_type_name = "SumReductionParams",
        .is_expensive = false,
        .is_deterministic = true,
        .supports_cancellation = true
    }
);

} // anonymous namespace

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
