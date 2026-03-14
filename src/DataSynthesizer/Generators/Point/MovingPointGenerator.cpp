/**
 * @file MovingPointGenerator.cpp
 * @brief PointData generator that produces a single point moving along a trajectory.
 *
 * Registers a "MovingPoint" generator in the DataSynthesizer registry.
 * Produces a PointData with exactly one point per frame, following a trajectory
 * computed by the shared Trajectory library.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"
#include "DataSynthesizer/Trajectory/Trajectory.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

struct MovingPointParams {
    std::optional<float> start_x;
    std::optional<float> start_y;
    int num_frames = 100;

    // Trajectory parameters (flattened, all optional with defaults via accessors)
    std::optional<std::string> model;

    std::optional<float> velocity_x;
    std::optional<float> velocity_y;

    std::optional<float> amplitude_x;
    std::optional<float> amplitude_y;
    std::optional<float> frequency_x;
    std::optional<float> frequency_y;
    std::optional<float> phase_x;
    std::optional<float> phase_y;

    std::optional<float> diffusion;
    std::optional<uint64_t> seed;

    std::optional<std::string> boundary_mode;
    std::optional<float> bounds_min_x;
    std::optional<float> bounds_max_x;
    std::optional<float> bounds_min_y;
    std::optional<float> bounds_max_y;
};

WhiskerToolbox::DataSynthesizer::TrajectoryParams toTrajectoryParams(
        MovingPointParams const & p) {
    return WhiskerToolbox::DataSynthesizer::TrajectoryParams{
            .model = p.model.value_or("linear"),
            .velocity_x = p.velocity_x.value_or(1.0f),
            .velocity_y = p.velocity_y.value_or(0.0f),
            .amplitude_x = p.amplitude_x.value_or(0.0f),
            .amplitude_y = p.amplitude_y.value_or(0.0f),
            .frequency_x = p.frequency_x.value_or(0.0f),
            .frequency_y = p.frequency_y.value_or(0.0f),
            .phase_x = p.phase_x.value_or(0.0f),
            .phase_y = p.phase_y.value_or(0.0f),
            .diffusion = p.diffusion.value_or(1.0f),
            .seed = p.seed.value_or(42ULL),
            .boundary_mode = p.boundary_mode.value_or("clamp"),
            .bounds_min_x = p.bounds_min_x.value_or(0.0f),
            .bounds_max_x = p.bounds_max_x.value_or(640.0f),
            .bounds_min_y = p.bounds_min_y.value_or(0.0f),
            .bounds_max_y = p.bounds_max_y.value_or(480.0f)};
}

DataTypeVariant generateMovingPoint(MovingPointParams const & params) {
    if (params.num_frames <= 0) {
        throw std::invalid_argument("MovingPoint: num_frames must be > 0");
    }

    auto const trajectory = WhiskerToolbox::DataSynthesizer::computeTrajectory(
            params.start_x.value_or(100.0f),
            params.start_y.value_or(100.0f),
            params.num_frames, toTrajectoryParams(params));

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
