#ifndef WHISKERTOOLBOX_V2_CONTAINER_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_CONTAINER_TRANSFORM_HPP

#include "transforms/v2/detail/ContainerTraits.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/extension/ElementTransform.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"

#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include <functional>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Helper to Extract Element Data from Various Iterator Types
// ============================================================================

/**
 * @brief Extract the actual data element from a container's iterator value
 * 
 * Different containers have different iterator structures:
 * - RaggedTimeSeries elements(): std::pair<TimeFrameIndex, DataEntry<T>>
 * - AnalogTimeSeries getAllSamples(): TimeValuePoint (has .value() method)
 * - Some containers: direct value (float, etc.)
 * 
 * This helper uses compile-time checks to extract the correct data.
 */
template<typename IterValue, typename ElementType>
decltype(auto) extractElement(IterValue const& iter_value) {
    using ValueType = std::decay_t<IterValue>;
    
    // Check if it's a pair (time, something)
    if constexpr (requires { iter_value.second; }) {
        using SecondType = std::decay_t<decltype(iter_value.second)>;
        
        // Case 1: pair.second is std::reference_wrapper<DataEntry<T>>
        if constexpr (requires { iter_value.second.get().data; }) {
            return iter_value.second.get().data;
        }
        // Case 2: pair.second has .data member directly (DataEntry or similar)
        else if constexpr (requires { iter_value.second.data; }) {
            return iter_value.second.data;
        }
        // Case 3: pair.second is the element itself
        else {
            return iter_value.second;
        }
    }
    // Not a pair - assume it's the element itself
    else {
        return iter_value;
    }
}

// ============================================================================
// Container Transform Helpers
// ============================================================================

/**
 * @brief Apply element transform to a ragged container
 * 
 * Automatically lifts an element-level transform (e.g., Mask2D → float) to
 * operate on a ragged container (e.g., MaskData → RaggedAnalogTimeSeries).
 * 
 * Uses std::ranges to preserve the ragged structure (multiple values per time).
 * 
 * @tparam InContainer Input container type (e.g., MaskData)
 * @tparam OutContainer Output container type (e.g., RaggedAnalogTimeSeries)
 * @tparam InElement Input element type (e.g., Mask2D)
 * @tparam OutElement Output element type (e.g., float)
 * @tparam Params Parameter type for the transform
 * 
 * @param input Input container
 * @param transform_name Name of registered transform
 * @param params Parameters for the transform
 * @return Shared pointer to output container
 */
template<typename InContainer, typename OutContainer, typename InElement, typename OutElement, typename Params>
requires RaggedContainer<InContainer> && std::is_same_v<ElementFor_t<InContainer>, InElement>
std::shared_ptr<OutContainer> applyElementTransform(
    InContainer const& input,
    std::string const& transform_name,
    Params const& params)
{
    auto& registry = ElementRegistry::instance();
    
    // Create output container
    auto output = std::make_shared<OutContainer>();
    output->setTimeFrame(input.getTimeFrame());
    
    // Transform each element using elements() which provides (time, entry) pairs
    for (auto const& elem : input.elements()) {
        // Extract the actual data using our helper
        InElement const& data = extractElement<decltype(elem), InElement>(elem);
        
        // Transform it
        auto result = registry.execute<InElement, OutElement, Params>(
            transform_name, data, params);
        
        // Get the time (first element of pair)
        auto time = elem.first;
        
        // Add to output
        output->appendAtTime(time, std::vector<OutElement>{result}, NotifyObservers::No);
    }
    
    return output;
}

/**
 * @brief Apply element transform to a ragged container (no params version)
 */
template<typename InContainer, typename OutContainer, typename InElement, typename OutElement>
requires RaggedContainer<InContainer> && std::is_same_v<ElementFor_t<InContainer>, InElement>
std::shared_ptr<OutContainer> applyElementTransform(
    InContainer const& input,
    std::string const& transform_name)
{
    return applyElementTransform<InContainer, OutContainer, InElement, OutElement, NoParams>(
        input, transform_name, NoParams{});
}

