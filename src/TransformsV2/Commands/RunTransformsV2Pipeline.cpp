/**
 * @file RunTransformsV2Pipeline.cpp
 * @brief Implementation of RunTransformsV2Pipeline command
 */

#include "Commands/RunTransformsV2Pipeline.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/TransformsV2PipelineOutput.hpp"

#include "DataManager/DataManager.hpp"
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"

#include <exception>
#include <string>

namespace commands {

namespace {

constexpr std::string_view kCommandName = "RunTransformsV2Pipeline";

}// namespace

RunTransformsV2Pipeline::RunTransformsV2Pipeline(RunTransformsV2PipelineParams p)
    : _params(std::move(p)) {}

std::string RunTransformsV2Pipeline::commandName() const {
    return std::string(kCommandName);
}

std::string RunTransformsV2Pipeline::toJson() const {
    return rfl::json::write(_params);
}

CommandResult RunTransformsV2Pipeline::execute(CommandContext const & ctx) {
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

    auto const input_variant = ctx.data_manager->getDataVariant(_params.input_key);
    if (!input_variant.has_value()) {
        return CommandResult::error(
                std::string(kCommandName) + ": input_key '" + _params.input_key +
                "' not found in DataManager");
    }

    std::string time_key_error;
    auto const time_key = resolvePipelineOutputTimeKey(
            *ctx.data_manager,
            _params.input_key,
            _params.output_time_key,
            kCommandName,
            time_key_error);
    if (!time_key.has_value()) {
        return CommandResult::error(time_key_error);
    }

    auto const pipeline_result =
            Neuralyzer::Transforms::V2::Examples::loadPipelineFromFile(_params.pipeline_path);
    if (!pipeline_result) {
        return CommandResult::error(
                std::string(kCommandName) + ": failed to load pipeline '" +
                _params.pipeline_path + "': " +
                pipeline_result.error()->what());// NOLINT(bugprone-unchecked-optional-access)
    }

    DataTypeVariant output;
    try {
        output = Neuralyzer::Transforms::V2::executePipeline(
                input_variant.value(),
                pipeline_result.value());
    } catch (std::exception const & ex) {
        return CommandResult::error(
                std::string(kCommandName) + ": pipeline execution failed: " +
                std::string{ex.what()});
    }

    if (auto const store_error = storePipelineOutputReplace(
                *ctx.data_manager,
                _params.output_key,
                output,
                time_key.value(),
                kCommandName)) {
        return CommandResult::error(*store_error);
    }

    return CommandResult::ok({_params.output_key});
}

}// namespace commands
