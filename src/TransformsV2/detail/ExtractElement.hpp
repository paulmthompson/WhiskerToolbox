#ifndef WHISKERTOOLBOX_V2_DETAIL_EXTRACT_ELEMENT_HPP
#define WHISKERTOOLBOX_V2_DETAIL_EXTRACT_ELEMENT_HPP

#include <type_traits>

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
decltype(auto) extractElement(IterValue const & iter_value) {
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

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_DETAIL_EXTRACT_ELEMENT_HPP