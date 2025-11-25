#ifndef WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP

#include "transforms/v2/algorithms/LineMinPointDist/LineMinPointDist.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"

#include "CoreGeometry/masks.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Pipeline Step Factory Registration
// ============================================================================
//
// Register PipelineStep factories for all parameter types.
// This must happen after PipelineStep is fully defined (from PipelineLoader.hpp include).
//
// Note: JSON deserializers and typed executors are registered automatically
// when transforms are registered via ElementRegistry::registerTransform().
// ============================================================================

namespace {

// Register PipelineStep factories for all parameter types used in this file
inline bool const init_pipeline_factories = []() {
    registerPipelineStepFactoryFor<NoParams>();
    registerPipelineStepFactoryFor<MaskAreaParams>();
    registerPipelineStepFactoryFor<SumReductionParams>();
    registerPipelineStepFactoryFor<LineMinPointDistParams>();
    return true;
}();

}// anonymous namespace

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
                .supports_cancellation = false});

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
                .supports_cancellation = true});

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
                .supports_cancellation = false});

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
                .supports_cancellation = true});

// Register LineMinPointDistTransform (Binary - takes two inputs)
inline auto const register_line_min_point_dist = RegisterBinaryTransform<
        Line2D,
        Point2D<float>,
        float,
        LineMinPointDistParams>(
        "CalculateLineMinPointDistance",
        calculateLineMinPointDistance,
        TransformMetadata{
                .name = "CalculateLineMinPointDistance",
                .description = "Calculate distance from a point to a line (1:1 matching)",
                .category = "Geometry",
                .input_type = typeid(std::tuple<Line2D, Point2D<float>>),
                .output_type = typeid(float),
                .params_type = typeid(LineMinPointDistParams),
                .is_multi_input = true,
                .input_arity = 2,
                .input_type_name = "std::tuple<Line2D, Point2D<float>>",
                .output_type_name = "float",
                .params_type_name = "LineMinPointDistParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false,
});

}// anonymous namespace

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
