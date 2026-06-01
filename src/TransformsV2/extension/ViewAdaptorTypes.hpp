#ifndef WHISKERTOOLBOX_V2_VIEW_ADAPTOR_TYPES_HPP
#define WHISKERTOOLBOX_V2_VIEW_ADAPTOR_TYPES_HPP

/**
 * @file ViewAdaptorTypes.hpp
 * @brief Function-object types for gather-slice transforms and reductions
 *
 * Defines `ViewAdaptorFn`, `ReducerFn`, and factory aliases used with
 * `GatherResult` and interval/tensor builders. Callers supply these callables
 * directly (often wrapping `RangeReductionRegistry::executeErased()`).
 *
 * ## Production patterns
 *
 * **Per-trial scalar features (interval rows):**
 * `gatherAndExecutePipeline()` and `TensorColumnBuilders` run
 * `executePipeline()` on each gathered slice, then apply the pipeline's
 * terminal range reduction via `RangeReductionRegistry::executeErased()`.
 *
 * **Per-element projection (raster plots):**
 * `bindValueProjectionV2()` + `GatherResult::projectV2()` with
 * `PipelineValueStore` from `buildTrialStore()`.
 *
 * **Trial sorting / batch reduce:**
 * Build a `ReducerFactoryV2` lambda and pass it to `GatherResult::reduce()`.
 *
 * ```cpp
 * ReducerFactoryV2<EventWithId, float> factory =
 *     [&](PipelineValueStore const&) -> ReducerFn<EventWithId, float> {
 *         return [](std::span<EventWithId const> events) -> float {
 *             auto& reg = RangeReductionRegistry::instance();
 *             std::any input_any{events};
 *             return std::any_cast<float>(reg.executeErased(
 *                 "FirstPositiveLatency", typeid(EventWithId), input_any, {}));
 *         };
 *     };
 * auto latencies = gather.reduce(factory);
 * ```
 *
 * @see TransformPipeline::bindValueProjectionV2()
 * @see GatherPipelineExecutor.hpp gatherAndExecutePipeline()
 * @see TensorColumnBuilders.hpp buildIntervalReductionProvider()
 * @see RangeReductionRegistry.hpp
 * @see GatherResult.hpp
 * @see PipelineValueStore
 */

#include "TransformTypes.hpp"

#include "TimeFrame/TimeFrameIndex.hpp"

#include <any>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <typeindex>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Legacy Context Types (for backward compatibility)
// ============================================================================

/**
 * @brief Context for trial-aligned analysis (legacy - use PipelineValueStore for V2)
 *
 * This struct is kept for backward compatibility with existing code that uses
 * the context injection pattern. For new code, prefer using PipelineValueStore
 * with parameter bindings.
 *
 * @see PipelineValueStore for the V2 pattern
 * @see GatherResult::buildTrialStore() for V2 trial value population
 */
struct TrialContext {
    /// The time to use as the reference point (t=0) for normalization
    TimeFrameIndex alignment_time{0};

    /// Optional: Index of the current trial (for debugging/logging)
    std::optional<std::size_t> trial_index{std::nullopt};

    /// Optional: Duration of the trial (end_time - start_time)
    std::optional<int64_t> trial_duration{std::nullopt};

    /// Optional: End time of the trial interval
    std::optional<TimeFrameIndex> end_time{std::nullopt};
};

}  // namespace WhiskerToolbox::Transforms::V2

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// View Adaptor Types
// ============================================================================

/**
 * @brief Type-erased view adaptor function
 *
 * Takes a span of input elements and produces a vector of output elements.
 * This is the materialized version - for lazy evaluation, use the templated
 * view adaptor that returns a range adaptor.
 *
 * @note This is primarily used when type erasure is needed (e.g., storing
 * in containers, passing through non-template interfaces).
 */
template<typename InElement, typename OutElement>
using ViewAdaptorFn = std::function<std::vector<OutElement>(std::span<InElement const>)>;

/**
 * @brief Type-erased view adaptor that works with std::any
 *
 * Used internally by the pipeline for runtime-typed execution.
 */
using ErasedViewAdaptorFn = std::function<std::any(std::any const &)>;

/**
 * @brief Factory that creates a view adaptor from TrialContext
 *
 * This is used when the pipeline contains context-aware transforms
 * (e.g., NormalizeTime). The factory receives context for each trial
 * and produces an adaptor with that context injected.
 */
template<typename InElement, typename OutElement>
using ViewAdaptorFactory = std::function<ViewAdaptorFn<InElement, OutElement>(TrialContext const &)>;

/**
 * @brief Type-erased view adaptor factory
 */
using ErasedViewAdaptorFactory = std::function<ErasedViewAdaptorFn(TrialContext const &)>;

// ============================================================================
// Reducer Types
// ============================================================================

/**
 * @brief Typed reducer function
 *
 * Consumes a span of input elements and produces a scalar.
 * This combines the view transformation and range reduction.
 */
template<typename InElement, typename Scalar>
using ReducerFn = std::function<Scalar(std::span<InElement const>)>;

/**
 * @brief Type-erased reducer that works with std::any
 */
