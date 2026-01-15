#ifndef WHISKERTOOLBOX_V2_CONTEXT_AWARE_PARAMS_HPP
#define WHISKERTOOLBOX_V2_CONTEXT_AWARE_PARAMS_HPP

/**
 * @file ContextAwareParams.hpp
 * @brief Context injection infrastructure for transform parameters
 *
 * This file defines the concepts and types for context-aware transform parameters.
 * Context injection enables transforms to receive per-trial (or per-segment) information
 * such as alignment time, trial metadata, or computed statistics.
 *
 * ## Primary Use Case: Time Normalization
 *
 * When processing trial-aligned data (e.g., raster plots), each trial's events need
 * to be normalized relative to an alignment point (e.g., trial start, stimulus onset).
 * The context carries this alignment time:
 *
 * ```cpp
 * // Context with alignment time from trial interval
 * TrialContext ctx{.alignment_time = trial_start_time};
 *
 * // Parameters receive context before transform execution
 * NormalizeTimeParams params;
 * params.setContext(ctx);
 *
 * // Transform uses cached alignment for each element
 * auto normalized = normalizeEventTime(event, params);
 * ```
 *
 * ## Context Injection Flow
 *
 * ```
 * GatherResult (trials)
 *     │
 *     ├── Trial 0: interval [100, 200]
 *     │       └── TrialContext{alignment_time=100}
 *     │              └── params.setContext(ctx)
 *     │                     └── transform(element, params)
 *     │
 *     ├── Trial 1: interval [300, 450]
 *     │       └── TrialContext{alignment_time=300}
 *     │              └── params.setContext(ctx)
 *     │                     └── transform(element, params)
 *     │
 *     └── ...
 * ```
 *
 * ## Design Principles
 *
 * 1. **Opt-in Detection**: Uses C++20 concepts to detect if parameters support context
 * 2. **Minimal Interface**: Context types are simple structs with public fields
 * 3. **Copy Semantics**: Context is copied to parameters (small, immutable)
 * 4. **Composable**: Same context can be used by multiple transforms in a pipeline
 *
 * @see TransformPipeline for context injection during execution
 * @see NormalizeTime.hpp for the primary context-aware transform
 * @see GatherResult for trial-aligned analysis
 */

#include "TimeFrame/TimeFrame.hpp"

#include <concepts>
#include <optional>
#include <type_traits>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Context Types
// ============================================================================

/**
 * @brief Context for trial-aligned analysis
 *
 * Carries per-trial information that transforms can use.
 * The primary use case is temporal normalization where each trial's
 * events are shifted relative to an alignment point.
 *
 * ## Fields
 *
 * - `alignment_time`: The time point to use as t=0 (e.g., trial start, stimulus onset)
 * - `trial_index`: Optional index of the current trial (for debugging/logging)
 * - `trial_duration`: Optional duration of the trial interval
 *
 * ## Usage
 *
 * ```cpp
 * // From interval [start, end]
 * TrialContext ctx;
 * ctx.alignment_time = interval.start_time;
 * ctx.trial_index = trial_idx;
 * ctx.trial_duration = interval.end_time - interval.start_time;
 *
 * // Pass to context-aware parameters
 * if constexpr (ContextAwareParams<Params, TrialContext>) {
 *     params.setContext(ctx);
 * }
 * ```
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

// ============================================================================
// Context-Aware Parameter Concepts
// ============================================================================

/**
 * @brief Concept for parameters that can receive trial context
 *
 * Parameters satisfying this concept have a `setContext` method that accepts
 * the context type. The pipeline will automatically call this before executing
 * the transform for each trial.
 *
 * @tparam Params The parameter type to check
 * @tparam Context The context type (defaults to TrialContext)
 *
 * ## Implementation Requirements
 *
 * ```cpp
 * struct MyParams {
 *     // Required: setContext method
 *     void setContext(TrialContext const& ctx) {
 *         cached_alignment_ = ctx.alignment_time;
 *     }
 *
 *     // Optional: Check if context has been set
 *     [[nodiscard]] bool hasContext() const noexcept {
 *         return cached_alignment_.has_value();
 *     }
 *
 * private:
 *     std::optional<TimeFrameIndex> cached_alignment_;
 * };
 * ```
 */
template<typename Params, typename Context>
concept ContextAwareParams = requires(Params& p, Context const& ctx) {
    { p.setContext(ctx) };
};

/**
 * @brief Concept for parameters that specifically accept TrialContext
 *
 * Convenience alias for the most common case.
 */
template<typename Params>
concept TrialContextAwareParams = ContextAwareParams<Params, TrialContext>;

// ============================================================================
// Type Traits for Context Detection
// ============================================================================

/**
 * @brief Type trait to check if parameters are context-aware
 */
template<typename Params, typename Context, typename = void>
struct is_context_aware_params : std::false_type {};

template<typename Params, typename Context>
struct is_context_aware_params<Params, Context,
    std::void_t<decltype(std::declval<Params&>().setContext(std::declval<Context const&>()))>>
    : std::true_type {};

template<typename Params, typename Context>
inline constexpr bool is_context_aware_params_v = is_context_aware_params<Params, Context>::value;

/**
 * @brief Convenience trait for TrialContext
 */
template<typename Params>
inline constexpr bool is_trial_context_aware_v = is_context_aware_params_v<Params, TrialContext>;

// ============================================================================
// Context Injection Helpers
// ============================================================================

/**
 * @brief Inject context into parameters if supported
 *
 * This is a no-op for parameters that don't support context injection.
 * For context-aware parameters, it calls setContext.
 *
 * @tparam Params Parameter type
 * @tparam Context Context type
 * @param params Parameters to potentially inject context into
 * @param ctx Context to inject
 *
 * Usage:
 * ```cpp
 * NormalizeTimeParams params;
 * TrialContext ctx{.alignment_time = TimeFrameIndex{100}};
 * injectContext(params, ctx);  // params.setContext(ctx) called
 *
 * SomeOtherParams other;
 * injectContext(other, ctx);   // No-op, other doesn't support context
 * ```
 */
template<typename Params, typename Context>
constexpr void injectContext(Params& params, Context const& ctx) {
    if constexpr (ContextAwareParams<Params, Context>) {
        params.setContext(ctx);
    }
    // Else: No-op for non-context-aware parameters
}

/**
 * @brief Check if parameters have received context
 *
 * Returns true if:
 * - Parameters don't support context (always "ready")
 * - Parameters support context AND have a hasContext() method returning true
 * - Parameters support context but have no hasContext() method (assume ready after setContext)
 *
 * @tparam Params Parameter type
 * @param params Parameters to check
 * @return true if parameters are ready for use
 */
template<typename Params>
[[nodiscard]] constexpr bool hasRequiredContext(Params const& params) {
    if constexpr (!is_trial_context_aware_v<Params>) {
        // Non-context-aware params are always ready
        return true;
    } else if constexpr (requires { { params.hasContext() } -> std::convertible_to<bool>; }) {
        // Has explicit context check
        return params.hasContext();
    } else {
        // Context-aware but no hasContext() method - assume ready
        return true;
    }
}

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_CONTEXT_AWARE_PARAMS_HPP
