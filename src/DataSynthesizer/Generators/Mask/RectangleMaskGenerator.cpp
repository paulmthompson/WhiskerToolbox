/**
 * @file RectangleMaskGenerator.cpp
 * @brief MaskData generator that produces a rectangular mask repeated for each frame.
 *
 * Registers a "RectangleMask" generator in the DataSynthesizer registry.
 * Produces a MaskData where each frame contains a filled rectangle defined by
 * center position and dimensions, rasterized to pixel coordinates.
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

struct RectangleMaskParams {
    int image_width = 640;
    int image_height = 480;
    float center_x = 320.0f;
    float center_y = 240.0f;
    float width = 100.0f;
    float height = 60.0f;
    int num_frames = 100;
};

DataTypeVariant generateRectangleMask(RectangleMaskParams const & params) {
    if (params.image_width <= 0) {
        throw std::invalid_argument("RectangleMask: image_width must be > 0");
    }
    if (params.image_height <= 0) {
        throw std::invalid_argument("RectangleMask: image_height must be > 0");
    }
    if (params.width <= 0.0f) {
        throw std::invalid_argument("RectangleMask: width must be > 0");
    }
    if (params.height <= 0.0f) {
        throw std::invalid_argument("RectangleMask: height must be > 0");
    }
    if (params.num_frames <= 0) {
        throw std::invalid_argument("RectangleMask: num_frames must be > 0");
    }

    auto pixels = generate_rectangle_pixels(
            params.center_x, params.center_y, params.width, params.height);
    pixels = clipPixelsToImage(std::move(pixels), params.image_width, params.image_height);
    Mask2D const mask(std::move(pixels));

    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{params.image_width, params.image_height});

    for (int i = 0; i < params.num_frames; ++i) {
        mask_data->addAtTime(TimeFrameIndex(i), mask, NotifyObservers::No);
    }

    return mask_data;
}

auto const rectangle_mask_reg =
        WhiskerToolbox::DataSynthesizer::RegisterGenerator<RectangleMaskParams>(
                "RectangleMask",
                generateRectangleMask,
                WhiskerToolbox::DataSynthesizer::GeneratorMetadata{
                        .description = "Generates a rectangular mask repeated at every frame. "
                                       "The rectangle is defined by center position and dimensions, "
                                       "rasterized to pixel coordinates.",
                        .category = "Spatial",
                        .output_type = "MaskData"});

}// namespace
