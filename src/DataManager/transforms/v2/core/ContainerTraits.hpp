#ifndef WHISKERTOOLBOX_V2_CONTAINER_TRAITS_HPP
#define WHISKERTOOLBOX_V2_CONTAINER_TRAITS_HPP

#include "TypeTraits/DataTypeTraits.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

class MaskData;
class LineData;
class PointData;
class AnalogTimeSeries;
class RaggedAnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;

class Mask2D;
class Line2D;
template<typename T>
struct Point2D;

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Element Type to Container Type Mapping
// ============================================================================

/**
 * @brief Maps element types to their container types
 * 
 * Defines the bidirectional relationship between:
 * - Element types (Mask2D, Line2D, Point2D<float>, float)
 * - Container types (MaskData, LineData, PointData, AnalogTimeSeries)
 */

// Primary template (undefined for unsupported types)
template<typename ElementType>
struct ContainerFor;

// Specializations for supported element types

template<>
struct ContainerFor<Mask2D> {
    using type = MaskData;
    using element_type = Mask2D;
    using container_ptr = std::shared_ptr<MaskData>;
};

template<>
struct ContainerFor<Line2D> {
    using type = LineData;
    using element_type = Line2D;
    using container_ptr = std::shared_ptr<LineData>;
};

template<>
struct ContainerFor<Point2D<float>> {
    using type = PointData;
    using element_type = Point2D<float>;
    using container_ptr = std::shared_ptr<PointData>;
};

// float → RaggedAnalogTimeSeries (used with ragged containers like MaskData)
// Note: For non-ragged single-value-per-time containers, would use AnalogTimeSeries
template<>
struct ContainerFor<float> {
    using type = RaggedAnalogTimeSeries;
    using element_type = float;
    using container_ptr = std::shared_ptr<RaggedAnalogTimeSeries>;
};

// Helper alias
template<typename Element>
using ContainerFor_t = typename ContainerFor<Element>::type;

template<typename Element>
using ContainerPtr_t = typename ContainerFor<Element>::container_ptr;

// ============================================================================
// Container Type to Element Type Mapping (Reverse)
// ============================================================================

/**
 * @brief Maps container types back to their element types
 */
template<typename ContainerType>
struct ElementFor;

template<>
struct ElementFor<MaskData> {
    using type = Mask2D;
};

template<>
struct ElementFor<LineData> {
    using type = Line2D;
};

template<>
struct ElementFor<PointData> {
    using type = Point2D<float>;
};

template<>
struct ElementFor<AnalogTimeSeries> {
    using type = float;
};

template<>
struct ElementFor<RaggedAnalogTimeSeries> {
    using type = float;
};

// Helper alias
template<typename Container>
using ElementFor_t = typename ElementFor<Container>::type;

// ============================================================================
// Raggedness Traits (Orthogonal to Element Type)
// ============================================================================

/**
 * @brief Trait to determine if a container is ragged (multiple entries per time)
 * 
 * Ragged containers can have variable numbers of elements at each time point.
 * Non-ragged containers have exactly one element per time point.
 * 
 * NOTE: This now delegates to the DataTraits system for consistency.
 */
template<typename T>
struct is_ragged : std::bool_constant<TypeTraits::is_ragged_v<T>> {};

template<typename T>
inline constexpr bool is_ragged_v = is_ragged<T>::value;

/**
 * @brief Get the corresponding non-ragged container for an element type
 * 
 * Maps element types to single-value-per-time containers:
 * - float → AnalogTimeSeries (not RaggedAnalogTimeSeries)
 */
template<typename ElementType>
struct NonRaggedContainerFor;

template<>
struct NonRaggedContainerFor<float> {
    using type = AnalogTimeSeries;
};

template<typename T>
using NonRaggedContainerFor_t = typename NonRaggedContainerFor<T>::type;

/**
 * @brief Get the corresponding ragged container for an element type
 * 
 * Maps element types to multi-value-per-time containers:
 * - Mask2D → MaskData
 * - Line2D → LineData
 * - Point2D<float> → PointData
 * - float → RaggedAnalogTimeSeries
 * - std::vector<float> → RaggedAnalogTimeSeries
 */
template<typename ElementType>
struct RaggedContainerFor;

