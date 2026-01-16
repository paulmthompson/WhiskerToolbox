#ifndef WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP
#define WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame.hpp"

#include <variant>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Describes the lineage relationship a transform creates
 * 
 * This enum allows transforms to declare what kind of entity relationship
 * exists between their input and output data. The TransformPipeline uses
 * this to automatically record lineage when executing transforms.
 */
enum class TransformLineageType {
    /// No lineage tracking (default) - output is independent of input entities
    None,
    
    /// 1:1 mapping by time: output[t, i] derives from input[t, i]
    /// Example: calculateMaskArea (each output area derives from one mask)
    OneToOneByTime,
    
    /// N:1 mapping by time: output[t] derives from ALL input entities at time t
    /// Example: SumReduction (single sum from all values at each time)
    AllToOneByTime,
    
    /// Subset mapping: output contains a subset of input entities
    /// Example: Filtering by property threshold
    Subset,
    
    /// Transform creates source data (no input lineage)
    /// Example: Loading from file, user annotation
    Source
};

/**
 * @brief Variant type for single elements in the transform pipeline
 * 
 * Includes:
 * - float: Scalar results (e.g., normalized times, areas) - first for default construction
 * - TimeFrameIndex: Fundamental time type for temporal transforms
 * - Point2D<float>: 2D spatial points
 * - Line2D: Polylines/curves
 * - Mask2D: Binary masks
 */
using ElementVariant = std::variant<
    float,
    TimeFrameIndex,
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
    std::vector<TimeFrameIndex>,
    std::vector<Point2D<float>>,
    std::vector<Line2D>,
    std::vector<Mask2D>
>;

} // namespace WhiskerToolbox::Transforms::V2

#endif // WHISKERTOOLBOX_V2_TRANSFORM_TYPES_HPP
