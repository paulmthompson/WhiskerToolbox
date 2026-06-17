/**
 * @file RunTransformsV2PipelineAtTime.cpp
 * @brief Implementation of RunTransformsV2PipelineAtTime command
 */

#include "Commands/RunTransformsV2PipelineAtTime.hpp"

#include "Commands/Core/CommandContext.hpp"

#include "DataManager/utils/DataManagerMerge.hpp"
#include "DataManager/utils/DataManagerTemporalSubset.hpp"
#include "DataManager/DataManager.hpp" // DataManager
#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/io/PipelineLoader.hpp"
#include "TimeFrame/StrongTimeTypes.hpp" // TimeKey
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/interval_data.hpp"

#include <exception>
#include <filesystem>
#include <string>

namespace commands {

namespace {

/// @brief Store pipeline output into @p output_key, merging when supported.
/// @pre @p dm is non-null
/// @post On success, @p output_key exists with updated or merged data
[[nodiscard]] std::optional<std::string>
storePipelineOutput(DataManager & dm,
                    std::string const & input_key,
                    std::string const & output_key,
                    DataTypeVariant const & output) {
    TimeKey time_key;
    TimeKey const input_time_key = dm.getTimeKey(input_key);
    if (!input_time_key.empty()) {
        time_key = input_time_key;
    }

    auto const existing = dm.getDataVariant(output_key);
    if (existing.has_value()) {
        if (existing->index() != output.index()) {
            return "RunTransformsV2PipelineAtTime: output_key '" + output_key +
                   "' already exists with a different data type";
        }

        if (supportsMergeOverwrite(output)) {
            std::string merge_error;
            auto const merged = mergeOverwriteData(dm, output_key, output, merge_error);
            if (!merged.has_value()) {
                if (merge_error.empty()) {
                    merge_error = "mergeOverwriteData failed";
                }
                return "RunTransformsV2PipelineAtTime: " + merge_error;
            }
            return std::nullopt;
        }
    }

    dm.setData(output_key, output, time_key);
    return std::nullopt;
}

}// namespace

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

    if (_params.input_key.empty()) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: input_key must not be empty");
    }

    if (_params.output_key.empty()) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: output_key must not be empty");
    }

    if (_params.pipeline_path.empty()) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: pipeline_path must not be empty");
    }

    if (!ctx.data_manager->getDataVariant(_params.input_key).has_value()) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: input_key '" + _params.input_key +
                "' not found in DataManager");
    }

    auto const pipeline_result =
            WhiskerToolbox::Transforms::V2::Examples::loadPipelineFromFile(_params.pipeline_path);
    if (!pipeline_result) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: failed to load pipeline '" +
                _params.pipeline_path + "': " + pipeline_result.error()->what());
    }

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
        return CommandResult::error("RunTransformsV2PipelineAtTime: " + subset_error);
    }

    DataTypeVariant output;
    try {
        output = WhiskerToolbox::Transforms::V2::executePipeline(
                subset.value(), pipeline_result.value());
    } catch (std::exception const & ex) {
        return CommandResult::error(
                "RunTransformsV2PipelineAtTime: pipeline execution failed: " +
                std::string{ex.what()});
    }

    if (auto const store_error = storePipelineOutput(
                *ctx.data_manager,
                _params.input_key,
                _params.output_key,
                output)) {
        return CommandResult::error(*store_error);
    }

    return CommandResult::ok({_params.output_key});
}

}// namespace commands
