#include "transforms/v2/algorithms/Temporal/RegisteredTemporalTransforms.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "transforms/v2/algorithms/Temporal/NormalizeTime.hpp"
#include "transforms/v2/core/ContextAwareParams.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"

namespace WhiskerToolbox::Transforms::V2::Temporal {

// Use the registration helper from Examples namespace
using WhiskerToolbox::Transforms::V2::Examples::registerPipelineStepFactoryFor;

// ============================================================================
// Static Initialization
// ============================================================================

namespace {

/**
 * @brief Register context injector for NormalizeTimeParams
 *
 * This enables automatic context injection in TransformPipeline binding.
 */
static RegisterContextInjector<NormalizeTimeParams> const register_context_injector;

/**
 * @brief Register PipelineStep factories for temporal parameter types
 *
 * This enables JSON deserialization of pipelines containing temporal transforms.
 */
bool const init_pipeline_factories = []() {
    registerPipelineStepFactoryFor<NormalizeTimeParams>();
    return true;
}();

/**
 * @brief Registration helper for element transforms
 *
 * Uses static initialization to register transforms at program startup.
 */
template<typename In, typename Out, typename Params>
struct RegisterTransform {
    RegisterTransform(
            std::string name,
            Out (*func)(In const&, Params const&),
            TransformMetadata metadata) {
        metadata.name = name;
        metadata.input_type = typeid(In);
        metadata.output_type = typeid(Out);
        metadata.params_type = typeid(Params);

        ElementRegistry::instance().registerTransform<In, Out, Params>(
                name,
                func,
                std::move(metadata));
    }
};

// ============================================================================
// Transform Registrations
// ============================================================================

/**
 * @brief Register NormalizeEventTime transform
 *
 * Transforms EventWithId → NormalizedEvent by shifting time relative to alignment.
 * Uses context-aware NormalizeTimeParams that receives alignment from TrialContext.
 */
auto const register_normalize_event_time = RegisterTransform<
        EventWithId,
        NormalizedEvent,
        NormalizeTimeParams>(
        "NormalizeEventTime",
        normalizeEventTime,
        TransformMetadata{
                .name = "NormalizeEventTime",
                .description = "Normalize event times relative to alignment point (t=0)",
                .category = "Temporal",
                .input_type = typeid(EventWithId),
                .output_type = typeid(NormalizedEvent),
                .params_type = typeid(NormalizeTimeParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "EventWithId",
                .output_type_name = "NormalizedEvent",
                .params_type_name = "NormalizeTimeParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

/**
 * @brief Register NormalizeValueTime transform
 *
 * Transforms TimeValuePoint → NormalizedValue by shifting time relative to alignment.
 * The sample value is preserved unchanged.
 */
auto const register_normalize_value_time = RegisterTransform<
        AnalogTimeSeries::TimeValuePoint,
        NormalizedValue,
        NormalizeTimeParams>(
        "NormalizeValueTime",
        normalizeValueTime,
        TransformMetadata{
                .name = "NormalizeValueTime",
                .description = "Normalize analog sample times relative to alignment point (t=0)",
                .category = "Temporal",
                .input_type = typeid(AnalogTimeSeries::TimeValuePoint),
                .output_type = typeid(NormalizedValue),
                .params_type = typeid(NormalizeTimeParams),
                .lineage_type = TransformLineageType::OneToOneByTime,
                .input_type_name = "TimeValuePoint",
                .output_type_name = "NormalizedValue",
                .params_type_name = "NormalizeTimeParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

// ============================================================================
// Value Projection Registrations (Return float directly)
// ============================================================================

/**
 * @brief Register NormalizeEventTimeValue transform (value projection)
 *
 * Transforms EventWithId → float by computing normalized time.
 * This is the value projection version - returns only the normalized time,
 * allowing EntityId to be accessed from the source element.
 *
 * Use this for:
 * - Raster plot drawing (need time for position, EntityId for color)
 * - Range reductions (FirstPositiveLatency, etc.)
 * - Lazy iteration without intermediate type allocation
 */
auto const register_normalize_event_time_value = RegisterTransform<
        EventWithId,
        float,
        NormalizeTimeParams>(
        "NormalizeEventTimeValue",
        normalizeEventTimeValue,
        TransformMetadata{
                .name = "NormalizeEventTimeValue",
                .description = "Compute normalized event time as float (value projection)",
                .category = "Temporal",
                .input_type = typeid(EventWithId),
                .output_type = typeid(float),
                .params_type = typeid(NormalizeTimeParams),
                .lineage_type = TransformLineageType::None,  // No entity tracking for scalar output
                .input_type_name = "EventWithId",
                .output_type_name = "float",
                .params_type_name = "NormalizeTimeParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

/**
 * @brief Register NormalizeSampleTimeValue transform (value projection)
 *
 * Transforms TimeValuePoint → float by computing normalized time.
 * This is the value projection version for analog samples.
 */
auto const register_normalize_sample_time_value = RegisterTransform<
        AnalogTimeSeries::TimeValuePoint,
        float,
        NormalizeTimeParams>(
        "NormalizeSampleTimeValue",
        normalizeSampleTimeValue,
        TransformMetadata{
                .name = "NormalizeSampleTimeValue",
                .description = "Compute normalized sample time as float (value projection)",
                .category = "Temporal",
                .input_type = typeid(AnalogTimeSeries::TimeValuePoint),
                .output_type = typeid(float),
                .params_type = typeid(NormalizeTimeParams),
                .lineage_type = TransformLineageType::None,
                .input_type_name = "TimeValuePoint",
                .output_type_name = "float",
                .params_type_name = "NormalizeTimeParams",
                .is_expensive = false,
                .is_deterministic = true,
                .supports_cancellation = false});

}  // anonymous namespace

// ============================================================================
// Public Registration Function
// ============================================================================

void registerTemporalTransforms() {
    // Transforms are already registered via static initialization above.
    // This function exists for:
    // 1. Explicit registration timing control in tests
    // 2. Documentation of what gets registered
    // 3. Forcing the translation unit to be linked

    // Force instantiation of static registrations
    (void)register_normalize_event_time;
    (void)register_normalize_value_time;
    (void)register_normalize_event_time_value;
    (void)register_normalize_sample_time_value;
    (void)init_pipeline_factories;
    (void)register_context_injector;
}

}  // namespace WhiskerToolbox::Transforms::V2::Temporal
