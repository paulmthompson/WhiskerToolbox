/**
 * @file RunTransformsV2PipelineAtTime.cpp
 * @brief Implementation of RunTransformsV2PipelineAtTime command
 */

#include "Commands/RunTransformsV2PipelineAtTime.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/TransformsV2PipelineOutput.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerTemporalSubset.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <exception>
#include <string>

namespace commands {

namespace {

constexpr std::string_view kCommandName = "RunTransformsV2PipelineAtTime";

}// namespace

RunTransformsV2PipelineAtTime::RunTransformsV2PipelineAtTime(
        RunTransformsV2PipelineAtTimeParams p)
    : _params(std::move(p)) {}

std::string RunTransformsV2PipelineAtTime::commandName() const {
    return std::string(kCommandName);
}

std::string RunTransformsV2PipelineAtTime::toJson() const {
    return rfl::json::write(_params);
}

CommandResult RunTransformsV2PipelineAtTime::execute(CommandContext const & ctx) {
    if (!ctx.data_manager) {
        return CommandResult::error(
                std::string(kCommandName) + " requires CommandContext::data_manager");
    }

    if (_params.input_key.empty()) {
        return CommandResult::error(
                std::string(kCommandName) + ": input_key must not be empty");
    }

    if (_params.output_key.empty()) {
        return CommandResult::error(
                std::string(kCommandName) + ": output_key must not be empty");
    }

    if (_params.pipeline_path.empty()) {
        return CommandResult::error(
                std::string(kCommandName) + ": pipeline_path must not be empty");
    }

    if (!ctx.data_manager->getDataVariant(_params.input_key).has_value()) {
        return CommandResult::error(
                std::string(kCommandName) + ": input_key '" + _params.input_key +
                "' not found in DataManager");
    }

    auto const pipeline_result =
            Neuralyzer::Transforms::V2::Examples::loadPipelineFromFile(_params.pipeline_path);
    if (!pipeline_result) {
        return CommandResult::error(
                std::string(kCommandName) + ": failed to load pipeline '" +
                _params.pipeline_path + "': " +
                pipeline_result.error()->what());// NOLINT(bugprone-unchecked-optional-access)
    }
    auto const & pipeline = pipeline_result.value();

    auto const time = TimeFrameIndex(_params.time);
    std::string subset_error;
    auto const subset = createTemporalSubset(
            *ctx.data_manager,
            _params.input_key,
            TimeFrameInterval{time, time},
            subset_error);
    if (!subset.has_value()) {
        if (subset_error.empty()) {
            subset_error = "createTemporalSubset failed";
        }
        return CommandResult::error(std::string(kCommandName) + ": " + subset_error);
    }

    DataTypeVariant output;
    try {
        output = Neuralyzer::Transforms::V2::executePipeline(
                subset.value(), pipeline);
    } catch (std::exception const & ex) {
        return CommandResult::error(
                std::string(kCommandName) + ": pipeline execution failed: " +
                std::string{ex.what()});
    }

    if (auto const store_error = storePipelineOutputMerged(
                *ctx.data_manager,
                _params.input_key,
                _params.output_key,
                output,
                kCommandName)) {
        return CommandResult::error(*store_error);
    }

    return CommandResult::ok({_params.output_key});
}

}// namespace commands
