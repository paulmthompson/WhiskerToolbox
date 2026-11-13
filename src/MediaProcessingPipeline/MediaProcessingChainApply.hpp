/**
 * @file MediaProcessingChainApply.hpp
 * @brief Apply registry-driven image processing steps to MediaData (Qt-free).
 */

#ifndef MEDIA_PROCESSING_PIPELINE_CHAIN_APPLY_HPP
#define MEDIA_PROCESSING_PIPELINE_CHAIN_APPLY_HPP

#include "ImageProcessing/ProcessingOptions.hpp"

#include <string>

class MediaData;

namespace Neuralyzer::MediaProcessing {

/**
 * @brief Serializable processing-chain settings for a single media key.
 *
 * Mirrors the processing-related fields of MediaDisplayOptions without a
 * dependency on Media_Widget.
 */
struct MediaImageProcessingOptions {
    bool contrast_active = false;
    bool gamma_active = false;
    bool sharpen_active = false;
    bool clahe_active = false;
    bool bilateral_active = false;
    bool median_active = false;
    bool magic_eraser_active = false;

    ContrastOptions contrast_options{};
    GammaOptions gamma_options{};
    SharpenOptions sharpen_options{};
    ClaheOptions clahe_options{};
    BilateralOptions bilateral_options{};
    MedianOptions median_options{};
    MagicEraserParams magic_eraser_params{};
    MagicEraserState magic_eraser_state{};
};

/**
 * @brief Rebuild OpenCV processing steps on @p target from @p options.
 *
 * Clears existing steps on @p target, ensures an OpenCV processor backend,
 * then registers active registry steps and optional magic eraser.
 *
 * @pre target must be loaded (valid image size) before calling magic eraser.
 */
void applyMediaProcessingChain(MediaData & target,
                               MediaImageProcessingOptions const & options);

/**
 * @brief Read active flag for a registry chain key from options.
 */
[[nodiscard]] bool isChainStepActive(MediaImageProcessingOptions const & options,
                                     std::string const & chain_key);

/**
 * @brief Serialize options struct for a registry chain key to JSON.
 */
[[nodiscard]] std::string chainStepOptionsJson(MediaImageProcessingOptions const & options,
                                               std::string const & chain_key);

}// namespace Neuralyzer::MediaProcessing

#endif// MEDIA_PROCESSING_PIPELINE_CHAIN_APPLY_HPP
