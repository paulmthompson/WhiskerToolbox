#include "transforms/v2/core/RegisteredTransforms.hpp"

#include "CoreGeometry/masks.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "transforms/v2/algorithms/AnalogEventThreshold/AnalogEventThreshold.hpp"
#include "transforms/v2/algorithms/AnalogIntervalPeak/AnalogIntervalPeak.hpp"
#include "transforms/v2/algorithms/AnalogIntervalThreshold/AnalogIntervalThreshold.hpp"
#include "transforms/v2/algorithms/DigitalIntervalBoolean/DigitalIntervalBoolean.hpp"
#include "transforms/v2/algorithms/LineAngle/LineAngle.hpp"
#include "transforms/v2/algorithms/LineBaseFlip/LineBaseFlip.hpp"
#include "transforms/v2/algorithms/LineMinPointDist/LineMinPointDist.hpp"
#include "transforms/v2/algorithms/LineResample/LineResample.hpp"
#include "transforms/v2/algorithms/LineSubsegment/LineSubsegment.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"

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
bool const init_pipeline_factories = []() {
    registerPipelineStepFactoryFor<NoParams>();
    registerPipelineStepFactoryFor<MaskAreaParams>();
    registerPipelineStepFactoryFor<SumReductionParams>();
    registerPipelineStepFactoryFor<LineAngleParams>();
    registerPipelineStepFactoryFor<LineBaseFlipParams>();
    registerPipelineStepFactoryFor<LineMinPointDistParams>();
    registerPipelineStepFactoryFor<LineResampleParams>();
    registerPipelineStepFactoryFor<LineSubsegmentParams>();
    registerPipelineStepFactoryFor<AnalogEventThresholdParams>();
    registerPipelineStepFactoryFor<AnalogIntervalPeakParams>();
    registerPipelineStepFactoryFor<AnalogIntervalThresholdParams>();
    registerPipelineStepFactoryFor<DigitalIntervalBooleanParams>();
    registerPipelineStepFactoryFor<ZScoreNormalizationParams>();
    return true;
}();

}// anonymous namespace

// ============================================================================
// Compile-Time Transform Registration
// ============================================================================

