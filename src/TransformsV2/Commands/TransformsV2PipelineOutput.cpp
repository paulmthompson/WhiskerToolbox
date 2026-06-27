/**
 * @file TransformsV2PipelineOutput.cpp
 * @brief Implementation of shared TransformsV2 pipeline output storage helpers
 */

#include "Commands/TransformsV2PipelineOutput.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DataManagerMerge.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <string>
#include <string_view>

namespace commands {

namespace {

[[nodiscard]] std::string makeError(std::string_view command_prefix, std::string message) {
    return std::string(command_prefix) + ": " + std::move(message);
}

}// namespace

std::optional<TimeKey>
resolvePipelineOutputTimeKey(DataManager & dm,
                             std::string const & input_key,// NOLINT(bugprone-easily-swappable-parameters)
                             std::string const & output_time_key,
                             std::string_view command_prefix,
                             std::string & error_message) {
    if (output_time_key.empty()) {
        return dm.getTimeKey(input_key);
    }

    auto const time_frame = dm.getTime(TimeKey(output_time_key));
    if (time_frame == nullptr) {
        error_message = makeError(
                command_prefix,
                "output_time_key '" + output_time_key + "' not found in DataManager");
        return std::nullopt;
    }

    return TimeKey(output_time_key);
}

std::optional<std::string>
storePipelineOutputMerged(DataManager & dm,
                          std::string const & input_key,// NOLINT(bugprone-easily-swappable-parameters)
                          std::string const & output_key,
                          DataTypeVariant const & output,
                          std::string_view command_prefix) {
    TimeKey time_key;
    TimeKey const input_time_key = dm.getTimeKey(input_key);
    if (!input_time_key.empty()) {
        time_key = input_time_key;
    }

    auto const existing = dm.getDataVariant(output_key);
    if (existing.has_value()) {
        if (existing->index() != output.index()) {
            return makeError(
                    command_prefix,
                    "output_key '" + output_key + "' already exists with a different data type");
        }

        if (supportsMergeOverwrite(output)) {
            std::string merge_error;
            auto const merged = mergeOverwriteData(dm, output_key, output, merge_error);
            if (!merged.has_value()) {
                if (merge_error.empty()) {
                    merge_error = "mergeOverwriteData failed";
                }
                return makeError(command_prefix, merge_error);
            }
            return std::nullopt;
        }
    }

    dm.setData(output_key, output, time_key);
    return std::nullopt;
}

std::optional<std::string>
storePipelineOutputReplace(DataManager & dm,
                           std::string const & output_key,
                           DataTypeVariant const & output,
                           TimeKey const & time_key,
                           std::string_view command_prefix) {
    auto const existing = dm.getDataVariant(output_key);
    if (existing.has_value() && existing->index() != output.index()) {
        return makeError(
                command_prefix,
                "output_key '" + output_key + "' already exists with a different data type");
    }

    dm.setData(output_key, output, time_key);
    return std::nullopt;
}

}// namespace commands
