#ifndef MEDIA_PROCESSING_OPTIONS_LOOKUP_HPP
#define MEDIA_PROCESSING_OPTIONS_LOOKUP_HPP

/**
 * @file MediaProcessingOptionsLookup.hpp
 * @brief Callback type for resolving Media processing options during batch inference.
 */

#include "MediaProcessingPipeline/MediaProcessingChainApply.hpp"

#include <functional>
#include <optional>
#include <string>

/**
 * @brief Lookup Media processing options for a DataManager media key.
 *
 * @return Options when configured (e.g. from MediaWidgetState), or empty.
 */
using MediaProcessingOptionsLookup =
        std::function<std::optional<Neuralyzer::MediaProcessing::MediaImageProcessingOptions>(
                std::string const & data_key)>;

#endif// MEDIA_PROCESSING_OPTIONS_LOOKUP_HPP
