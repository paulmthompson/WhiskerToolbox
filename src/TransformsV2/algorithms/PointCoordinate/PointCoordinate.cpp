/**
 * @file PointCoordinate.cpp
 * @brief Implementation of PointData coordinate projection.
 */

#include "PointCoordinate.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "Points/Point_Data.hpp"
#include "TransformsV2/core/ComputeContext.hpp"

#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

[[nodiscard]] float coordinateValue(
        Point2D<float> const & point,
        PointCoordinateComponent coordinate) {
    switch (coordinate) {
        case PointCoordinateComponent::X:
            return point.x;
        case PointCoordinateComponent::Y:
            return point.y;
    }
    return point.x;
}

}// namespace

std::shared_ptr<RaggedAnalogTimeSeries> pointCoordinate(
        PointData const & input,
        PointCoordinateParams const & params,
        ComputeContext const & ctx) {
    auto output = std::make_shared<RaggedAnalogTimeSeries>();
    output->setTimeFrame(input.getTimeFrame());

    std::size_t processed = 0;
    for (auto const & [time, entry]: input.elements()) {
        if (ctx.shouldCancel()) {
            return nullptr;
        }

        std::vector<float> values{coordinateValue(entry.data, params.coordinate)};
        output->appendAtTime(time, std::move(values), NotifyObservers::No);
        ++processed;
    }

    if (processed > 0) {
        ctx.reportProgress(100);
    }
    return output;
}

}// namespace Neuralyzer::Transforms::V2::Examples
