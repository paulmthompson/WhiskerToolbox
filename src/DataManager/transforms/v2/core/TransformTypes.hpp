#ifndef WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP
#define WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <variant>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Variant type for single elements in the transform pipeline
 * 
 * Replaces std::any for better performance (no heap allocation) and type safety.
 */
using ElementVariant = std::variant<
    float,
    Point2D<float>,
    Line2D,
    Mask2D
>;

/**
 * @brief Variant type for batches of elements (for time-grouped transforms)
 * 
 * Stores contiguous vectors of elements to allow zero-copy passing to
 * transforms that expect std::span<T>.
 */
using BatchVariant = std::variant<
    std::vector<float>,
    std::vector<Point2D<float>>,
    std::vector<Line2D>,
    std::vector<Mask2D>
>;

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP
