#ifndef NEURALYZER_V2_VIEW_ADAPTOR_TYPES_HPP
#define NEURALYZER_V2_VIEW_ADAPTOR_TYPES_HPP

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
 * `bindValueProjectionV2()` + `GatherResult::project()` with
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

#include "TransformTypes/TransformTypes.hpp"

#include <any>
#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <typeindex>
#include <vector>

namespace Neuralyzer::Transforms::V2 {
class PipelineValueStore;
}// namespace Neuralyzer::Transforms::V2

namespace Neuralyzer::Gather {

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
 * @brief Factory that creates a reducer from PipelineValueStore
 *
 * @tparam InElement Input element type
 * @tparam Scalar Output scalar type
 *
 * @see GatherResult::reduce() for usage
 * @see PipelineValueStore for store documentation
 */
template<typename InElement, typename Scalar>
using ReducerFactoryV2 = std::function<ReducerFn<InElement, Scalar>(Neuralyzer::Transforms::V2::PipelineValueStore const &)>;

/**
 * @brief Type-erased reducer factory (V2 pattern)
 */
using ErasedReducerFactoryV2 = std::function<ErasedReducerFn(Neuralyzer::Transforms::V2::PipelineValueStore const &)>;

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

}// namespace Neuralyzer::Gather

#endif// NEURALYZER_V2_VIEW_ADAPTOR_TYPES_HPP
