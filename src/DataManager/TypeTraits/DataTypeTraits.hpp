#ifndef WHISKERTOOLBOX_DATA_TYPE_TRAITS_HPP
#define WHISKERTOOLBOX_DATA_TYPE_TRAITS_HPP

#include <concepts>
#include <type_traits>

namespace WhiskerToolbox::TypeTraits {

/**
 * @brief Base trait structure for data container types
 * 
 * This provides a standardized interface for describing properties of data containers.
 * Each container type should define a nested `DataTraits` struct that inherits from
 * or conforms to this interface.
 * 
 * @tparam Container The container type (e.g., MaskData, LineData)
 * @tparam Element The element type stored in the container (e.g., Mask2D, Line2D)
 */
template<typename Container, typename Element>
struct DataTypeTraitsBase {
    using container_type = Container;
    using element_type = Element;
    
    /// True if container can hold multiple elements per time point
    static constexpr bool is_ragged = false;
    
    /// True if container has a TimeFrame association
    static constexpr bool is_temporal = true;
    
    /// True if elements in container have EntityId tracking
    static constexpr bool has_entity_ids = false;
    
    /// True if container represents spatial data (custom property example)
    static constexpr bool is_spatial = false;
};

/**
 * @brief Concept for types that provide DataTraits
 * 
 * A type satisfies this concept if it has a nested DataTraits type with
 * the required properties.
 */
template<typename T>
concept HasDataTraits = requires {
    typename T::DataTraits;
    typename T::DataTraits::container_type;
    typename T::DataTraits::element_type;
    { T::DataTraits::is_ragged } -> std::convertible_to<bool>;
    { T::DataTraits::is_temporal } -> std::convertible_to<bool>;
    { T::DataTraits::has_entity_ids } -> std::convertible_to<bool>;
};

/**
 * @brief Helper to access traits (with SFINAE for types without traits)
 */
template<typename T>
struct TraitsOf {
    static_assert(HasDataTraits<T>, "Type must define DataTraits");
    using type = typename T::DataTraits;
};

template<typename T>
using TraitsOf_t = typename TraitsOf<T>::type;

// Convenience aliases for common trait queries
template<typename T>
using element_type_t = typename TraitsOf_t<T>::element_type;

template<typename T>
inline constexpr bool is_ragged_v = TraitsOf_t<T>::is_ragged;

template<typename T>
inline constexpr bool is_temporal_v = TraitsOf_t<T>::is_temporal;

template<typename T>
inline constexpr bool has_entity_ids_v = TraitsOf_t<T>::has_entity_ids;

template<typename T>
inline constexpr bool is_spatial_v = TraitsOf_t<T>::is_spatial;

/**
 * @brief Concepts based on trait properties
 */
template<typename T>
concept RaggedContainer = HasDataTraits<T> && is_ragged_v<T>;

template<typename T>
concept TemporalContainer = HasDataTraits<T> && is_temporal_v<T>;

template<typename T>
concept EntityTrackedContainer = HasDataTraits<T> && has_entity_ids_v<T>;

template<typename T>
concept SpatialContainer = HasDataTraits<T> && is_spatial_v<T>;

} // namespace WhiskerToolbox::TypeTraits

#endif // WHISKERTOOLBOX_DATA_TYPE_TRAITS_HPP
