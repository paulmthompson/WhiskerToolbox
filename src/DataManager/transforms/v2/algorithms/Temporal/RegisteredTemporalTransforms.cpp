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
// Value Projection Registrations (Return float directly)
// ============================================================================

/**
 * @brief Register NormalizeTimeValue transform (value projection)
 *
 * Transforms TimeFrameIndex → float by computing normalized time.
 * This is the fundamental temporal normalization transform.
 *
 * Use this for:
 * - Raster plot drawing: extract .time() from EventWithId, normalize, draw
 * - Range reductions (FirstPositiveLatency, etc.)
 * - Any case where you need a time offset as a float
 *
 * The caller extracts the TimeFrameIndex from their element type:
 * - EventWithId: event.time()
 * - TimeValuePoint: sample.time()
 * - Custom types: element.time()
 */
auto const register_normalize_time_value = RegisterTransform<
        TimeFrameIndex,
        float,
        NormalizeTimeParams>(
        "NormalizeTimeValue",
        normalizeTimeValue,
        TransformMetadata{
                .name = "NormalizeTimeValue",
                .description = "Compute normalized time offset as float (value projection)",
                .category = "Temporal",
                .input_type = typeid(TimeFrameIndex),
                .output_type = typeid(float),
                .params_type = typeid(NormalizeTimeParams),
                .lineage_type = TransformLineageType::None,  // No entity tracking for scalar output
                .input_type_name = "TimeFrameIndex",
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
    (void)register_normalize_time_value;
    (void)register_normalize_sample_time_value;
    (void)init_pipeline_factories;
    (void)register_context_injector;
}

}  // namespace WhiskerToolbox::Transforms::V2::Temporal
