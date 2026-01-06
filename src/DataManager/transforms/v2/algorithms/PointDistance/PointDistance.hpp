#ifndef WHISKERTOOLBOX_V2_POINT_DISTANCE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_POINT_DISTANCE_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

class PointData;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::PointDistance {

/**
 * @brief Reference point type for distance calculation
 */
enum class ReferenceType {
    GlobalAverage,      // Average of all X and Y values across all time
    RollingAverage,     // Rolling average of X and Y values over a window
    SetPoint,           // User-specified fixed point
    OtherPointData      // Another PointData object (e.g., compare jaw to tongue)
};

/**
 * @brief Parameters for point distance calculation
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 *
 * Example JSON for global average:
 * ```json
 * {
 *   "reference_type": "GlobalAverage"
 * }
 * ```
 *
 * Example JSON for rolling average:
 * ```json
 * {
 *   "reference_type": "RollingAverage",
 *   "window_size": 1000
 * }
 * ```
 *
 * Example JSON for set point:
 * ```json
 * {
 *   "reference_type": "SetPoint",
 *   "reference_x": 100.0,
 *   "reference_y": 200.0
 * }
 * ```
 *
 * Example JSON for other point data:
 * ```json
 * {
 *   "reference_type": "OtherPointData",
 *   "reference_point_data_name": "jaw_point"
 * }
 * ```
 */
struct PointDistanceParams {
    // Type of reference point
    ReferenceType reference_type = ReferenceType::GlobalAverage;

    // Rolling average window size (frames) - only used for RollingAverage
    // Must be positive
    std::optional<rfl::Validator<int, rfl::Minimum<1>>> window_size;

    // Set point coordinates - only used for SetPoint
    std::optional<float> reference_x;
    std::optional<float> reference_y;

    // Name of reference point data - only used for OtherPointData
    std::optional<std::string> reference_point_data_name;

    // Helper methods to get values with defaults
    int getWindowSize() const {
        return window_size.has_value() ? window_size.value().value() : 1000;
    }

    float getReferenceX() const {
        return reference_x.value_or(0.0f);
    }

    float getReferenceY() const {
        return reference_y.value_or(0.0f);
    }

    std::string getReferencePointDataName() const {
        return reference_point_data_name.value_or("");
    }
};

/**
 * @brief Result structure for point distance calculation
 *
 * Contains the time index and distance value
 */
struct PointDistanceResult {
    int time;
    float distance;
};

/**
 * @brief Calculate euclidean distance of points from a reference
 *
 * This transform calculates the euclidean distance from each point to a reference point.
 * The reference can be:
 * - Global average of all points
 * - Rolling average over a time window
 * - A fixed set point
 * - Another point data object (for comparing two features)
 *
 * Handles missing points by skipping them in the output.
 *
 * @param point_data Input point data
 * @param params Parameters specifying reference type and settings
 * @param reference_point_data Optional reference point data (used when reference_type is OtherPointData)
 * @return Vector of (time, distance) pairs
 */
std::vector<PointDistanceResult> calculatePointDistance(
        PointData const & point_data,
        PointDistanceParams const & params,
        PointData const * reference_point_data = nullptr);

/**
 * @brief Alternative: Calculate point distance with context support
 *
 * Demonstrates progress reporting and cancellation checking.
 */
std::vector<PointDistanceResult> calculatePointDistanceWithContext(
        PointData const & point_data,
        PointDistanceParams const & params,
        ComputeContext const & ctx,
        PointData const * reference_point_data = nullptr);

}// namespace WhiskerToolbox::Transforms::V2::PointDistance

#endif// WHISKERTOOLBOX_V2_POINT_DISTANCE_TRANSFORM_HPP

