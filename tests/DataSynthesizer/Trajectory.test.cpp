/**
 * @file Trajectory.test.cpp
 * @brief Unit tests for the Trajectory library (computeTrajectory).
 */
#include "DataSynthesizer/Trajectory/Trajectory.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>

using namespace WhiskerToolbox::DataSynthesizer;
using Catch::Matchers::WithinAbs;

// =====================================================================
// Basic properties
// =====================================================================

TEST_CASE("computeTrajectory returns correct number of positions", "[Trajectory]") {
    TrajectoryParams params;
    auto traj = computeTrajectory(100.0f, 100.0f, 50, params);
    REQUIRE(traj.size() == 50);
}

TEST_CASE("computeTrajectory first position is the start position", "[Trajectory]") {
    TrajectoryParams params;
    auto traj = computeTrajectory(42.0f, 99.0f, 10, params);
    REQUIRE_THAT(traj[0].x, WithinAbs(42.0f, 1e-5));
    REQUIRE_THAT(traj[0].y, WithinAbs(99.0f, 1e-5));
}

TEST_CASE("computeTrajectory single frame returns start position", "[Trajectory]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 5.0f;
    auto traj = computeTrajectory(10.0f, 20.0f, 1, params);
    REQUIRE(traj.size() == 1);
    REQUIRE_THAT(traj[0].x, WithinAbs(10.0f, 1e-5));
    REQUIRE_THAT(traj[0].y, WithinAbs(20.0f, 1e-5));
}

// =====================================================================
// Linear motion model
// =====================================================================

