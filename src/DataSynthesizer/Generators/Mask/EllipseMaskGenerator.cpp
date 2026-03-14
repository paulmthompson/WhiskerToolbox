/**
 * @file EllipseMaskGenerator.cpp
 * @brief MaskData generator that produces an elliptical mask repeated for each frame.
 *
 * Registers an "EllipseMask" generator in the DataSynthesizer registry.
 * Produces a MaskData where each frame contains an ellipse defined by center,
 * semi-major/minor axes, and rotation angle, rasterized to pixel coordinates.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace {

struct EllipseMaskParams {
    int image_width = 640;
    int image_height = 480;
    float center_x = 320.0f;
    float center_y = 240.0f;
    float semi_major = 80.0f;
    float semi_minor = 40.0f;
    std::optional<float> angle;
    int num_frames = 100;
};

DataTypeVariant generateEllipseMask(EllipseMaskParams const & params) {
    if (params.image_width <= 0) {
        throw std::invalid_argument("EllipseMask: image_width must be > 0");
    }
    if (params.image_height <= 0) {
        throw std::invalid_argument("EllipseMask: image_height must be > 0");
    }
    if (params.semi_major <= 0.0f) {
        throw std::invalid_argument("EllipseMask: semi_major must be > 0");
    }
    if (params.semi_minor <= 0.0f) {
        throw std::invalid_argument("EllipseMask: semi_minor must be > 0");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("EllipseMask: num_frames must be > 0");
    }

    float const angle_rad = params.angle.value_or(0.0f) * std::numbers::pi_v<float> / 180.0f;

    std::vector<Point2D<uint32_t>> pixels;

    if (std::abs(angle_rad) < 1e-6f) {
        pixels = generate_ellipse_pixels(
                params.center_x, params.center_y, params.semi_major, params.semi_minor);
    } else {
        pixels = generate_rotated_ellipse_pixels(
                params.center_x, params.center_y,
                params.semi_major, params.semi_minor, angle_rad);
    }

    pixels = clipPixelsToImage(std::move(pixels), params.image_width, params.image_height);
    Mask2D const mask(std::move(pixels));

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{params.image_width, params.image_height});

    for (int i = 0; i < params.num_frames; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), mask, NotifyObservers::No);
    }

    return mask_data;
}

auto const ellipse_mask_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<EllipseMaskParams>(
                "EllipseMask",
                generateEllipseMask,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates an elliptical mask repeated at every frame. "
                                       "The ellipse is defined by center, semi-major/minor axes, "
                                       "and an optional rotation angle (degrees). "
                                       "Rasterized to pixel coordinates.",
                        .category = "Spatial",
                        .output_type = "MaskData"});

}// namespace
