#ifndef MEDIA_INFERENCE_OVERRIDES_HPP
#define MEDIA_INFERENCE_OVERRIDES_HPP

/**
 * @file MediaInferenceOverrides.hpp
 * @brief Build thread-safe MediaData clones for batch deep learning inference.
 */

#include "DeepLearning_Widget/Core/MediaOverrides.hpp"
#include "DeepLearning_Widget/Core/MediaProcessingOptionsLookup.hpp"

#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"

#include <memory>
#include <string>
#include <vector>

class DataManager;

/**
 * @brief Build per-key MediaData clones for batch inference workers.
 *
 * Clones every image-encoder binding and static memory data key. Applies the
 * media processing chain when the binding requests it and options are available.
 *
 * @param warnings Optional human-readable warnings (e.g. missing display options).
 */
[[nodiscard]] MediaOverrides buildMediaInferenceOverrides(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<dl::MemoryFrameBinding> const & memory_frames,
        MediaProcessingOptionsLookup const & options_lookup,
        std::vector<std::string> * warnings = nullptr);

#endif// MEDIA_INFERENCE_OVERRIDES_HPP
