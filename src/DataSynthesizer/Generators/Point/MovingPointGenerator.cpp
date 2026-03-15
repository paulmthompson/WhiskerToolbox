/**
 * @file MovingPointGenerator.cpp
 * @brief PointData generator that produces a single point moving along a trajectory.
 *
 * Registers a "MovingPoint" generator in the DataSynthesizer registry.
 * Produces a PointData with exactly one point per frame, following a trajectory
 * computed by the shared Trajectory library.
 *
 * Uses MotionModelVariant (rfl::TaggedUnion) and BoundaryMode (enum class)
 * for typed, combo-box-driven parameter UI via AutoParamWidget.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"
#include "DataSynthesizer/Trajectory/MotionParams.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <memory>
#include <stdexcept>

namespace {

using WhiskerToolbox::DataSynthesizer::BoundaryMode;
using WhiskerToolbox::DataSynthesizer::BoundaryParams;
using WhiskerToolbox::DataSynthesizer::LinearMotionParams;
using WhiskerToolbox::DataSynthesizer::MotionModelVariant;

struct MovingPointParams {
    float start_x = 100.0f;
    float start_y = 100.0f;
    int num_frames = 100;
    MotionModelVariant motion = LinearMotionParams{};
    BoundaryMode boundary_mode = BoundaryMode::clamp;
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

DataTypeVariant generateMovingPoint(MovingPointParams const & params) {
    if (params.num_frames <= 0) {
        throw std::invalid_argument("MovingPoint: num_frames must be > 0");
    }

    BoundaryParams const boundary{
            params.boundary_mode,
            params.bounds_min_x,
            params.bounds_max_x,
            params.bounds_min_y,
            params.bounds_max_y};
    auto const tp = WhiskerToolbox::DataSynthesizer::toTrajectoryParams(
            params.motion, boundary);

    auto const trajectory = WhiskerToolbox::DataSynthesizer::computeTrajectory(
            params.start_x,
            params.start_y,
            params.num_frames, tp);

    auto point_data = std::make_shared<PointData>();

    for (int i = 0; i < params.num_frames; ++i) {
        std::vector<Point2D<float>> const single_point{trajectory[static_cast<size_t>(i)]};
        point_data->addAtTime(TimeFrameIndex(i), single_point, NotifyObservers::No);
    }

    return point_data;
}

auto const moving_point_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<MovingPointParams>(
                "MovingPoint",
                generateMovingPoint,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a single point that moves along a trajectory. "
                                       "Supports linear, sinusoidal, and Brownian motion models "
                                       "with clamp, bounce, or wrap boundary modes. "
                                       "Deterministic: same seed produces the same output for Brownian motion.",
                        .category = "Spatial",
                        .output_type = "PointData"});

}// namespace