TEST_CASE("Linear trajectory moves at constant velocity", "[Trajectory][Linear]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 2.0f;
    params.velocity_y = 3.0f;
    params.boundary_mode = "clamp";
    params.bounds_max_x = 10000.0f;
    params.bounds_max_y = 10000.0f;

    auto traj = computeTrajectory(0.0f, 0.0f, 5, params);

    // Frame 0: start
    REQUIRE_THAT(traj[0].x, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(traj[0].y, WithinAbs(0.0f, 1e-5));
    // Frame 1: start + 1 * velocity
    REQUIRE_THAT(traj[1].x, WithinAbs(2.0f, 1e-5));
    REQUIRE_THAT(traj[1].y, WithinAbs(3.0f, 1e-5));
    // Frame 4: start + 4 * velocity
    REQUIRE_THAT(traj[4].x, WithinAbs(8.0f, 1e-5));
    REQUIRE_THAT(traj[4].y, WithinAbs(12.0f, 1e-5));
}

TEST_CASE("Linear trajectory with zero velocity is stationary", "[Trajectory][Linear]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 0.0f;
    params.velocity_y = 0.0f;

    auto traj = computeTrajectory(50.0f, 75.0f, 10, params);

    for (auto const & p: traj) {
        REQUIRE_THAT(p.x, WithinAbs(50.0f, 1e-5));
        REQUIRE_THAT(p.y, WithinAbs(75.0f, 1e-5));
    }
}

// =====================================================================
// Sinusoidal motion model
// =====================================================================

TEST_CASE("Sinusoidal trajectory returns near start after one period", "[Trajectory][Sinusoidal]") {
    TrajectoryParams params;
    params.model = "sinusoidal";
    params.amplitude_x = 50.0f;
    params.amplitude_y = 30.0f;
    params.frequency_x = 1.0f / 100.0f;// period = 100 frames
    params.frequency_y = 1.0f / 100.0f;
    params.phase_x = 0.0f;
    params.phase_y = 0.0f;
    params.bounds_min_x = -1000.0f;
    params.bounds_max_x = 1000.0f;
    params.bounds_min_y = -1000.0f;
    params.bounds_max_y = 1000.0f;

    auto traj = computeTrajectory(200.0f, 200.0f, 101, params);

    // Frame 0 and frame 100 should both be at start (sin(0) = sin(2pi) = 0)
    REQUIRE_THAT(traj[0].x, WithinAbs(200.0f, 1e-3));
    REQUIRE_THAT(traj[100].x, WithinAbs(200.0f, 1e-3));
    REQUIRE_THAT(traj[0].y, WithinAbs(200.0f, 1e-3));
    REQUIRE_THAT(traj[100].y, WithinAbs(200.0f, 1e-3));
}

TEST_CASE("Sinusoidal trajectory reaches expected peak", "[Trajectory][Sinusoidal]") {
    TrajectoryParams params;
    params.model = "sinusoidal";
    params.amplitude_x = 50.0f;
    params.amplitude_y = 0.0f;
    params.frequency_x = 1.0f / 100.0f;// period = 100 frames
    params.frequency_y = 0.0f;
    params.bounds_min_x = -1000.0f;
    params.bounds_max_x = 1000.0f;
    params.bounds_min_y = -1000.0f;
    params.bounds_max_y = 1000.0f;

    auto traj = computeTrajectory(100.0f, 100.0f, 101, params);

    // Peak at frame 25 (quarter period): sin(pi/2) = 1
    REQUIRE_THAT(traj[25].x, WithinAbs(150.0f, 0.5f));
}

TEST_CASE("Sinusoidal trajectory with zero amplitude is stationary", "[Trajectory][Sinusoidal]") {
    TrajectoryParams params;
    params.model = "sinusoidal";
    params.amplitude_x = 0.0f;
    params.amplitude_y = 0.0f;
    params.frequency_x = 1.0f;
    params.frequency_y = 1.0f;

    auto traj = computeTrajectory(50.0f, 75.0f, 10, params);

    for (auto const & p: traj) {
        REQUIRE_THAT(p.x, WithinAbs(50.0f, 1e-5));
        REQUIRE_THAT(p.y, WithinAbs(75.0f, 1e-5));
    }
}

// =====================================================================
// Brownian motion model
// =====================================================================

TEST_CASE("Brownian trajectory is deterministic with seed", "[Trajectory][Brownian]") {
    TrajectoryParams params;
    params.model = "brownian";
    params.diffusion = 2.0f;
    params.seed = 12345;
    params.bounds_min_x = -10000.0f;
    params.bounds_max_x = 10000.0f;
    params.bounds_min_y = -10000.0f;
    params.bounds_max_y = 10000.0f;

    auto traj1 = computeTrajectory(0.0f, 0.0f, 50, params);
    auto traj2 = computeTrajectory(0.0f, 0.0f, 50, params);

    REQUIRE(traj1.size() == traj2.size());
    for (size_t i = 0; i < traj1.size(); ++i) {
        REQUIRE_THAT(traj1[i].x, WithinAbs(traj2[i].x, 1e-10));
        REQUIRE_THAT(traj1[i].y, WithinAbs(traj2[i].y, 1e-10));
    }
}

TEST_CASE("Brownian trajectory differs with different seeds", "[Trajectory][Brownian]") {
    TrajectoryParams params;
    params.model = "brownian";
    params.diffusion = 5.0f;
    params.bounds_min_x = -10000.0f;
    params.bounds_max_x = 10000.0f;
    params.bounds_min_y = -10000.0f;
    params.bounds_max_y = 10000.0f;

    params.seed = 1;
    auto traj1 = computeTrajectory(0.0f, 0.0f, 20, params);

    params.seed = 2;
    auto traj2 = computeTrajectory(0.0f, 0.0f, 20, params);

    // At least some positions should differ
    bool any_differ = false;
    for (size_t i = 1; i < traj1.size(); ++i) {
        if (std::abs(traj1[i].x - traj2[i].x) > 1e-5 ||
            std::abs(traj1[i].y - traj2[i].y) > 1e-5) {
            any_differ = true;
            break;
        }
    }
    REQUIRE(any_differ);
}

TEST_CASE("Brownian trajectory does not move from start at frame 0", "[Trajectory][Brownian]") {
    TrajectoryParams params;
    params.model = "brownian";
    params.diffusion = 10.0f;
    params.seed = 99;

    auto traj = computeTrajectory(42.0f, 99.0f, 5, params);
    REQUIRE_THAT(traj[0].x, WithinAbs(42.0f, 1e-5));
    REQUIRE_THAT(traj[0].y, WithinAbs(99.0f, 1e-5));
}

// =====================================================================
// Boundary mode: clamp
// =====================================================================

TEST_CASE("Clamp boundary stops at bounds", "[Trajectory][Boundary][Clamp]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 10.0f;
    params.velocity_y = 0.0f;
    params.boundary_mode = "clamp";
    params.bounds_min_x = 0.0f;
    params.bounds_max_x = 25.0f;

    auto traj = computeTrajectory(0.0f, 0.0f, 10, params);

    // Should clamp at 25
    for (auto const & p: traj) {
        REQUIRE(p.x >= 0.0f);
        REQUIRE(p.x <= 25.0f);
    }

    // After frame 2 (x = 20), frame 3 would be 30 => clamped to 25.
    // All subsequent frames should stay at 25 (since velocity doesn't change,
    // it keeps trying to go past, and keeps getting clamped).
    REQUIRE_THAT(traj[3].x, WithinAbs(25.0f, 1e-5));
    REQUIRE_THAT(traj[9].x, WithinAbs(25.0f, 1e-5));
}

// =====================================================================
// Boundary mode: bounce
// =====================================================================

TEST_CASE("Bounce boundary reflects at bounds", "[Trajectory][Boundary][Bounce]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 10.0f;
    params.velocity_y = 0.0f;
    params.boundary_mode = "bounce";
    params.bounds_min_x = 0.0f;
    params.bounds_max_x = 25.0f;

    auto traj = computeTrajectory(0.0f, 0.0f, 10, params);

    // All positions should be within bounds
    for (auto const & p: traj) {
        REQUIRE(p.x >= -0.01f);
        REQUIRE(p.x <= 25.01f);
    }

    // Frame 0: 0, Frame 1: 10, Frame 2: 20
    // Frame 3: 30 → bounce to 20 (velocity flips to -10)
    // Frame 4: 20 - 10 = 10
    REQUIRE_THAT(traj[0].x, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(traj[1].x, WithinAbs(10.0f, 1e-5));
    REQUIRE_THAT(traj[2].x, WithinAbs(20.0f, 1e-5));
    REQUIRE_THAT(traj[3].x, WithinAbs(20.0f, 1e-5));// bounced from 30 → 25 - 5 = 20
    REQUIRE_THAT(traj[4].x, WithinAbs(10.0f, 1e-5));// velocity now -10
}

// =====================================================================
// Boundary mode: wrap
// =====================================================================

TEST_CASE("Wrap boundary wraps around", "[Trajectory][Boundary][Wrap]") {
    TrajectoryParams params;
    params.model = "linear";
    params.velocity_x = 10.0f;
    params.velocity_y = 0.0f;
    params.boundary_mode = "wrap";
    params.bounds_min_x = 0.0f;
    params.bounds_max_x = 25.0f;

    auto traj = computeTrajectory(0.0f, 0.0f, 6, params);

    // All positions should be within bounds
    for (auto const & p: traj) {
        REQUIRE(p.x >= -0.01f);
        REQUIRE(p.x <= 25.01f);
    }

    // Frame 0: 0, Frame 1: 10, Frame 2: 20
    // Frame 3: 30 → wraps to 30 - 25 = 5
    REQUIRE_THAT(traj[0].x, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(traj[1].x, WithinAbs(10.0f, 1e-5));
    REQUIRE_THAT(traj[2].x, WithinAbs(20.0f, 1e-5));
    REQUIRE_THAT(traj[3].x, WithinAbs(5.0f, 1e-5));
}

// =====================================================================
// Error handling
// =====================================================================

TEST_CASE("computeTrajectory rejects invalid num_frames", "[Trajectory]") {
    TrajectoryParams params;
    REQUIRE_THROWS_AS(computeTrajectory(0.0f, 0.0f, 0, params), std::invalid_argument);
    REQUIRE_THROWS_AS(computeTrajectory(0.0f, 0.0f, -1, params), std::invalid_argument);
}

TEST_CASE("computeTrajectory rejects invalid bounds", "[Trajectory]") {
    TrajectoryParams params;
    params.bounds_min_x = 100.0f;
    params.bounds_max_x = 50.0f;// max < min
    REQUIRE_THROWS_AS(computeTrajectory(0.0f, 0.0f, 10, params), std::invalid_argument);
}

TEST_CASE("computeTrajectory rejects unknown model", "[Trajectory]") {
    TrajectoryParams params;
    params.model = "teleportation";
    REQUIRE_THROWS_AS(computeTrajectory(0.0f, 0.0f, 10, params), std::invalid_argument);
}
