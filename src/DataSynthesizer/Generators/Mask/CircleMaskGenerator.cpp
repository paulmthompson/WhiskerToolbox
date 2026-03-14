/**
 * @file CircleMaskGenerator.cpp
 * @brief MaskData generator that produces a circular mask repeated for each frame.
 *
 * Registers a "CircleMask" generator in the DataSynthesizer registry.
 * Produces a MaskData where each frame contains a circle defined by center and radius,
 * rasterized to pixel coordinates using generate_ellipse_pixels().
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/Registration.hpp"

#include "DataManager/DataManagerTypes.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>

namespace {

struct CircleMaskParams {
    int image_width = 640;
    int image_height = 480;
    float center_x = 320.0f;
    float center_y = 240.0f;
    float radius = 50.0f;
    int num_frames = 100;
};

DataTypeVariant generateCircleMask(CircleMaskParams const & params) {
    if (params.image_width <= 0) {
        throw std::invalid_argument("CircleMask: image_width must be > 0");
    }
    if (params.image_height <= 0) {
        throw std::invalid_argument("CircleMask: image_height must be > 0");
    }
    if (params.radius <= 0.0f) {
        throw std::invalid_argument("CircleMask: radius must be > 0");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("CircleMask: num_frames must be > 0");
    }

    auto pixels = generate_ellipse_pixels(
            params.center_x, params.center_y, params.radius, params.radius);
    pixels = clipPixelsToImage(std::move(pixels), params.image_width, params.image_height);
    Mask2D const mask(std::move(pixels));

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{params.image_width, params.image_height});

    for (int i = 0; i < params.num_frames; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), mask, NotifyObservers::No);
    }

    return mask_data;
}

auto const circle_mask_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<CircleMaskParams>(
                "CircleMask",
                generateCircleMask,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a circular mask repeated at every frame. "
                                       "The circle is defined by center position and radius, "
                                       "rasterized to pixel coordinates.",
                        .category = "Spatial",
                        .output_type = "MaskData"});

}// namespace
