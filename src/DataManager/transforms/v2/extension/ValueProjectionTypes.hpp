#ifndef WHISKERTOOLBOX_V2_VALUE_PROJECTION_TYPES_HPP
#define WHISKERTOOLBOX_V2_VALUE_PROJECTION_TYPES_HPP

/**
 * @file ValueProjectionTypes.hpp
 * @brief Types for value projections in trial-aligned analysis
 *
 * This file provides type definitions for value projections - transforms that
 * compute scalar values from source elements without creating intermediate
 * element types.
 *
 * ## Value Projection vs Element Transform
 *
 * Traditional element transforms produce new element types:
 * ```
 * EventWithId → NormalizedEvent  // New type required
 * ```
 *
 * Value projections produce scalars while preserving source identity:
 * ```
 * EventWithId → float            // Just the normalized time
 *     └── .id() accessible from source element
 * ```
 *
 * ## Motivation
 *
 * For trial-aligned analysis (raster plots, PSTH), we often need to:
 * 1. Normalize event times relative to trial alignment
 * 2. Draw or reduce the normalized values
 * 3. Preserve access to EntityId for coloring/grouping
 *
 * Creating intermediate types like `NormalizedEvent` causes:
 * - Type explosion in `ElementVariant`
 * - Unnecessary data duplication (EntityId copied to output)
 * - Complex type management in pipelines
 *
 * Value projections solve this by:
 * - Computing only the derived value (e.g., normalized time)
 * - Leaving identity info in the source element
 * - Enabling zero-copy lazy iteration
 *
 * ## Usage Example
 *
 * ```cpp
 * // Build pipeline that normalizes event time to float
 * TransformPipeline pipeline;
 * pipeline.addStep("NormalizeEventTimeValue", NormalizeTimeParams{});
 *
 * // Bind to value projection factory
 * auto factory = bindValueProjectionWithContext<EventWithId, float>(pipeline);
 *
 * // For each trial
 * for (size_t i = 0; i < gather_result.size(); ++i) {
 *     auto ctx = gather_result.buildContext(i);
 *     auto projection = factory(ctx);
 *
 *     for (auto const& event : gather_result[i]->view()) {
 *         float norm_time = projection(event);  // Computed value
 *         EntityId id = event.id();             // From source
 *         draw(norm_time, i, id);
 *     }
 * }
 * ```
 *
 * ## Relationship to Other Types
 *
 * - `ViewAdaptorFn`: `span<In> → vector<Out>` (batch, materializing)
 * - `ValueProjectionFn`: `In → Value` (single element, computed)
 * - `ReducerFn`: `span<In> → Scalar` (batch, terminal)
 *
 * Value projections are element-level and can be used inside a reduction:
 * ```
 * span<EventWithId> → [project each] → span<float> → [reduce] → Scalar
 * ```
 *
 * @see ViewAdaptorTypes.hpp for batch adaptor types
 * @see ContextAwareParams.hpp for context injection
 * @see GatherResult.hpp for trial-aligned analysis
 */

#include "transforms/v2/extension/ContextAwareParams.hpp"

#include <concepts>
#include <functional>
#include <ranges>
#include <utility>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Value Projection Function Types
// ============================================================================

/**
 * @brief Value projection function type
 *
 * Takes a single source element and returns a computed value.
 * The source element retains identity information (EntityId, etc.)
 * that can be accessed separately.
 *
 * @tparam InElement Input element type (e.g., EventWithId)
 * @tparam Value Output value type (e.g., float for normalized time)
 *
 * ## Example
 *
 * ```cpp
 * ValueProjectionFn<EventWithId, float> normalize = [alignment](EventWithId const& e) {
 *     return static_cast<float>(e.time().getValue() - alignment.getValue());
 * };
 *
 * for (auto const& event : view) {
 *     float norm_time = normalize(event);
 *     EntityId id = event.id();  // Still accessible from source
 * }
 * ```
 */
template<typename InElement, typename Value>
using ValueProjectionFn = std::function<Value(InElement const&)>;

