/**
 * @file MovingMaskGenerator.cpp
 * @brief MaskData generator that produces a shape moving along a trajectory.
 *
 * Registers a "MovingMask" generator in the DataSynthesizer registry.
 * Produces a MaskData where each frame contains a shape (circle, rectangle,
 * or ellipse) at a position determined by the shared Trajectory library.
 * Pixels are clipped to image bounds at each frame.
 *
 * Uses MotionModelVariant (rfl::TaggedUnion) and BoundaryMode (enum class)
 * for typed, combo-box-driven parameter UI via AutoParamWidget.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"
#include "DataSynthesizer/Trajectory/MotionParams.hpp"
#include "DataSynthesizer/Trajectory/PixelClipping.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

namespace WhiskerToolbox::DataSynthesizer {

/// Shape type for the moving mask.
enum class MaskShape { circle,
                       rectangle,
                       ellipse };

}// namespace WhiskerToolbox::DataSynthesizer

namespace {

using WhiskerToolbox::DataSynthesizer::BoundaryMode;
using WhiskerToolbox::DataSynthesizer::BoundaryParams;
using WhiskerToolbox::DataSynthesizer::LinearMotionParams;
using WhiskerToolbox::DataSynthesizer::MaskShape;
using WhiskerToolbox::DataSynthesizer::MotionModelVariant;

struct MovingMaskParams {
    MaskShape shape = MaskShape::circle;

    int image_width = 640;
    int image_height = 480;

    float start_x = 320.0f;
    float start_y = 240.0f;

    /// Circle radius (used when shape == circle).
    float radius = 50.0f;
    /// Rectangle/ellipse width (used when shape == rectangle).
    float width = 100.0f;
    /// Rectangle/ellipse height (used when shape == rectangle).
    float height = 60.0f;
    /// Ellipse semi-major axis (used when shape == ellipse).
    float semi_major = 80.0f;
    /// Ellipse semi-minor axis (used when shape == ellipse).
    float semi_minor = 40.0f;
    /// Ellipse rotation angle in degrees (used when shape == ellipse).
    float angle = 0.0f;

    int num_frames = 100;

    MotionModelVariant motion = LinearMotionParams{};
    BoundaryMode boundary_mode = BoundaryMode::clamp;
    float bounds_min_x = 0.0f;
    float bounds_max_x = 640.0f;
    float bounds_min_y = 0.0f;
    float bounds_max_y = 480.0f;
};

/// Rasterize shape pixels centered at the given position
std::vector<Point2D<uint32_t>> rasterizeShape(
        MovingMaskParams const & params,
        float center_x, float center_y) {
    switch (params.shape) {
        case MaskShape::circle:
            return generate_ellipse_pixels(center_x, center_y, params.radius, params.radius);
        case MaskShape::rectangle:
            return generate_rectangle_pixels(center_x, center_y, params.width, params.height);
        case MaskShape::ellipse: {
            float const angle_rad = params.angle * std::numbers::pi_v<float> / 180.0f;
            if (std::abs(angle_rad) < 1e-6f) {
                return generate_ellipse_pixels(center_x, center_y, params.semi_major, params.semi_minor);
            }
            return generate_rotated_ellipse_pixels(
                    center_x, center_y, params.semi_major, params.semi_minor, angle_rad);
        }
    }
    return {};
}

DataTypeVariant generateMovingMask(MovingMaskParams const & params) {
    if (params.image_width <= 0) {
        throw std::invalid_argument("MovingMask: image_width must be > 0");
    }
    if (params.image_height <= 0) {
        throw std::invalid_argument("MovingMask: image_height must be > 0");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("MovingMask: num_frames must be > 0");
    }

    // Shape-specific validation
    switch (params.shape) {
        case MaskShape::circle:
            if (params.radius <= 0.0f) {
                throw std::invalid_argument("MovingMask: radius must be > 0 for circle shape");
            }
            break;
        case MaskShape::rectangle:
            if (params.width <= 0.0f) {
                throw std::invalid_argument("MovingMask: width must be > 0 for rectangle shape");
            }
            if (params.height <= 0.0f) {
                throw std::invalid_argument("MovingMask: height must be > 0 for rectangle shape");
            }
            break;
        case MaskShape::ellipse:
            if (params.semi_major <= 0.0f) {
                throw std::invalid_argument("MovingMask: semi_major must be > 0 for ellipse shape");
            }
            if (params.semi_minor <= 0.0f) {
                throw std::invalid_argument("MovingMask: semi_minor must be > 0 for ellipse shape");
            }
            break;
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

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{params.image_width, params.image_height});

    for (int i = 0; i < params.num_frames; ++i) {
        auto const & pos = trajectory[static_cast<size_t>(i)];
        auto pixels = rasterizeShape(params, pos.x, pos.y);
        pixels = clipPixelsToImage(std::move(pixels), params.image_width, params.image_height);
        Mask2D const mask(std::move(pixels));
        mask_data->addAtTime(TimeFrameIndex(i), mask, NotifyObservers::No);
    }

    return mask_data;
}

auto const moving_mask_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<MovingMaskParams>(
                "MovingMask",
                generateMovingMask,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a shape (circle, rectangle, or ellipse) that moves "
                                       "along a trajectory. Supports linear, sinusoidal, and Brownian "
                                       "motion models with clamp, bounce, or wrap boundary modes. "
                                       "Pixels are clipped to image bounds at each frame. "
                                       "Deterministic: same seed produces the same output for Brownian motion.",
                        .category = "Spatial",
                        .output_type = "MaskData"});

}// namespace
