/**
 * @file MediaInferenceOverrides.cpp
 * @brief Implementation of batch inference MediaOverrides construction.
 */

#include "MediaInferenceOverrides.hpp"

#include "CloneMediaForInference.hpp"

#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning/channel_encoding/EncoderDispatch.hpp"

#include "DataManager/DataManager.hpp"
#include "Media/Media_Data.hpp"

#include <spdlog/spdlog.h>

#include <set>
#include <string>
#include <type_traits>

namespace {

void collectMediaKeys(
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<dl::MemoryFrameBinding> const & memory_frames,
        std::set<std::string> & keys) {

    for (auto const & binding: input_bindings) {
        if (!dl::isImageEncoder(binding.encoder) || binding.data_key.empty()) {
            continue;
        }
        keys.insert(binding.data_key);
    }

    for (auto const & frame: memory_frames) {
        if (!dl::isStaticFrame(frame)) {
            continue;
        }
        auto const data_key = dl::staticDataKey(frame);
        if (!data_key.empty()) {
            keys.insert(data_key);
        }
    }
}

[[nodiscard]] bool bindingWantsProcessingChain(
        std::vector<SlotBindingData> const & input_bindings,
        std::string const & data_key) {
    for (auto const & binding: input_bindings) {
        if (binding.data_key != data_key || !dl::isImageEncoder(binding.encoder)) {
            continue;
        }
        return binding.encoder.visit([](auto const & params) {
            using ParamsT = std::decay_t<decltype(params)>;
            if constexpr (std::is_same_v<ParamsT, dl::ImageEncoderParams>) {
                return params.use_media_processing_chain;
            }
            return true;
        });
    }
    return true;
}

void applyChainIfNeeded(
        MediaData & clone,
        MediaData const & canonical,
        std::string const & data_key,
        bool wants_chain,
        MediaProcessingOptionsLookup const & options_lookup,
        std::vector<std::string> * warnings) {

    if (!wants_chain) {
        return;
    }

    if (!options_lookup) {
        if (canonical.getProcessingStepCount() > 0 && warnings != nullptr) {
            warnings->push_back(
                    "Media processing chain enabled for '" + data_key +
                    "' but no Media display options are available; using raw pixels on "
                    "inference clone.");
        }
        return;
    }

    auto const options = options_lookup(data_key);
    if (!options) {
        if (canonical.getProcessingStepCount() > 0 && warnings != nullptr) {
            warnings->push_back(
                    "No Media display options for '" + data_key +
                    "'; inference clone may not match preview processing.");
        }
        return;
    }

    Neuralyzer::MediaProcessing::applyMediaProcessingChain(clone, *options);
}

}// namespace

MediaOverrides buildMediaInferenceOverrides(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<dl::MemoryFrameBinding> const & memory_frames,
        MediaProcessingOptionsLookup const & options_lookup,
        std::vector<std::string> * warnings) {

    MediaOverrides overrides;
    std::set<std::string> keys;
    collectMediaKeys(input_bindings, memory_frames, keys);

    for (auto const & data_key: keys) {
        auto canonical = dm.getData<MediaData>(data_key);
        if (!canonical) {
            continue;
        }
        if (overrides.contains(data_key)) {
            continue;
        }

        auto clone = cloneMediaForInference(*canonical);
        if (!clone) {
            spdlog::error(
                    "buildMediaInferenceOverrides: failed to clone media '{}'",
                    data_key);
            continue;
        }

        applyChainIfNeeded(
                *clone,
                *canonical,
                data_key,
                bindingWantsProcessingChain(input_bindings, data_key),
                options_lookup,
                warnings);

        overrides[data_key] = std::move(clone);
    }

    return overrides;
}
