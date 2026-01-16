#ifndef WHISKERTOOLBOX_V2_RANGE_REDUCTION_TYPES_HPP
#define WHISKERTOOLBOX_V2_RANGE_REDUCTION_TYPES_HPP

/**
 * @file RangeReductionTypes.hpp
 * @brief Types and concepts for range reduction operations
 *
 * This file defines the concepts and metadata types for range reductions.
 * Range reductions consume an entire range of elements and produce a scalar.
 *
 * ## Distinction from TimeGroupedTransform
 *
 * Range reductions are fundamentally different from TimeGroupedTransform:
 *
 * | Aspect              | TimeGroupedTransform            | RangeReduction                    |
 * |---------------------|----------------------------------|-----------------------------------|
 * | **Scope**           | Elements at ONE time point       | Elements across ALL time points   |
 * | **Temporal**        | Preserved                        | Collapsed                         |
 * | **Signature**       | `span<In>` → `vector<Out>`       | `range<Element>` → `Scalar`       |
 * | **Use Case**        | RaggedAnalogTimeSeries → AnalogTimeSeries | Trial view → sort key    |
 * | **Example**         | Sum 3 mask areas at t=100 → [6]  | Count all spikes in trial → 50   |
 *
 * ## Usage with GatherResult
 *
 * Range reductions are designed to work with GatherResult for trial-based analysis:
 *
 * ```cpp
 * auto spike_gather = gather(spikes, trials);
 *
 * // Build pipeline with range reduction
 * auto sort_pipeline = TransformPipeline()
 *     .addStep("NormalizeEventTime", NormalizeTimeParams{})
 *     .addRangeReduction("FirstPositiveLatency", FirstPositiveLatencyParams{});
 *
 * // Reduce each trial to a scalar for sorting
 * auto latencies = spike_gather.reducePipeline<EventWithId, float>(sort_pipeline);
 * ```
 *
 * @see TimeSeriesConcepts.hpp for element concepts (TimeSeriesElement, etc.)
 * @see ElementTransform.hpp for TimeGroupedTransform
 * @see GatherResult.hpp for trial-aligned analysis
 */

#include "utils/TimeSeriesConcepts.hpp"

#include <concepts>
#include <ranges>
#include <string>
#include <typeindex>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Range Reduction Concepts
// ============================================================================

/**
 * @brief Concept for parameterized range reduction operations
 *
 * A range reduction consumes an entire range of elements and produces a scalar.
 * This is the primary concept for most range reductions.
 *
 * @tparam F The reduction function type
 * @tparam Element The element type consumed from the range
 * @tparam Scalar The scalar type produced
 * @tparam Params The parameter type
 *
 * Use cases:
 * - Count events in a trial: range<EventWithId> → int
 * - First positive latency: range<EventWithId> → float
 * - Max value in trial: range<TimeValuePoint> → float
 */
template<typename F, typename Element, typename Scalar, typename Params>
concept ParameterizedRangeReduction = requires(F f, Params const & params) {
    // Must be callable with any input_range of Element
    requires requires(std::vector<Element> const & vec) {
        { f(vec, params) } -> std::convertible_to<Scalar>;
    };
};

/**
 * @brief Concept for stateless range reduction (no parameters)
 *
 * Some reductions don't need parameters (e.g., simple count).
 *
 * @tparam F The reduction function type
 * @tparam Element The element type consumed from the range
 * @tparam Scalar The scalar type produced
 */
template<typename F, typename Element, typename Scalar>
concept StatelessRangeReduction = requires(F f) {
    requires requires(std::vector<Element> const & vec) {
        { f(vec) } -> std::convertible_to<Scalar>;
    };
};

/**
 * @brief Concept for any valid range reduction
 */
template<typename F, typename Element, typename Scalar, typename Params = void>
concept RangeReduction =
        ParameterizedRangeReduction<F, Element, Scalar, Params> ||
        (std::is_void_v<Params> && StatelessRangeReduction<F, Element, Scalar>);

// ============================================================================
// Metadata for Range Reductions
// ============================================================================

/**
 * @brief Metadata about a registered range reduction operation
 *
 * Parallels TransformMetadata but specialized for range reductions.
 * Used for UI generation, discovery, and JSON serialization.
 */
struct RangeReductionMetadata {
    /// Unique name for the reduction (e.g., "FirstPositiveLatency")
    std::string name;

    /// Human-readable description
    std::string description;

    /// Category for UI grouping (e.g., "Event Statistics", "Value Statistics")
    std::string category;

    /// Type of element consumed from the input range
    std::type_index input_type = typeid(void);

    /// Type of scalar produced
    std::type_index output_type = typeid(void);

    /// Type of parameters (typeid(void) for stateless)
    std::type_index params_type = typeid(void);

    /// String names for serialization/UI display
    std::string input_type_name;
    std::string output_type_name;
    std::string params_type_name;

    /// Version for compatibility tracking
    std::string version = "1.0";

    /// Author information
    std::string author;

    /// Performance hints
    bool is_expensive = false;///< True if reduction is computationally intensive
    bool is_deterministic = true;///< True if same input always produces same output

    /// Input element concept requirements (for documentation/validation)
    bool requires_time_series_element = true;///< Input must satisfy TimeSeriesElement
    bool requires_entity_element = false;///< Input must satisfy EntityElement
    bool requires_value_element = false;///< Input must satisfy ValueElement
};

// ============================================================================
// Empty Parameter Type for Stateless Reductions
// ============================================================================

/**
 * @brief Empty parameter struct for reductions that don't need configuration
 *
 * This mirrors NoParams in ElementRegistry for consistency.
 */
struct NoReductionParams {};

// ============================================================================
// Helper Type Traits
// ============================================================================

/**
 * @brief Check if a type is a valid range reduction function
 */
template<typename F, typename Element, typename Scalar, typename Params>
struct is_range_reduction : std::bool_constant<RangeReduction<F, Element, Scalar, Params>> {};

template<typename F, typename Element, typename Scalar, typename Params>
inline constexpr bool is_range_reduction_v = is_range_reduction<F, Element, Scalar, Params>::value;

/**
 * @brief Check if a type is a stateless range reduction
 */
template<typename F, typename Element, typename Scalar>
struct is_stateless_range_reduction : std::bool_constant<StatelessRangeReduction<F, Element, Scalar>> {};

template<typename F, typename Element, typename Scalar>
inline constexpr bool is_stateless_range_reduction_v = is_stateless_range_reduction<F, Element, Scalar>::value;

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_RANGE_REDUCTION_TYPES_HPP