namespace {

// Register MaskAreaTransform
auto const register_mask_area = RegisterTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea",
        calculateMaskArea,
        TransformMetadata{
                .name = "CalculateMaskArea",
                .description = "Calculate the area of a mask in pixels",
                .category = "Image Processing",
                .input_type = typeid(Mask2D),
                .output_type = typeid(float),
                .params_type = typeid(MaskAreaParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Mask2D",
                .output_type_name = "float",
                .params_type_name = "MaskAreaParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version
auto const register_mask_area_ctx = RegisterContextTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskAreaWithContext",
        calculateMaskAreaWithContext,
        TransformMetadata{
                .name = "CalculateMaskAreaWithContext",
                .description = "Calculate the area of a mask with progress reporting",
                .category = "Image Processing",
                .input_type = typeid(Mask2D),
                .output_type = typeid(float),
                .params_type = typeid(MaskAreaParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Mask2D",
                .output_type_name = "float",
                .params_type_name = "MaskAreaParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register SumReductionTransform (time-grouped)
auto const register_sum_reduction = RegisterTimeGroupedTransform<float, float, SumReductionParams>(
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
                .produces_single_output = true,
                .lineage_type = TransformLineageType::AllToOneByTime,
                .input_type_name = "float",
                .output_type_name = "float",
                .params_type_name = "SumReductionParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version
auto const register_sum_reduction_ctx = RegisterContextTimeGroupedTransform<float, float, SumReductionParams>(
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
                .lineage_type = TransformLineageType::AllToOneByTime,
                .input_type_name = "float",
                .output_type_name = "float",
                .params_type_name = "SumReductionParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register LineMinPointDistTransform (Binary - takes two inputs)
auto const register_line_min_point_dist = RegisterBinaryTransform<
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
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "std::tuple<Line2D, Point2D<float>>",
                .output_type_name = "float",
                .params_type_name = "LineMinPointDistParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false,
        });

// Register LineAngleTransform (Unary - takes Line2D, returns float)
auto const register_line_angle = RegisterTransform<Line2D, float, LineAngleParams>(
        "CalculateLineAngle",
        calculateLineAngle,
        TransformMetadata{
                .name = "CalculateLineAngle",
                .description = "Calculate the angle at a position along a line",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(float),
                .params_type = typeid(LineAngleParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "float",
                .params_type_name = "LineAngleParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version of LineAngle
auto const register_line_angle_ctx = RegisterContextTransform<Line2D, float, LineAngleParams>(
        "CalculateLineAngleWithContext",
        calculateLineAngleWithContext,
        TransformMetadata{
                .name = "CalculateLineAngleWithContext",
                .description = "Calculate the angle at a position along a line with progress reporting",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(float),
                .params_type = typeid(LineAngleParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "float",
                .params_type_name = "LineAngleParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register LineBaseFlipTransform (Unary - takes Line2D, returns Line2D)
auto const register_line_base_flip = RegisterTransform<Line2D, Line2D, LineBaseFlipParams>(
        "FlipLineBase",
        flipLineBase,
        TransformMetadata{
                .name = "FlipLineBase",
                .description = "Flip line orientation based on distance to a reference point",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineBaseFlipParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineBaseFlipParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version of LineBaseFlip
auto const register_line_base_flip_ctx = RegisterContextTransform<Line2D, Line2D, LineBaseFlipParams>(
        "FlipLineBaseWithContext",
        flipLineBaseWithContext,
        TransformMetadata{
                .name = "FlipLineBaseWithContext",
                .description = "Flip line orientation based on distance to a reference point with progress reporting",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineBaseFlipParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineBaseFlipParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register LineResampleTransform (Unary - takes Line2D, returns Line2D)
auto const register_line_resample = RegisterTransform<Line2D, Line2D, LineResampleParams>(
        "ResampleLine",
        resampleLine,
        TransformMetadata{
                .name = "ResampleLine",
                .description = "Resample or simplify a line using FixedSpacing or Douglas-Peucker algorithm",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineResampleParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineResampleParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version of LineResample
auto const register_line_resample_ctx = RegisterContextTransform<Line2D, Line2D, LineResampleParams>(
        "ResampleLineWithContext",
        resampleLineWithContext,
        TransformMetadata{
                .name = "ResampleLineWithContext",
                .description = "Resample or simplify a line with progress reporting",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineResampleParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineResampleParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register LineSubsegment (Unary - takes Line2D, returns Line2D)
auto const register_line_subsegment = RegisterTransform<Line2D, Line2D, LineSubsegmentParams>(
        "ExtractLineSubsegment",
        extractLineSubsegment,
        TransformMetadata{
                .name = "ExtractLineSubsegment",
                .description = "Extract a subsegment from a line between specified fractional positions using Direct or Parametric method",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineSubsegmentParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineSubsegmentParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register context-aware version of LineSubsegment
auto const register_line_subsegment_ctx = RegisterContextTransform<Line2D, Line2D, LineSubsegmentParams>(
        "ExtractLineSubsegmentWithContext",
        extractLineSubsegmentWithContext,
        TransformMetadata{
                .name = "ExtractLineSubsegmentWithContext",
                .description = "Extract a line subsegment with progress reporting",
                .category = "Geometry",
                .input_type = typeid(Line2D),
                .output_type = typeid(Line2D),
                .params_type = typeid(LineSubsegmentParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "Line2D",
                .output_type_name = "Line2D",
                .params_type_name = "LineSubsegmentParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

// Register ZScoreNormalization (Multi-Pass Element Transform)
auto const register_zscore_normalization = RegisterTransform<float, float, ZScoreNormalizationParams>(
        "ZScoreNormalization",
        zScoreNormalization,
        TransformMetadata{
                .name = "ZScoreNormalization",
                .description = "Normalize values to z-scores (mean=0, std=1) using multi-pass statistics computation",
                .category = "Statistics",
                .input_type = typeid(float),
                .output_type = typeid(float),
                .params_type = typeid(ZScoreNormalizationParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "float",
                .output_type_name = "float",
                .params_type_name = "ZScoreNormalizationParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// Register AnalogEventThreshold (Container Transform)
void registerAnalogEventThreshold() {
    auto & registry = ElementRegistry::instance();
    registry.registerContainerTransform<AnalogTimeSeries, DigitalEventSeries, AnalogEventThresholdParams>(
            "AnalogEventThreshold",
            analogEventThreshold,
            ContainerTransformMetadata{
                    .description = "Detect threshold crossing events with lockout period",
                    .category = "Signal Processing",
                    .input_type_name = "AnalogTimeSeries",
                    .output_type_name = "DigitalEventSeries",
                    .params_type_name = "AnalogEventThresholdParams",
                    .is_expensive = false,
                    .is_deterministic = true,
                    .supports_cancellation = true});
}

// Register AnalogIntervalPeak (Binary Container Transform)
void registerAnalogIntervalPeak() {
    auto & registry = ElementRegistry::instance();
    registry.registerBinaryContainerTransform<
            DigitalIntervalSeries,
            AnalogTimeSeries,
            DigitalEventSeries,
            AnalogIntervalPeakParams>(
            "AnalogIntervalPeak",
            analogIntervalPeak,
            ContainerTransformMetadata{
                    .description = "Find peak (min/max) analog values within intervals",
                    .category = "Signal Processing / Time Series",
                    .is_expensive = false,
                    .is_deterministic = true,
                    .supports_cancellation = true});
}

// Register AnalogIntervalThreshold (Container Transform)
void registerAnalogIntervalThreshold() {
    auto & registry = ElementRegistry::instance();
    registry.registerContainerTransform<AnalogTimeSeries, DigitalIntervalSeries, AnalogIntervalThresholdParams>(
            "AnalogIntervalThreshold",
            analogIntervalThreshold,
            ContainerTransformMetadata{
                    .description = "Detect intervals where signal meets threshold criteria",
                    .category = "Signal Processing",
                    .input_type_name = "AnalogTimeSeries",
                    .output_type_name = "DigitalIntervalSeries",
                    .params_type_name = "AnalogIntervalThresholdParams",
                    .is_expensive = false,
                    .is_deterministic = true,
                    .supports_cancellation = true});
}

// Register DigitalIntervalBoolean (Binary Container Transform) - compile-time registration
auto const register_digital_interval_boolean = RegisterBinaryContainerTransform<
        DigitalIntervalSeries,
        DigitalIntervalSeries,
        DigitalIntervalSeries,
        DigitalIntervalBooleanParams>(
        "DigitalIntervalBoolean",
        digitalIntervalBoolean,
        ContainerTransformMetadata{
                .description = "Apply boolean logic between two interval series",
                .category = "Signal Processing / Logic",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = true});

auto const container_transform_registration = []() {
    registerAnalogEventThreshold();
    registerAnalogIntervalPeak();
    registerAnalogIntervalThreshold();
    // DigitalIntervalBoolean is now registered via compile-time RAII above
    return true;
}();

}// anonymous namespace

}// namespace WhiskerToolbox::Transforms::V2::Examples
