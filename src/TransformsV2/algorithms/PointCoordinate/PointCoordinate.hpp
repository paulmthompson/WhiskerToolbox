/**
 * @file PointCoordinate.hpp
 * @brief Container transform that projects PointData coordinates to ragged analog values.
 */

#ifndef NEURALYZER_V2_POINT_COORDINATE_HPP
#define NEURALYZER_V2_POINT_COORDINATE_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>

class PointData;
class RaggedAnalogTimeSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Coordinate component to extract from each point.
 */
enum class PointCoordinateComponent {
    X,
    Y
};

/**
 * @brief Parameters for PointCoordinate.
 */
struct PointCoordinateParams {
    PointCoordinateComponent coordinate{PointCoordinateComponent::X};
};

/**
 * @brief Project PointData x or y coordinates into RaggedAnalogTimeSeries.
 *
 * Container signature: PointData -> RaggedAnalogTimeSeries.
 * Each point at a time contributes one coordinate value at the same time.
 *
 * @param input Point data with one or more points at each time
 * @param params Coordinate selection
 * @param ctx Compute context for progress/cancellation
 * @return Ragged analog series containing the selected coordinate values
 */
[[nodiscard]] std::shared_ptr<RaggedAnalogTimeSeries> pointCoordinate(
        PointData const & input,
        PointCoordinateParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_POINT_COORDINATE_HPP