/**
 * @brief Apply time-grouped transform to reduce ragged time series
 * 
 * Applies a transform that operates on all values at each time point,
 * typically reducing multiple values to fewer values (e.g., sum reduction).
 * 
 * Example: RaggedAnalogTimeSeries → AnalogTimeSeries (many floats per time → one float per time)
 * 
 * @tparam InContainer Input container type (e.g., RaggedAnalogTimeSeries)
 * @tparam OutContainer Output container type (e.g., AnalogTimeSeries)
 * @tparam InElement Input element type (e.g., float)
 * @tparam OutElement Output element type (e.g., float)
 * @tparam Params Parameter type for the transform
 * 
 * @param input Input container
 * @param transform_name Name of registered time-grouped transform
 * @param params Parameters for the transform
 * @return Shared pointer to output container
 */
template<typename InContainer, typename OutContainer, typename InElement, typename OutElement, typename Params>
requires RaggedContainer<InContainer> && std::is_same_v<ElementFor_t<InContainer>, InElement>
std::shared_ptr<OutContainer> applyTimeGroupedTransform(
    InContainer const& input,
    std::string const& transform_name,
    Params const& params)
{
    auto& registry = ElementRegistry::instance();
    
    // For RaggedAnalogTimeSeries → AnalogTimeSeries
    if constexpr (std::is_same_v<InContainer, RaggedAnalogTimeSeries> && 
                  std::is_same_v<OutContainer, AnalogTimeSeries>) {
        
        // Collect all time indices
        auto time_indices = input.getTimeIndices();
        std::vector<OutElement> output_values;
        output_values.reserve(time_indices.size());
        
        // Apply transform at each time point
        for (auto time : time_indices) {
            auto data_at_time = input.getDataAtTime(time);
            auto result = registry.executeTimeGrouped<InElement, OutElement, Params>(
                transform_name, data_at_time, params);
            
            // Should produce exactly one value for reduction
            if (!result.empty()) {
                output_values.push_back(result[0]);
            } else {
                output_values.push_back(OutElement{});
            }
        }
        
        // Create output container
        std::map<int, OutElement> output_map;
        for (size_t i = 0; i < time_indices.size(); ++i) {
            output_map[time_indices[i].getValue()] = output_values[i];
        }
        
        auto output = std::make_shared<AnalogTimeSeries>(output_map);
        output->setTimeFrame(input.getTimeFrame());
        
        return output;
    }
    else {
        static_assert(std::is_same_v<InContainer, void>, "Unsupported container combination");
    }
}

/**
 * @brief Apply time-grouped transform (no params version)
 */
template<typename InContainer, typename OutContainer, typename InElement, typename OutElement>
requires RaggedContainer<InContainer> && std::is_same_v<ElementFor_t<InContainer>, InElement>
std::shared_ptr<OutContainer> applyTimeGroupedTransform(
    InContainer const& input,
    std::string const& transform_name)
{
    return applyTimeGroupedTransform<InContainer, OutContainer, InElement, OutElement, NoParams>(
        input, transform_name, NoParams{});
}

// ============================================================================
// View-Based Transform Helpers (Lazy Evaluation)
// ============================================================================

