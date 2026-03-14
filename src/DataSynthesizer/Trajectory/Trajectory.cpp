/**
 * @file Trajectory.cpp
 * @brief Implementation of trajectory computation for moving spatial generators.
 *
 * Implements linear, sinusoidal, and Brownian motion models with clamp, bounce,
 * and wrap boundary enforcement applied per-step.
 */
#include "Trajectory.hpp"

#include <cassert>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>

namespace WhiskerToolbox::DataSynthesizer {

namespace {

/// Apply boundary enforcement to a single coordinate.
void applyBoundary(
        float & pos,
        float & vel,
        float min_bound,
        float max_bound,
        std::string const & mode) {

    if (mode == "clamp") {
        if (pos < min_bound) {
            pos = min_bound;
        } else if (pos > max_bound) {
            pos = max_bound;
        }
    } else if (mode == "bounce") {
        // Reflect until within bounds
        while (pos < min_bound || pos > max_bound) {
            if (pos < min_bound) {
                pos = min_bound + (min_bound - pos);
                vel = -vel;
            }
            if (pos > max_bound) {
                pos = max_bound - (pos - max_bound);
                vel = -vel;
            }
        }
    } else if (mode == "wrap") {
        float const range = max_bound - min_bound;
        if (range > 0.0f) {
            while (pos < min_bound) {
                pos += range;
            }
            while (pos > max_bound) {
                pos -= range;
            }
        }
    }
}

}// namespace

std::vector<Point2D<float>> computeTrajectory(
        float start_x,
        float start_y,
        int num_frames,
        TrajectoryParams const & params) {

    if (num_frames <= 0) {
        throw std::invalid_argument("computeTrajectory: num_frames must be > 0");
    }
    if (params.bounds_max_x <= params.bounds_min_x) {
        throw std::invalid_argument("computeTrajectory: bounds_max_x must be > bounds_min_x");
    }
    if (params.bounds_max_y <= params.bounds_min_y) {
        throw std::invalid_argument("computeTrajectory: bounds_max_y must be > bounds_min_y");
    }

    std::vector<Point2D<float>> trajectory;
    trajectory.reserve(static_cast<size_t>(num_frames));

    if (params.model == "linear") {
        float vx = params.velocity_x;
        float vy = params.velocity_y;
        float x = start_x;
        float y = start_y;

        for (int t = 0; t < num_frames; ++t) {
            if (t > 0) {
                x += vx;
                y += vy;
            }
            applyBoundary(x, vx, params.bounds_min_x, params.bounds_max_x, params.boundary_mode);
            applyBoundary(y, vy, params.bounds_min_y, params.bounds_max_y, params.boundary_mode);
            trajectory.emplace_back(x, y);
        }
    } else if (params.model == "sinusoidal") {
        constexpr float two_pi = 2.0f * std::numbers::pi_v<float>;
        float dummy_vx = 0.0f;
        float dummy_vy = 0.0f;

        for (int t = 0; t < num_frames; ++t) {
            auto const tf = static_cast<float>(t);
            float x = start_x + params.amplitude_x * std::sin(two_pi * params.frequency_x * tf + params.phase_x);
            float y = start_y + params.amplitude_y * std::sin(two_pi * params.frequency_y * tf + params.phase_y);
            applyBoundary(x, dummy_vx, params.bounds_min_x, params.bounds_max_x, params.boundary_mode);
            applyBoundary(y, dummy_vy, params.bounds_min_y, params.bounds_max_y, params.boundary_mode);
            trajectory.emplace_back(x, y);
        }
    } else if (params.model == "brownian") {
        std::mt19937_64 rng(params.seed);
        std::normal_distribution<float> dist(0.0f, params.diffusion);
        float dummy_vx = 0.0f;
        float dummy_vy = 0.0f;
        float x = start_x;
        float y = start_y;

        for (int t = 0; t < num_frames; ++t) {
            if (t > 0) {
                x += dist(rng);
                y += dist(rng);
            }
            applyBoundary(x, dummy_vx, params.bounds_min_x, params.bounds_max_x, params.boundary_mode);
            applyBoundary(y, dummy_vy, params.bounds_min_y, params.bounds_max_y, params.boundary_mode);
            trajectory.emplace_back(x, y);
        }
    } else {
        throw std::invalid_argument(
                "computeTrajectory: unknown model '" + params.model + "'. "
                                                                      "Supported: linear, sinusoidal, brownian");
    }

    return trajectory;
}

}// namespace WhiskerToolbox::DataSynthesizer