/**
 * @brief Factory that creates value projections from TrialContext
 *
 * For context-aware transforms (e.g., NormalizeTime), the factory receives
 * per-trial context and produces a projection function with that context
 * injected.
 *
 * @tparam InElement Input element type
 * @tparam Value Output value type
 *
 * ## Example
 *
 * ```cpp
 * ValueProjectionFactory<EventWithId, float> factory = ...;
 *
 * for (size_t i = 0; i < trials.size(); ++i) {
 *     TrialContext ctx{.alignment_time = trials[i].start()};
 *     auto projection = factory(ctx);
 *
 *     for (auto const& event : trial_views[i]) {
 *         float norm_time = projection(event);
 *     }
 * }
 * ```
 */
template<typename InElement, typename Value>
using ValueProjectionFactory = std::function<ValueProjectionFn<InElement, Value>(TrialContext const&)>;

// ============================================================================
// Type-Erased Versions (for runtime composition)
// ============================================================================

/**
 * @brief Type-erased value projection function
 *
 * Used internally by the pipeline for runtime-typed execution.
 * Input and output are wrapped in std::any.
 */
using ErasedValueProjectionFn = std::function<std::any(std::any const&)>;

/**
 * @brief Type-erased value projection factory
 */
using ErasedValueProjectionFactory = std::function<ErasedValueProjectionFn(TrialContext const&)>;

// ============================================================================
// Concepts for Value Projections
// ============================================================================

/**
 * @brief Concept for a valid value projection function
 *
 * A value projection takes a single element and returns a value.
 */
template<typename F, typename InElement, typename Value>
concept ValueProjection = requires(F f, InElement const& input) {
    { f(input) } -> std::convertible_to<Value>;
};

/**
 * @brief Concept for a valid value projection factory
 *
 * A factory takes TrialContext and returns a value projection.
 */
template<typename F, typename InElement, typename Value>
concept ValueProjectionFactoryConcept = requires(F f, TrialContext const& ctx, InElement const& elem) {
    { f(ctx) } -> ValueProjection<InElement, Value>;
};

// ============================================================================
// Projected View Types
// ============================================================================

/**
 * @brief A lazy view that yields (source_element_ref, projected_value) pairs
 *
 * This enables zero-allocation iteration where both the original element
 * (for identity) and the projected value (for analysis) are available.
 *
 * @tparam InElement Source element type
 * @tparam Value Projected value type
 *
 * ## Note on Types
 *
 * When using `std::make_pair(std::cref(elem), value)`, the pair type becomes
 * `std::pair<Element const&, Value>`, NOT `std::pair<reference_wrapper, Value>`.
 * This means `pair.first` is a direct reference, not a wrapper.
 *
 * ## Example
 *
 * ```cpp
 * auto projected = makeProjectedView(trial_view, projection_fn);
 *
 * for (auto const& pair : projected) {
 *     EntityId id = pair.first.id();  // pair.first is Element const&
 *     float norm_time = pair.second;
 *     draw(norm_time, trial_row, id);
 * }
 * ```
 */
template<typename InElement, typename Value>
using ProjectedPair = std::pair<InElement const&, Value>;

/**
 * @brief Create a lazy projected view from a range and projection function
 *
 * Returns a range adaptor that yields (element_ref, projected_value) pairs.
 * No intermediate storage is allocated - values are computed on iteration.
 *
 * ## Type Note
 *
 * Uses `std::make_pair(std::cref(elem), value)` which creates `pair<T const&, Value>`.
 * The first element is a direct reference, not a reference_wrapper.
 *
 * @tparam Range Input range type
 * @tparam Proj Projection function type
 * @param range Source range of elements
 * @param projection Function to compute projected value
 * @return Lazy range of (element_ref, value) pairs
 *
 * ## Example
 *
 * ```cpp
 * auto view = trial->view();
 * auto projection = [](EventWithId const& e) { return e.time().getValue() * 0.001f; };
 *
 * for (auto const& pair : makeProjectedView(view, projection)) {
 *     // pair.first is EventWithId const&
 *     // pair.second is float (computed on-demand)
 *     EntityId id = pair.first.id();
 *     float scaled = pair.second;
 * }
 * ```
 */