/**
 * @brief Apply element transform to a container, returning a lazy view
 * 
 * This function returns a lazy range view that applies the transform on-demand
 * as elements are accessed. No materialization occurs until the view is consumed
 * (e.g., by constructing a container from it).
 * 
 * The returned view preserves the (TimeFrameIndex, TransformedData) structure,
 * making it suitable for further view-based transformations or final materialization.
 * 
 * @tparam InContainer Input container type (must have elements() view)
 * @tparam InElement Input element type
 * @tparam OutElement Output element type
 * @tparam Params Parameter type for the transform
 * 
 * @param input Input container
 * @param transform_name Name of registered transform
 * @param params Parameters for the transform
 * @return Lazy range view of (TimeFrameIndex, OutElement) pairs
 * 
 * @example
 * ```cpp
 * // No materialization happens here - just creates a view
 * auto view = applyElementTransformView<MaskData, Mask2D, float, MaskAreaParams>(
 *     mask_data, "CalculateMaskArea", params);
 * 
 * // Chain another transform on the view
 * auto chained_view = view | std::views::transform([](auto pair) { 
 *     return std::make_pair(pair.first, pair.second * 2.0f);
 * });
 * 
 * // Materialize only when needed
 * auto result = std::make_shared<RaggedAnalogTimeSeries>(chained_view);
 * ```
 */
template<typename InContainer, typename InElement, typename OutElement, typename Params>
requires requires(InContainer const& c) { c.elements(); } && 
         std::is_same_v<ElementFor_t<InContainer>, InElement>
auto applyElementTransformView(
    InContainer const& input,
    std::string const& transform_name,
    Params const& params)
{
    auto& registry = ElementRegistry::instance();
    
    // Create a view that transforms each element lazily
    return input.elements() | std::views::transform([&registry, transform_name, params](auto const& elem) {
        // Extract the actual data element
        InElement const& data = extractElement<decltype(elem), InElement>(elem);
        
        // Transform it (this is the only computation that happens per element access)
        auto result = registry.execute<InElement, OutElement, Params>(
            transform_name, data, params);
        
        // Get the time from the element (first element of pair)
        auto time = elem.first;
        
        // Return (time, transformed_data) pair
        return std::make_pair(time, result);
    });
}

/**
 * @brief Apply element transform returning a lazy view (no params version)
 */
template<typename InContainer, typename InElement, typename OutElement>
requires requires(InContainer const& c) { c.elements(); } && 
         std::is_same_v<ElementFor_t<InContainer>, InElement>
auto applyElementTransformView(
    InContainer const& input,
    std::string const& transform_name)
{
    return applyElementTransformView<InContainer, InElement, OutElement, NoParams>(
        input, transform_name, NoParams{});
}

// ============================================================================
// Simplified API using type deduction
// ============================================================================

/**
 * @brief Apply a registered transform by name, automatically deducing container types
 * 
 * This is a simplified API that uses the registry metadata to determine
 * the appropriate output container type.
 * 
 * @tparam InContainer Input container type
 * @tparam Params Parameter type
 * 
 * @param input Input container
 * @param transform_name Name of registered transform
 * @param params Parameters for the transform
 * @return Shared pointer to output container (type determined by registry)
 */
template<typename InContainer, typename Params>
auto applyTransform(
    InContainer const& input,
    std::string const& transform_name,
    Params const& params)
{
    auto& registry = ElementRegistry::instance();
    auto const* meta = registry.getMetadata(transform_name);
    
    if (!meta) {
        throw std::runtime_error("Transform not found: " + transform_name);
    }
    
    using InElement = ElementFor_t<InContainer>;
    
    // Verify input type matches
    if (meta->input_type != typeid(InElement)) {
        throw std::runtime_error("Input type mismatch for transform: " + transform_name);
    }
    
    // Determine output type and dispatch
    if (meta->output_type == typeid(float)) {
        if (meta->is_time_grouped) {
            // Time-grouped transform: likely reducing to single value per time
            return applyTimeGroupedTransform<InContainer, AnalogTimeSeries, InElement, float, Params>(
                input, transform_name, params);
        } else {
            // Element transform: preserves ragged structure
            return applyElementTransform<InContainer, RaggedAnalogTimeSeries, InElement, float, Params>(
                input, transform_name, params);
        }
    }
    
    throw std::runtime_error("Unsupported output type for transform: " + transform_name);
}

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_CONTAINER_TRANSFORM_HPP
