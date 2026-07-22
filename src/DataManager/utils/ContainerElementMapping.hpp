/// @file ContainerElementMapping.hpp
/// @brief Compile-time mapping between data element types and container types.

#ifndef NEURALYZER_CONTAINER_ELEMENT_MAPPING_HPP
#define NEURALYZER_CONTAINER_ELEMENT_MAPPING_HPP

#include "TypeTraits/DataTypeTraits.hpp"

#include <memory>
#include <type_traits>

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class LineData;
class MaskData;
class PointData;
class RaggedAnalogTimeSeries;
class TensorData;

class Line2D;
class Mask2D;
template<typename T>
struct Point2D;

namespace Neuralyzer::TypeTraits {

// ============================================================================
// Element type → container type
// ============================================================================

/// @brief Maps element types to their default container types.
template<typename ElementType>
struct ContainerFor;

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

/// float maps to RaggedAnalogTimeSeries by default (multi-value-per-time context).
/// For single-value-per-time, use AnalogTimeSeries via contextual resolution at runtime.
template<>
struct ContainerFor<float> {
    using type = RaggedAnalogTimeSeries;
    using element_type = float;
    using container_ptr = std::shared_ptr<RaggedAnalogTimeSeries>;
};

template<typename Element>
using ContainerFor_t = typename ContainerFor<Element>::type;

template<typename Element>
using ContainerPtr_t = typename ContainerFor<Element>::container_ptr;

// ============================================================================
// Container type → element type
// ============================================================================

/// @brief Maps container types to their element types.
template<typename ContainerType>
struct ElementFor;

template<typename Container>
    requires HasDataTraits<Container>
struct ElementFor<Container> {
    using type = element_type_t<Container>;
};

template<typename Container>
using ElementFor_t = typename ElementFor<Container>::type;

// ============================================================================
// SFINAE helpers
// ============================================================================

/// @brief Concept for containers that provide an elements() method.
template<typename T>
concept HasElements = requires(T const & t) {
    { t.elements() };
};

/// @brief SFINAE-safe ElementFor; yields void for unsupported containers.
template<typename Container, typename = void>
struct ElementForSafe {
    using type = void;
    static constexpr bool is_valid = false;
};

template<typename Container>
struct ElementForSafe<Container, std::void_t<typename ElementFor<Container>::type>> {
    using type = typename ElementFor<Container>::type;
    static constexpr bool is_valid = true;
};

template<typename Container>
using ElementForSafe_t = typename ElementForSafe<Container>::type;

/// @brief True when the container supports element-level transform pipelines (has elements()).
template<typename Container>
inline constexpr bool has_element_type_v =
        ElementForSafe<Container>::is_valid && HasElements<Container>;

} // namespace Neuralyzer::TypeTraits

#endif // NEURALYZER_CONTAINER_ELEMENT_MAPPING_HPP
