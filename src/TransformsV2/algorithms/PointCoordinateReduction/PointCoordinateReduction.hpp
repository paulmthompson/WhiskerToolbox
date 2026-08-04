/**
 * @file PointCoordinateReduction.hpp
 * @brief Container transform that projects PointData coordinates to analog values, optionally reducing ragged dimensions.
 */

#ifndef NEURALYZER_V2_POINT_COORDINATE_REDUCTION_HPP
#define NEURALYZER_V2_POINT_COORDINATE_REDUCTION_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>

class PointData;
class AnalogTimeSeries;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

#include "algorithms/PointCoordinate/PointCoordinate.hpp"

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Method for reducing multiple points at a single time frame into a scalar.
 */
enum class PointReductionMethod {
    First,
    Mean,
    Sum,
    Max,
    Min
};

/**
 * @brief Parameters for PointCoordinateReduction.
 */
struct PointCoordinateReductionParams {
    PointCoordinateComponent coordinate{PointCoordinateComponent::X};
    PointReductionMethod reduction{PointReductionMethod::First};
};

/**
 * @brief Project PointData x or y coordinates into an AnalogTimeSeries, collapsing multiple points at a single time.
 *
 * Container signature: PointData -> AnalogTimeSeries.
 *
 * @param input Point data with one or more points at each time
 * @param params Coordinate selection and reduction method
 * @param ctx Compute context for progress/cancellation
 * @return Analog series containing the reduced coordinate values
 */
[[nodiscard]] std::shared_ptr<AnalogTimeSeries> pointCoordinateReduction(
        PointData const & input,
        PointCoordinateReductionParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_POINT_COORDINATE_REDUCTION_HPP
