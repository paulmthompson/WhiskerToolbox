/**
 * @file MediaProcessingChainApply.cpp
 * @brief Implementation of MediaData processing chain application.
 */

#include "MediaProcessingChainApply.hpp"

#include "ProcessingStepRegistry.hpp"

#include "ImageProcessing/OpenCVUtility.hpp"
#include "Media/Media_Data.hpp"

#include <nlohmann/json.hpp>
#include <opencv2/core/mat.hpp>

#include <rfl/json.hpp>

namespace Neuralyzer::MediaProcessing {

namespace {

bool getActiveFlag(MediaImageProcessingOptions const & options,
                   std::string const & chain_key) {
    if (chain_key == "1__lineartransform") {
        return options.contrast_active;
    }
    if (chain_key == "2__gamma") {
        return options.gamma_active;
    }
    if (chain_key == "3__sharpen") {
        return options.sharpen_active;
    }
    if (chain_key == "4__clahe") {
        return options.clahe_active;
    }
    if (chain_key == "5__bilateral") {
        return options.bilateral_active;
    }
    if (chain_key == "6__median") {
        return options.median_active;
    }
    return false;
}

std::string getOptionsJson(MediaImageProcessingOptions const & options,
                           std::string const & chain_key) {
    if (chain_key == "1__lineartransform") {
        return rfl::json::write(options.contrast_options);
    }
    if (chain_key == "2__gamma") {
        return rfl::json::write(options.gamma_options);
    }
    if (chain_key == "3__sharpen") {
        return rfl::json::write(options.sharpen_options);
    }
    if (chain_key == "4__clahe") {
        return rfl::json::write(options.clahe_options);
    }
    if (chain_key == "5__bilateral") {
        return rfl::json::write(options.bilateral_options);
    }
    if (chain_key == "6__median") {
        return rfl::json::write(options.median_options);
    }
    return "{}";
}

void applyRegistryStep(MediaData & media_data,
                       std::string const & chain_key,
                       bool active,
                       std::string const & params_json) {
    if (!active) {
        media_data.removeProcessingStep(chain_key);
        return;
    }

    auto const * step = ProcessingStepRegistry::instance().findByKey(chain_key);
    if (!step) {
        return;
    }

    auto parsed = nlohmann::json::parse(params_json, nullptr, false);
    if (parsed.is_discarded()) {
        return;
    }

    media_data.addProcessingStep(chain_key, [apply = step->apply, parsed](void * input) {
        auto * mat = static_cast<cv::Mat *>(input);
        apply(*mat, parsed);
    });
}

void applyMagicEraser(MediaData & media_data,
                      MediaImageProcessingOptions const & options) {
    auto combined = MagicEraserOptions::fromParamsAndState(
            options.magic_eraser_params, options.magic_eraser_state);

    if (options.magic_eraser_active && !combined.mask.empty()) {
        media_data.addProcessingStep("7__magic_eraser", [combined](void * input) {
            auto * mat = static_cast<cv::Mat *>(input);
            ImageProcessing::apply_magic_eraser(*mat, combined);
        });
    } else {
        media_data.removeProcessingStep("7__magic_eraser");
    }
}

}// namespace

bool isChainStepActive(MediaImageProcessingOptions const & options,
                       std::string const & chain_key) {
    return getActiveFlag(options, chain_key);
}

std::string chainStepOptionsJson(MediaImageProcessingOptions const & options,
                                std::string const & chain_key) {
    return getOptionsJson(options, chain_key);
}

void applyMediaProcessingChain(MediaData & target,
                               MediaImageProcessingOptions const & options) {
    target.clearProcessingSteps();
    if (!target.setImageProcessor("opencv")) {
        return;
    }

    for (auto const & step: ProcessingStepRegistry::instance().steps()) {
        applyRegistryStep(
                target,
                step.chain_key,
                getActiveFlag(options, step.chain_key),
                getOptionsJson(options, step.chain_key));
    }

    applyMagicEraser(target, options);
}

}// namespace Neuralyzer::MediaProcessing
