/**
 * @file RunTransformsV2PipelineAtTime.cpp
 * @brief Implementation of RunTransformsV2PipelineAtTime command (stub)
 */

#include "Commands/RunTransformsV2PipelineAtTime.hpp"

#include "Commands/Core/CommandContext.hpp"

#include "TransformsV2/io/PipelineLibrary.hpp"
#include "TransformsV2/io/PipelineLoader.hpp" // PipelineStepDescriptor

#include <spdlog/spdlog.h>

#include <filesystem>

namespace commands {

RunTransformsV2PipelineAtTime::RunTransformsV2PipelineAtTime(
        RunTransformsV2PipelineAtTimeParams p)
    : _params(std::move(p)) {}

std::string RunTransformsV2PipelineAtTime::commandName() const {
    return "RunTransformsV2PipelineAtTime";
}

std::string RunTransformsV2PipelineAtTime::toJson() const {
    return rfl::json::write(_params);
}

CommandResult RunTransformsV2PipelineAtTime::execute(CommandContext const & ctx) {
    if (!ctx.data_manager) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime requires CommandContext::data_manager");
    }

    if (_params.pipeline_path.empty()) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: pipeline_path must not be empty");
    }

    auto const load_result = WhiskerToolbox::Transforms::V2::Examples::loadPipelineDescriptorFromFile(
            std::filesystem::path{_params.pipeline_path});
    if (!load_result) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: failed to load pipeline '" +
                _params.pipeline_path + "': " + load_result.error()->what());
    }

    spdlog::info(
            "RunTransformsV2PipelineAtTime (stub): input_key='{}' output_key='{}' "
            "time={} pipeline_path='{}'",
            _params.input_key,
            _params.output_key,
            _params.time,
            _params.pipeline_path);

    return CommandResult::ok();
}

}// namespace commands