template<>
struct RaggedContainerFor<Mask2D> {
    using type = MaskData;
};

template<>
struct RaggedContainerFor<Line2D> {
    using type = LineData;
};

template<>
struct RaggedContainerFor<Point2D<float>> {
    using type = PointData;
};

template<>
struct RaggedContainerFor<float> {
    using type = RaggedAnalogTimeSeries;
};

template<>
struct RaggedContainerFor<std::vector<float>> {
    using type = RaggedAnalogTimeSeries;
};

template<typename T>
using RaggedContainerFor_t = typename RaggedContainerFor<T>::type;

// ============================================================================
// Container Type Traits
// ============================================================================

/**
 * @brief Concept for temporal containers (have TimeFrame)
 * 
 * NOTE: This now uses the DataTraits system for consistency.
 */
template<typename T>
concept TemporalContainer = TypeTraits::HasDataTraits<T> &&
                            TypeTraits::is_temporal_v<T>;

/**
 * @brief Concept for ragged time series containers
 * 
 * NOTE: This now uses the DataTraits system for consistency.
 */
template<typename T>
concept RaggedContainer = TypeTraits::HasDataTraits<T> &&
                          TypeTraits::is_ragged_v<T>;

/**
 * @brief Concept for containers with EntityIds
 * 
 * NOTE: This now uses the DataTraits system for consistency.
 */
template<typename T>
concept EntityContainer = TypeTraits::HasDataTraits<T> &&
                          TypeTraits::has_entity_ids_v<T>;

/**
 * @brief Check if type is a known container type
 */
template<typename T>
struct is_container : std::false_type {};

template<>
struct is_container<MaskData> : std::true_type {};
template<>
struct is_container<LineData> : std::true_type {};
template<>
struct is_container<PointData> : std::true_type {};
template<>
struct is_container<AnalogTimeSeries> : std::true_type {};
template<>
struct is_container<RaggedAnalogTimeSeries> : std::true_type {};
template<>
struct is_container<DigitalEventSeries> : std::true_type {};
template<>
struct is_container<DigitalIntervalSeries> : std::true_type {};

template<typename T>
inline constexpr bool is_container_v = is_container<T>::value;

// ============================================================================
// Type Index Utilities
// ============================================================================

/**
 * @brief Get type_index for element type
 */
template<typename Element>
std::type_index getElementTypeIndex() {
    return std::type_index(typeid(Element));
}

/**
 * @brief Get type_index for container type
 */
template<typename Container>
std::type_index getContainerTypeIndex() {
    return std::type_index(typeid(Container));
}

/**
 * @brief Runtime mapping from element type_index to container type_index
 * 
 * NOTE: Implementation moved to .cpp file to avoid issues with forward declarations
 * and typeid on incomplete types
 */
class TypeIndexMapper {
public:
    static std::type_index elementToContainer(std::type_index element_type);
    static std::type_index containerToElement(std::type_index container_type);
    static std::string containerToString(std::type_index container_type);
    static std::type_index stringToContainer(std::string const & name);
};

// ============================================================================
// Transform Type Compatibility Checking
// ============================================================================

/**
 * @brief Check if transform types are compatible
 */
template<typename In, typename Out>
struct TransformCompatible {
    // Compatible if:
    // 1. Both are known element types, or
    // 2. In is element type and Out is compatible element type
    static constexpr bool value =
            requires { typename ContainerFor<In>::type; } &&
            requires { typename ContainerFor<Out>::type; };
};

template<typename In, typename Out>
inline constexpr bool TransformCompatible_v = TransformCompatible<In, Out>::value;

/**
 * @brief Check if containers can be chained
 */
template<typename Container1, typename Container2>
struct ContainerChainable {
    using Element1 = ElementFor_t<Container1>;
    using Container1_Out = ContainerFor_t<Element1>;
    using Element2 = ElementFor_t<Container2>;

    // Chainable if output of Container1 can be input to Container2
    // This is always true in current design, but could have restrictions
    static constexpr bool value = true;
};

template<typename Container1, typename Container2>
inline constexpr bool ContainerChainable_v = ContainerChainable<Container1, Container2>::value;

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_CONTAINER_TRAITS_HPP
