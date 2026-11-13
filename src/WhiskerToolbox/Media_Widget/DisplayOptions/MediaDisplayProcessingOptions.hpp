/**
 * @file MediaDisplayProcessingOptions.hpp
 * @brief Convert MediaDisplayOptions to MediaProcessingPipeline chain settings.
 */

#ifndef MEDIA_DISPLAY_PROCESSING_OPTIONS_HPP
#define MEDIA_DISPLAY_PROCESSING_OPTIONS_HPP

#include "DisplayOptions/DisplayOptions.hpp"
#include "MediaProcessingPipeline/MediaProcessingChainApply.hpp"

/**
 * @brief Copy processing-chain fields from media display options.
 */
[[nodiscard]] inline Neuralyzer::MediaProcessing::MediaImageProcessingOptions
mediaImageProcessingOptionsFromDisplay(MediaDisplayOptions const & display) {
    Neuralyzer::MediaProcessing::MediaImageProcessingOptions out;
    out.contrast_active = display.contrast_active;
    out.gamma_active = display.gamma_active;
    out.sharpen_active = display.sharpen_active;
    out.clahe_active = display.clahe_active;
    out.bilateral_active = display.bilateral_active;
    out.median_active = display.median_active;
    out.magic_eraser_active = display.magic_eraser_active;
    out.contrast_options = display.contrast_options;
    out.gamma_options = display.gamma_options;
    out.sharpen_options = display.sharpen_options;
    out.clahe_options = display.clahe_options;
    out.bilateral_options = display.bilateral_options;
    out.median_options = display.median_options;
    out.magic_eraser_params = display.magic_eraser_params;
    out.magic_eraser_state = display.magic_eraser_state;
    return out;
}

#endif// MEDIA_DISPLAY_PROCESSING_OPTIONS_HPP
