#ifndef WHISKERTOOLBOX_V2_MASK_CENTROID_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_MASK_CENTROID_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>

class Mask2D;
template<typename T>
struct Point2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for mask centroid calculation
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * Currently no additional parameters are needed for basic centroid calculation,
 * but the struct is provided for future extensions (e.g., weighted centroids).
 * 
 * Example JSON:
 * ```json
 * {}
 * ```
 * 
 * Future extensions could include:
 * - weight_by_distance: bool - Weight points by distance from center
 * - center_x, center_y: float - Custom reference point for weighted calculation
 */
struct MaskCentroidParams {
    // Currently no parameters needed for basic centroid calculation
    // The centroid calculation assumes constant density and is parameter-free
    
    // Reserved for future extensions:
    // std::optional<bool> weight_by_distance;
    // std::optional<float> center_x;
    // std::optional<float> center_y;
};

/**
 * @brief Calculate the centroid (center of mass) of a single mask
 * 
 * This is the element-level transform: Mask2D → Point2D<float>
 * 
 * When applied to containers:
 * - MaskData (ragged) → PointData
 * 
 * The centroid is calculated as the arithmetic mean of all pixel coordinates,
 * assuming uniform density across the mask.
 * 
 * @param mask Input mask containing pixel coordinates
 * @param params Parameters (currently unused but available for future extensions)
 * @return Centroid as Point2D<float> with (x, y) coordinates
 * 
 * @note For empty masks, returns Point2D<float>(0.0f, 0.0f)
 */
Point2D<float> calculateMaskCentroid(
        Mask2D const & mask,
        MaskCentroidParams const & params);

/**
 * @brief Calculate the centroid with context support for progress/cancellation
 * 
 * Demonstrates progress reporting and cancellation checking for long-running
 * operations on large masks.
 * 
 * @param mask Input mask
 * @param params Parameters
 * @param ctx Compute context for progress reporting and cancellation
 * @return Centroid as Point2D<float>
 */
Point2D<float> calculateMaskCentroidWithContext(
        Mask2D const & mask,
        MaskCentroidParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif//WHISKERTOOLBOX_V2_MASK_CENTROID_TRANSFORM_HPP
