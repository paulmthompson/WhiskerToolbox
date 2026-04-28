/**
 * @file MotionParams.hpp
 * @brief Shared motion model and boundary parameter types for moving generators.
 *
 * Defines BoundaryMode enum, BoundaryParams, per-model parameter sub-structs,
 * and MotionModelVariant (rfl::TaggedUnion). These types are reused by all
 * moving generators (MovingPoint, MovingMask, MovingLine) and integrate with
 * ParameterSchema for automatic combo-box-driven UI in AutoParamWidget.
 *
 * The internal TrajectoryParams struct (Trajectory.hpp) remains unchanged —
 * generators convert from the typed variant representation using
 * toTrajectoryParams().
 */
#ifndef WHISKERTOOLBOX_DATASYNTHESIZER_MOTION_PARAMS_HPP
#define WHISKERTOOLBOX_DATASYNTHESIZER_MOTION_PARAMS_HPP

#include "Trajectory.hpp"

#include <cstdint>
#include <rfl.hpp>
#include <string>

namespace WhiskerToolbox::DataSynthesizer {

// ============================================================================
// Boundary
// ============================================================================

/**
 * @brief Boundary enforcement mode for trajectory computation.
 */
enum class BoundaryMode { clamp,
                          bounce,
                          wrap };

/**
 * @brief Boundary parameters shared by all moving generators.
 */
struct BoundaryParams {
    BoundaryMode boundary_mode = BoundaryMode::clamp;
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

// ============================================================================
// Per-Model Parameter Sub-Structs
// ============================================================================

/**
 * @brief Parameters for linear motion
 *  
 * position(t) = start + velocity * t
 * 
 */
struct LinearMotionParams {
    float velocity_x = 1.0f;
    float velocity_y = 0.0f;
};

/**
 * @brief Parameters for sinusoidal motion
 *  
 * position(t) = start + amplitude * sin(2pi * freq * t + phase)
 */
struct SinusoidalMotionParams {
    float amplitude_x = 0.0f;
    float amplitude_y = 0.0f;
    float frequency_x = 0.0f;
    float frequency_y = 0.0f;
    float phase_x = 0.0f;
    float phase_y = 0.0f;
};

/**
 * @brief Parameters for Brownian motion
 *  
 * position(t) = position(t-1) + N(0, diffusion)
 */
struct BrownianMotionParams {
    float diffusion = 1.0f;
    uint64_t seed = 42;
};

// ============================================================================
// Tagged Union Variant
// ============================================================================

/// Discriminated union of motion model parameters.
/// The discriminator field is "model" in the serialized JSON.
using MotionModelVariant = rfl::TaggedUnion<
        "model",
        LinearMotionParams,
        SinusoidalMotionParams,
        BrownianMotionParams>;

// ============================================================================
// Conversion to internal TrajectoryParams
// ============================================================================

/**
 * @brief Convert enum BoundaryMode to the string expected by TrajectoryParams.
 */
inline std::string boundaryModeToString(BoundaryMode mode) {
    switch (mode) {
        case BoundaryMode::clamp:
            return "clamp";
        case BoundaryMode::bounce:
            return "bounce";
        case BoundaryMode::wrap:
            return "wrap";
    }
    return "clamp";
}

/**
 * @brief Convert typed motion variant + boundary params to the internal TrajectoryParams.
 *
 * This bridges the user-facing typed parameters (enum + tagged union) to the
 * internal flat TrajectoryParams used by computeTrajectory().
 */
inline TrajectoryParams toTrajectoryParams(
        MotionModelVariant const & motion,
        BoundaryParams const & boundary) {
    TrajectoryParams tp;
    tp.boundary_mode = boundaryModeToString(boundary.boundary_mode);
    tp.bounds_min_x = boundary.bounds_min_x;
    tp.bounds_max_x = boundary.bounds_max_x;
    tp.bounds_min_y = boundary.bounds_min_y;
    tp.bounds_max_y = boundary.bounds_max_y;

    motion.visit(
            [&tp](auto const & model_params) {
                using T = std::decay_t<decltype(model_params)>;
                if constexpr (std::is_same_v<T, LinearMotionParams>) {
                    tp.model = "linear";
                    tp.velocity_x = model_params.velocity_x;
                    tp.velocity_y = model_params.velocity_y;
                } else if constexpr (std::is_same_v<T, SinusoidalMotionParams>) {
                    tp.model = "sinusoidal";
                    tp.amplitude_x = model_params.amplitude_x;
                    tp.amplitude_y = model_params.amplitude_y;
                    tp.frequency_x = model_params.frequency_x;
                    tp.frequency_y = model_params.frequency_y;
                    tp.phase_x = model_params.phase_x;
                    tp.phase_y = model_params.phase_y;
                } else if constexpr (std::is_same_v<T, BrownianMotionParams>) {
                    tp.model = "brownian";
                    tp.diffusion = model_params.diffusion;
                    tp.seed = model_params.seed;
                }
            });

    return tp;
}

}// namespace WhiskerToolbox::DataSynthesizer

#endif// WHISKERTOOLBOX_DATASYNTHESIZER_MOTION_PARAMS_HPP