using ErasedReducerFn = std::function<std::any(std::any const &)>;

/**
 * @brief Factory that creates a reducer from TrialContext (legacy)
 *
 * Used when the pipeline contains context-aware transforms.
 * For new code, prefer ReducerFactoryV2 with PipelineValueStore.
 */
template<typename InElement, typename Scalar>
using ReducerFactory = std::function<ReducerFn<InElement, Scalar>(TrialContext const &)>;

/**
 * @brief Type-erased reducer factory (legacy)
 */
using ErasedReducerFactory = std::function<ErasedReducerFn(TrialContext const &)>;

// Forward declaration for V2 types
class PipelineValueStore;

/**
 * @brief Factory that creates a reducer from PipelineValueStore (V2 pattern)
 *
 * This is the V2 replacement for ReducerFactory that uses the generic
 * PipelineValueStore instead of specialized TrialContext.
 *
 * @tparam InElement Input element type
 * @tparam Scalar Output scalar type
 *
 * @see GatherResult::reduceV2() for usage
 * @see PipelineValueStore for store documentation
 */
template<typename InElement, typename Scalar>
using ReducerFactoryV2 = std::function<ReducerFn<InElement, Scalar>(PipelineValueStore const &)>;

/**
 * @brief Type-erased reducer factory (V2 pattern)
 */
using ErasedReducerFactoryV2 = std::function<ErasedReducerFn(PipelineValueStore const &)>;

// ============================================================================
// Concepts for View Adaptors
// ============================================================================

/**
 * @brief Concept for a valid view adaptor
 *
 * A view adaptor is a callable that transforms a range of input elements
 * into a range or container of output elements.
 */
template<typename F, typename InElement, typename OutElement>
concept ViewAdaptor = requires(F f, std::span<InElement const> input) {
    { f(input) } -> std::ranges::input_range;
    // Output range must yield OutElement
    requires std::convertible_to<
        std::ranges::range_value_t<decltype(f(input))>,
        OutElement>;
};

/**
 * @brief Concept for a valid reducer
 *
 * A reducer consumes a range of input elements and produces a scalar.
 */
template<typename F, typename InElement, typename Scalar>
concept Reducer = requires(F f, std::span<InElement const> input) {
    { f(input) } -> std::convertible_to<Scalar>;
};

// ============================================================================
// Terminal Reduction Step Descriptor
// ============================================================================

/**
 * @brief Descriptor for a terminal range reduction in a pipeline
 *
 * This is stored in TransformPipeline when setRangeReduction() is called.
 * It contains the reduction name and type-erased parameters.
 */
struct RangeReductionStep {
    /// Name of the registered range reduction
    std::string reduction_name;

    /// Type-erased parameters for the reduction
    std::any params;

    /// Input element type (for validation)
    std::type_index input_type = typeid(void);

    /// Output scalar type
    std::type_index output_type = typeid(void);

    /// Parameter type
    std::type_index params_type = typeid(void);

    RangeReductionStep() = default;

    template<typename Params>
    RangeReductionStep(std::string name, Params p)
        : reduction_name(std::move(name)),
          params(std::move(p)),
          params_type(typeid(Params)) {}
};

// ============================================================================
// Result Types for Pipeline Binding
// ============================================================================

/**
 * @brief Result of binding a pipeline to produce a view adaptor
 *
 * Contains both the adaptor function and metadata about the transformation.
 */
template<typename InElement, typename OutElement>
struct BoundViewAdaptor {
    /// The view adaptor function
    ViewAdaptorFn<InElement, OutElement> adaptor;

    /// Whether the adaptor requires context (has context-aware params)
    bool requires_context{false};

    /// Input element type
    std::type_index input_type = typeid(InElement);

    /// Output element type
    std::type_index output_type = typeid(OutElement);
};

/**
 * @brief Result of binding a pipeline to produce a reducer
 */
template<typename InElement, typename Scalar>
struct BoundReducer {
    /// The reducer function
    ReducerFn<InElement, Scalar> reducer;

    /// Whether the reducer requires context
    bool requires_context{false};

    /// Input element type
    std::type_index input_type = typeid(InElement);

    /// Output scalar type
    std::type_index output_type = typeid(Scalar);

    /// Intermediate element type (output of transforms before reduction)
    std::type_index intermediate_type = typeid(void);
};

/**
 * @brief Result of binding a context-aware pipeline
 */
template<typename InElement, typename OutElement>
struct BoundContextAwareViewAdaptor {
    /// Factory that creates adaptors from context
    ViewAdaptorFactory<InElement, OutElement> factory;

    /// Input element type
    std::type_index input_type = typeid(InElement);

    /// Output element type
    std::type_index output_type = typeid(OutElement);
};

/**
 * @brief Result of binding a context-aware pipeline with reduction
 */
template<typename InElement, typename Scalar>
struct BoundContextAwareReducer {
    /// Factory that creates reducers from context
    ReducerFactory<InElement, Scalar> factory;

    /// Input element type
    std::type_index input_type = typeid(InElement);

    /// Output scalar type
    std::type_index output_type = typeid(Scalar);
};

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_VIEW_ADAPTOR_TYPES_HPP
