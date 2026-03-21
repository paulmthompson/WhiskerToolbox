/**
 * @file MovingLineGenerator.cpp
 * @brief LineData generator that produces a line segment moving along a trajectory.
 *
 * Registers a "MovingLine" generator in the DataSynthesizer registry.
 * Produces a LineData where each frame contains a line whose centroid follows
 * a trajectory computed by the shared Trajectory library. The line shape is
 * defined once as offsets from the centroid, then rigidly translated at each
 * frame. No clipping is applied — vertices are float-valued and may extend
 * beyond any nominal bounds.
 *
 * Uses MotionModelVariant (rfl::TaggedUnion) and BoundaryMode (enum class)
 * for typed, combo-box-driven parameter UI via AutoParamWidget.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"
#include "DataSynthesizer/Trajectory/MotionParams.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using WhiskerToolbox::DataSynthesizer::BoundaryMode;
using WhiskerToolbox::DataSynthesizer::BoundaryParams;
using WhiskerToolbox::DataSynthesizer::LinearMotionParams;
using WhiskerToolbox::DataSynthesizer::MotionModelVariant;

struct MovingLineParams {
    float start_x = 0.0f;
    float start_y = 0.0f;
    float end_x = 100.0f;
    float end_y = 100.0f;
    int num_points_per_line = 50;
    int num_frames = 100;

    /// Starting X position of the line centroid for the trajectory.
    float trajectory_start_x = 100.0f;
    /// Starting Y position of the line centroid for the trajectory.
    float trajectory_start_y = 100.0f;

    MotionModelVariant motion = LinearMotionParams{};
    BoundaryMode boundary_mode = BoundaryMode::clamp;
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

DataTypeVariant generateMovingLine(MovingLineParams const & params) {
    if (params.num_points_per_line <= 1) {
        throw std::invalid_argument("MovingLine: num_points_per_line must be > 1");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("MovingLine: num_frames must be > 0");
    }

    // Build the line shape as offsets from the centroid
    auto const n = static_cast<size_t>(params.num_points_per_line);

    // Compute the centroid of the line definition
    float const centroid_x = (params.start_x + params.end_x) * 0.5f;
    float const centroid_y = (params.start_y + params.end_y) * 0.5f;

    // Pre-compute vertex offsets relative to centroid
    std::vector<Point2D<float>> offsets;
    offsets.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(n - 1);
        float const x = params.start_x + t * (params.end_x - params.start_x);
        float const y = params.start_y + t * (params.end_y - params.start_y);
        offsets.emplace_back(x - centroid_x, y - centroid_y);
    }

    // Compute trajectory for the centroid
    BoundaryParams const boundary{
            params.boundary_mode,
            params.bounds_min_x,
            params.bounds_max_x,
            params.bounds_min_y,
            params.bounds_max_y};
    auto const tp = WhiskerToolbox::DataSynthesizer::toTrajectoryParams(
            params.motion, boundary);

    auto const trajectory = WhiskerToolbox::DataSynthesizer::computeTrajectory(
            params.trajectory_start_x,
            params.trajectory_start_y,
            params.num_frames, tp);

    auto line_data = std::make_shared<LineData>();

    for (int i = 0; i < params.num_frames; ++i) {
        auto const & pos = trajectory[static_cast<size_t>(i)];
        std::vector<Point2D<float>> points;
        points.reserve(n);
        for (auto const & offset: offsets) {
            points.emplace_back(pos.x + offset.x, pos.y + offset.y);
        }
        Line2D const line(std::move(points));
        line_data->addAtTime(TimeFrameIndex(i), line, NotifyObservers::No);
    }

    return line_data;
}

auto const moving_line_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<MovingLineParams>(
                "MovingLine",
                generateMovingLine,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a line segment that moves along a trajectory. "
                                       "The line shape is defined by start/end points and rigidly "
                                       "translated so its centroid follows the trajectory. "
                                       "Supports linear, sinusoidal, and Brownian motion models "
                                       "with clamp, bounce, or wrap boundary modes. No clipping — "
                                       "vertices may extend beyond bounds. "
                                       "Deterministic: same seed produces the same output for Brownian motion.",
                        .category = "Spatial",
                        .output_type = "LineData"});

}// namespace
