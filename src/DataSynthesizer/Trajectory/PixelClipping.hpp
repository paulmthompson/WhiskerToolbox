/**
 * @file PixelClipping.hpp
 * @brief Convenience re-export of clipPixelsToImage() for DataSynthesizer generators.
 *
 * Moving mask generators need to clip rasterized pixels to image
 * bounds at every frame. The implementation lives in CoreGeometry/masks.hpp;
 * this header provides a convenient include path within the DataSynthesizer tree.
 */
#ifndef NEURALYZER_DATASYNTHESIZER_PIXEL_CLIPPING_HPP
#define NEURALYZER_DATASYNTHESIZER_PIXEL_CLIPPING_HPP

#include "CoreGeometry/masks.hpp"// clipPixelsToImage, generate_ellipse_pixels, etc.

#endif// NEURALYZER_DATASYNTHESIZER_PIXEL_CLIPPING_HPP
