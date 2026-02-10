#ifndef WHISKERTOOLBOX_V2_VIEW_ADAPTOR_TYPES_HPP
#define WHISKERTOOLBOX_V2_VIEW_ADAPTOR_TYPES_HPP

/**
 * @file ViewAdaptorTypes.hpp
 * @brief Types for view adaptors and reducers produced by TransformPipeline
 *
 * This file provides the type definitions and interfaces for view adaptors
 * and reducers that enable lazy, composable transformations on GatherResult
 * views without intermediate materialization.
 *
 * ## View Adaptor
 *
 * A view adaptor transforms a range of input elements into a lazy range of
 * output elements. No intermediate storage is created - each element is
 * transformed on-demand as the output range is consumed.
 *
 * ```cpp
 * // ViewAdaptor: transforms range<In> → lazy range<Out>
 * auto adaptor = pipeline.bindToView<EventWithId, NormalizedEvent>();
 * auto lazy_view = adaptor(trial_events_view);
 * for (auto const& normalized : lazy_view) { ... }
 * ```
 *
 * ## Context-Aware View Adaptor Factory
 *
 * For transforms that need per-trial context (e.g., NormalizeTime), a factory
 * pattern is used. The factory accepts TrialContext and returns a view adaptor
 * with context injected into the parameters.
 *
 * ```cpp
 * // Factory: TrialContext → ViewAdaptor
 * auto factory = pipeline.bindToViewWithContext<EventWithId, NormalizedEvent>();
 * for (size_t i = 0; i < gather_result.size(); ++i) {
 *     TrialContext ctx{.alignment_time = interval.start};
 *     auto adaptor = factory(ctx);
 *     auto lazy_view = adaptor(gather_result[i]->view());
 * }
 * ```
 *
 * ## Reducer
 *
 * A reducer combines a view adaptor with a terminal range reduction to
 * produce a scalar from a range of input elements.
 *
 * ```cpp
 * // Reducer: range<In> → Scalar
 * auto reducer = pipeline.bindReducer<EventWithId, float>();
 * float latency = reducer(trial_events_view);
 * ```
 *
 * ## Context-Aware Reducer Factory
 *
 * Similar to view adaptor factories, reducer factories accept context.
 *
 * ```cpp
 * auto factory = pipeline.bindReducerWithContext<EventWithId, float>();
 * auto latencies = gather_result.transform([&](auto const& trial) {
 *     TrialContext ctx{...};
 *     return factory(ctx)(trial->view());
 * });
 * ```
 *
 * @see TransformPipeline for methods that create these types
 * @see RangeReductionRegistry.hpp for range reductions
 * @see GatherResult.hpp for trial-aligned analysis
 * @see PipelineValueStore for the V2 pattern replacing TrialContext
 */

#include "TimeFrame/StrongTimeTypes.hpp"
#include "transforms/v2/extension/TransformTypes.hpp"

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
