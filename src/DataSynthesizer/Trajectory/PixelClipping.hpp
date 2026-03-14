/**
 * @file PixelClipping.hpp
 * @brief Convenience re-export of clipPixelsToImage() for DataSynthesizer generators.
 *
 * Moving mask generators (Milestone 5b-ii) need to clip rasterized pixels to image
 * bounds at every frame. The implementation lives in CoreGeometry/masks.hpp;
 * this header provides a convenient include path within the DataSynthesizer tree.
 */
#ifndef WHISKERTOOLBOX_DATASYNTHESIZER_PIXEL_CLIPPING_HPP
#define WHISKERTOOLBOX_DATASYNTHESIZER_PIXEL_CLIPPING_HPP

#include "CoreGeometry/masks.hpp"// clipPixelsToImage, generate_ellipse_pixels, etc.

#endif// WHISKERTOOLBOX_DATASYNTHESIZER_PIXEL_CLIPPING_HPP
