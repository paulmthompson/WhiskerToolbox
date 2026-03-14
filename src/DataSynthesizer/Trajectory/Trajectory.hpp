/**
 * @file Trajectory.hpp
 * @brief Reusable trajectory computation for moving spatial generators.
 *
 * Provides TrajectoryParams (reflect-cpp enabled) and computeTrajectory(),
 * which produces a sequence of 2D positions given a motion model and
 * boundary mode. Used by MovingPoint, MovingMask, and MovingLine generators.
 */
#ifndef WHISKERTOOLBOX_DATASYNTHESIZER_TRAJECTORY_HPP
#define WHISKERTOOLBOX_DATASYNTHESIZER_TRAJECTORY_HPP

#include "CoreGeometry/points.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace WhiskerToolbox::DataSynthesizer {

/**
 * @brief Parameters controlling trajectory computation.
 *
 * Supports three motion models (linear, sinusoidal, brownian) and three
 * boundary modes (clamp, bounce, wrap). Only the fields relevant to the
 * selected model are used; others are ignored.
 *
 * reflect-cpp enabled for automatic JSON deserialization and ParameterSchema
 * extraction. Fields are intentionally flat (no nesting) so they compose
 * cleanly when flattened into generator-specific param structs.
 */
struct TrajectoryParams {
    /// Motion model: "linear", "sinusoidal", or "brownian"
    std::string model = "linear";

    // -- Linear: position(t) = start + velocity * t --
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;

    // -- Sinusoidal: position(t) = start + amplitude * sin(2π * frequency * t + phase) --
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 0.0f;
    float frequency_y = 0.0f;
    float phase_x = 0.0f;
    float phase_y = 0.0f;

    // -- Brownian: position(t) = position(t-1) + N(0, diffusion) --
    float diffusion = 1.0f;
    uint64_t seed = 42;

    // -- Boundary behavior --
    /// Boundary mode: "clamp", "bounce", or "wrap"
    std::string boundary_mode = "clamp";
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

/**
 * @brief Compute a trajectory of num_frames positions.
 *
 * @pre num_frames > 0
 * @pre bounds_max > bounds_min for both axes
 * @pre model is one of "linear", "sinusoidal", "brownian"
 * @pre boundary_mode is one of "clamp", "bounce", "wrap"
 *
 * @param start_x  Starting X position
 * @param start_y  Starting Y position
 * @param num_frames Number of frames (positions) to generate
 * @param params   Trajectory parameters controlling motion and boundaries
 * @return Vector of num_frames positions
 */
[[nodiscard]] std::vector<Point2D<float>> computeTrajectory(
        float start_x,
        float start_y,
        int num_frames,
        TrajectoryParams const & params);

}// namespace WhiskerToolbox::DataSynthesizer

#endif// WHISKERTOOLBOX_DATASYNTHESIZER_TRAJECTORY_HPP