template<std::ranges::input_range Range, typename Proj>
    requires std::invocable<Proj, std::ranges::range_reference_t<Range>>
auto makeProjectedView(Range&& range, Proj&& projection) {
    using Element = std::ranges::range_value_t<Range>;
    using Value = std::invoke_result_t<Proj, Element const&>;

    return std::forward<Range>(range)
        | std::views::transform([proj = std::forward<Proj>(projection)](auto const& elem) {
            return std::make_pair(std::cref(elem), proj(elem));
        });
}

/**
 * @brief Create a lazy view that yields only projected values
 *
 * When only the computed value is needed (not the source element),
 * this provides a simpler interface.
 *
 * @tparam Range Input range type
 * @tparam Proj Projection function type
 * @param range Source range of elements
 * @param projection Function to compute projected value
 * @return Lazy range of projected values
 *
 * ## Example
 *
 * ```cpp
 * auto values = makeValueView(trial->view(), normalize_fn);
 * float sum = std::accumulate(values.begin(), values.end(), 0.0f);
 * ```
 */
template<std::ranges::input_range Range, typename Proj>
    requires std::invocable<Proj, std::ranges::range_reference_t<Range>>
auto makeValueView(Range&& range, Proj&& projection) {
    return std::forward<Range>(range)
        | std::views::transform(std::forward<Proj>(projection));
}

// ============================================================================
// Helper: Wrap typed projection as erased
// ============================================================================

/**
 * @brief Wrap a typed value projection as a type-erased function
 *
 * Used by the pipeline to store projections in a type-erased manner.
 *
 * @tparam InElement Input element type
 * @tparam Value Output value type
 * @param typed_fn The typed projection function
 * @return Type-erased projection function
 */
template<typename InElement, typename Value>
ErasedValueProjectionFn eraseValueProjection(ValueProjectionFn<InElement, Value> typed_fn) {
    return [fn = std::move(typed_fn)](std::any const& input) -> std::any {
        auto const& elem = std::any_cast<InElement const&>(input);
        return std::any{fn(elem)};
    };
}

/**
 * @brief Wrap a typed value projection factory as a type-erased factory
 *
 * @tparam InElement Input element type
 * @tparam Value Output value type
 * @param typed_factory The typed projection factory
 * @return Type-erased projection factory
 */
template<typename InElement, typename Value>
ErasedValueProjectionFactory eraseValueProjectionFactory(
        ValueProjectionFactory<InElement, Value> typed_factory) {
    return [factory = std::move(typed_factory)](TrialContext const& ctx) -> ErasedValueProjectionFn {
        auto typed_fn = factory(ctx);
        return eraseValueProjection<InElement, Value>(std::move(typed_fn));
    };
}

// ============================================================================
// Helper: Recover typed projection from erased
// ============================================================================

/**
 * @brief Recover a typed value projection from a type-erased function
 *
 * Used when consuming a projection with known types.
 *
 * @tparam InElement Expected input element type
 * @tparam Value Expected output value type
 * @param erased_fn The type-erased projection function
 * @return Typed projection function
 */
template<typename InElement, typename Value>
ValueProjectionFn<InElement, Value> recoverValueProjection(ErasedValueProjectionFn erased_fn) {
    return [fn = std::move(erased_fn)](InElement const& elem) -> Value {
        // Pass element by value in std::any (matches eraseValueProjection convention)
        std::any result = fn(std::any{elem});
        return std::any_cast<Value>(result);
    };
}

/**
 * @brief Recover a typed value projection factory from a type-erased factory
 *
 * @tparam InElement Expected input element type
 * @tparam Value Expected output value type
 * @param erased_factory The type-erased projection factory
 * @return Typed projection factory
 */
template<typename InElement, typename Value>
ValueProjectionFactory<InElement, Value> recoverValueProjectionFactory(
        ErasedValueProjectionFactory erased_factory) {
    return [factory = std::move(erased_factory)](TrialContext const& ctx) 
            -> ValueProjectionFn<InElement, Value> {
        auto erased_fn = factory(ctx);
        return recoverValueProjection<InElement, Value>(std::move(erased_fn));
    };
}

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_VALUE_PROJECTION_TYPES_HPP
