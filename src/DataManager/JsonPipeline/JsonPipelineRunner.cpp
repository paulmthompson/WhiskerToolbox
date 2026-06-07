/**
 * @file JsonPipelineRunner.cpp
 * @brief Implementation of Qt-free DataManager JSON pipeline execution.
 */

#include "JsonPipelineRunner.hpp"

#include "Commands/Core/CommandContext.hpp"
#include "Commands/Core/CommandDescriptor.hpp"
#include "Commands/Core/SequenceExecution.hpp"
#include "Commands/Core/VariableSubstitution.hpp"
#include "Commands/register_core_commands.hpp"

#include "DataManager.hpp"

#include "nlohmann/json.hpp"

#include <rfl/json.hpp>

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace WhiskerToolbox::DataManagerPipeline {

namespace {

/**
 * @brief Convert the supported root schemas into the legacy loader schema.
 * @param config Parsed input configuration.
 * @return JSON value accepted by load_data_from_json_config().
 * @pre config must be an array, or an object containing a recognized pipeline section.
 * @post Returned JSON is either an array or an object with a "data" array.
 */
[[nodiscard]] nlohmann::json normalizeForLegacyLoad(nlohmann::json const & config) {
    if (config.is_array()) {
        return config;
    }

    if (!config.is_object()) {
        throw std::invalid_argument("Pipeline config root must be an object or array");
    }

    if (!config.contains("data") &&
        !config.contains("transformations") &&
        !config.contains("commands") &&
        !config.contains("saves")) {
        throw std::invalid_argument("Pipeline config object must contain 'data', 'transformations', 'commands', or 'saves'");
    }

    nlohmann::json normalized = nlohmann::json::object();
    if (config.contains("variables")) {
        if (!config["variables"].is_object()) {
            throw std::invalid_argument("'variables' must be an object when present");
        }
        normalized["variables"] = config["variables"];
    }

    nlohmann::json data = nlohmann::json::array();
    if (config.contains("data")) {
        if (!config["data"].is_array()) {
            throw std::invalid_argument("'data' must be an array when present");
        }
        data = config["data"];
    }

    if (config.contains("transformations")) {
        data.push_back(nlohmann::json{{"transformations", config["transformations"]}});
    }

    normalized["data"] = std::move(data);
    return normalized;
}

/**
 * @brief Construct a failed JsonPipelineResult.
 * @param phase Phase that failed.
 * @param message Diagnostic message.
 * @return Failed result populated with phase and message.
 * @post Returned result has success == false.
 */
[[nodiscard]] JsonPipelineResult makeFailure(JsonPipelinePhase const phase, std::string message) {
    JsonPipelineResult result;
    result.m_success = false;
    result.m_failed_phase = phase;
    result.m_error_message = std::move(message);
    return result;
}

/**
 * @brief Extract string variables from a root pipeline config.
 * @param config Parsed pipeline config.
 * @return Map suitable for command variable substitution.
 * @pre config may be any JSON value.
 * @post Only string-valued variables are included.
 */
[[nodiscard]] std::map<std::string, std::string> extractVariables(nlohmann::json const & config) {
    std::map<std::string, std::string> variables;
    if (!config.is_object() || !config.contains("variables") || !config["variables"].is_object()) {
        return variables;
    }

    for (auto const & [key, value]: config["variables"].items()) {
        if (value.is_string()) {
            variables.emplace(key, value.get<std::string>());
        }
    }
    return variables;
}

/**
 * @brief Resolve an output path against the pipeline config directory.
 * @param path Save path from the JSON descriptor.
 * @param base_path Directory containing the pipeline config.
 * @return Absolute or base-relative normalized output path.
 * @pre path must not be empty.
 * @post Absolute paths are returned unchanged except lexical normalization.
 */
[[nodiscard]] std::string resolveSavePath(std::string const & path, std::string const & base_path) {
    assert(!path.empty() && "resolveSavePath: path must not be empty");
    std::filesystem::path output_path(path);
    if (!output_path.is_absolute() && !base_path.empty()) {
        output_path = std::filesystem::path(base_path) / output_path;
    }
    return output_path.lexically_normal().string();
}

/**
 * @brief Create a non-owning shared pointer for command context wiring.
 * @param data_manager DataManager reference to expose to commands.
 * @return Shared pointer that observes but does not delete the DataManager.
 * @pre The DataManager object must outlive all commands using the returned pointer.
 * @post Deleting the returned shared pointer does not delete the DataManager.
 */
[[nodiscard]] std::shared_ptr<DataManager> makeNonOwningDataManagerPtr(DataManager & data_manager) {
    return std::shared_ptr<DataManager>(&data_manager, [](DataManager *) {});
}

/**
 * @brief Create the command runtime context for the active DataManager.
 * @param data_manager DataManager reference to expose to commands.
 * @return CommandContext containing a non-owning DataManager shared pointer.
 * @pre The DataManager object must outlive commands using this context.
 * @post The returned context does not own the DataManager.
 */
[[nodiscard]] commands::CommandContext makeCommandContext(DataManager & data_manager) {
    commands::CommandContext context;
    context.data_manager = makeNonOwningDataManagerPtr(data_manager);
    return context;
}

/**
 * @brief Execute root-level command descriptors as a command sequence.
 * @param data_manager DataManager used by command execution.
 * @param config Parsed root pipeline config.
 * @param variables Static variables for sequence substitution.
 * @param stop_on_error If true, stop at the first failed command.
 * @return Success result containing affected keys, or a failed result.
 * @pre The DataManager object must remain valid for the duration of the call.
 * @post On success, m_command_affected_keys contains keys reported by commands.
 */
[[nodiscard]] JsonPipelineResult executeCommands(
        DataManager & data_manager,
        nlohmann::json const & config,
        std::map<std::string, std::string> const & variables,
        bool const stop_on_error) {
    JsonPipelineResult result;
    result.m_success = true;

    if (!config.is_object() || !config.contains("commands")) {
        return result;
    }

    if (!config["commands"].is_array()) {
        return makeFailure(JsonPipelinePhase::Command, "'commands' must be an array when present");
    }

    nlohmann::json sequence_json = nlohmann::json::object();
    sequence_json["commands"] = config["commands"];
    if (!variables.empty()) {
        sequence_json["variables"] = variables;
    }

    auto sequence = rfl::json::read<commands::CommandSequenceDescriptor>(sequence_json.dump());
    if (!sequence) {
        return makeFailure(JsonPipelinePhase::Command, "Failed to parse root-level command sequence");
    }

    commands::register_core_commands();
    auto sequence_result = commands::executeSequence(*sequence, makeCommandContext(data_manager), stop_on_error);
    result.m_command_affected_keys = std::move(sequence_result.result.affected_keys);
    result.m_failed_command_index = sequence_result.failed_index;

    if (!sequence_result.result.success) {
        result.m_success = false;
        result.m_failed_phase = JsonPipelinePhase::Command;
        result.m_error_message = sequence_result.result.error_message;
        return result;
    }

    return result;
}

/**
 * @brief Execute root-level SaveData descriptors.
 * @param data_manager DataManager containing objects to save.
 * @param config Parsed root pipeline config.
 * @param base_path Directory containing the config file.
 * @param variables Static variables for descriptor substitution.
 * @param stop_on_error If true, stop at the first failed save.
 * @return Success result containing saved paths, or a failed result.
 * @pre The DataManager object must remain valid for the duration of the call.
 * @post Each successful save path is appended to m_saved_paths.
 */
[[nodiscard]] JsonPipelineResult executeSaves(
        DataManager & data_manager,
        nlohmann::json const & config,
        std::string const & base_path,
        std::map<std::string, std::string> const & variables,
        bool const stop_on_error) {
    JsonPipelineResult result;
    result.m_success = true;

    if (!config.is_object() || !config.contains("saves")) {
        return result;
    }

    if (!config["saves"].is_array()) {
        return makeFailure(JsonPipelinePhase::Save, "'saves' must be an array when present");
    }

    commands::register_core_commands();

    auto context = makeCommandContext(data_manager);

    for (std::size_t index = 0; index < config["saves"].size(); ++index) {
        auto const & save_descriptor = config["saves"][index];
        if (!save_descriptor.is_object()) {
            auto failure = makeFailure(JsonPipelinePhase::Save, "Save descriptor at index " + std::to_string(index) + " must be an object");
            if (stop_on_error) {
                return failure;
            }
            result = std::move(failure);
            continue;
        }

        auto substituted = commands::substituteVariables(save_descriptor.dump(), variables);
        auto save_json = nlohmann::json::parse(substituted, nullptr, false);
        if (save_json.is_discarded()) {
            auto failure = makeFailure(JsonPipelinePhase::Save, "Failed to parse save descriptor at index " + std::to_string(index) + " after variable substitution");
            if (stop_on_error) {
                return failure;
            }
            result = std::move(failure);
            continue;
        }

        if (!save_json.contains("path") || !save_json["path"].is_string()) {
            auto failure = makeFailure(JsonPipelinePhase::Save, "Save descriptor at index " + std::to_string(index) + " requires string field 'path'");
            if (stop_on_error) {
                return failure;
            }
            result = std::move(failure);
            continue;
        }

        auto const resolved_path = resolveSavePath(save_json["path"].get<std::string>(), base_path);
        save_json["path"] = resolved_path;

        auto command_result = commands::executeSingleCommand("SaveData", save_json.dump(), context);
        if (!command_result.success) {
            auto failure = makeFailure(
                    JsonPipelinePhase::Save,
                    "SaveData failed at index " + std::to_string(index) + ": " + command_result.error_message);
            if (stop_on_error) {
                return failure;
            }
            result = std::move(failure);
            continue;
        }

        result.m_saved_paths.push_back(resolved_path);
    }

    return result;
}

}// namespace

std::string phaseToString(JsonPipelinePhase const phase) {
    switch (phase) {
        case JsonPipelinePhase::None:
            return "none";
        case JsonPipelinePhase::Parse:
            return "parse";
        case JsonPipelinePhase::Normalize:
            return "normalize";
        case JsonPipelinePhase::LoadAndTransform:
            return "load_and_transform";
        case JsonPipelinePhase::Command:
            return "command";
        case JsonPipelinePhase::Save:
            return "save";
    }
    return "unknown";
}

JsonPipelineResult runJsonPipeline(
        DataManager & data_manager,
        nlohmann::json const & config,
        std::string const & base_path,
        JsonPipelineOptions const & options) {

    nlohmann::json normalized;
    try {
        normalized = normalizeForLegacyLoad(config);
    } catch (std::exception const & e) {
        return makeFailure(JsonPipelinePhase::Normalize, e.what());
    }

    JsonPipelineResult result;
    if (options.m_run_load_and_transform_phase) {
        try {
            result.m_loaded_data = load_data_from_json_config(&data_manager, normalized, base_path);
        } catch (std::exception const & e) {
            return makeFailure(JsonPipelinePhase::LoadAndTransform, e.what());
        }
    }

    if (options.m_run_command_phase) {
        auto command_result = executeCommands(
                data_manager,
                config,
                extractVariables(config),
                options.m_stop_on_command_error);
        result.m_command_affected_keys = std::move(command_result.m_command_affected_keys);
        result.m_failed_command_index = command_result.m_failed_command_index;
        if (!command_result.m_success) {
            command_result.m_loaded_data = std::move(result.m_loaded_data);
            return command_result;
        }
    }

    if (options.m_run_save_phase) {
        auto save_result = executeSaves(
                data_manager,
                config,
                base_path,
                extractVariables(config),
                options.m_stop_on_save_error);
        result.m_saved_paths = std::move(save_result.m_saved_paths);
        if (!save_result.m_success) {
            save_result.m_loaded_data = std::move(result.m_loaded_data);
            save_result.m_command_affected_keys = std::move(result.m_command_affected_keys);
            save_result.m_failed_command_index = result.m_failed_command_index;
            return save_result;
        }
    }

    result.m_success = true;
    return result;
}

JsonPipelineResult runJsonPipelineFile(
        DataManager & data_manager,
        std::string const & json_filepath,
        JsonPipelineOptions const & options) {

    try {
        std::ifstream input(json_filepath);
        if (!input.is_open()) {
            return makeFailure(JsonPipelinePhase::Parse, "Failed to open JSON file: " + json_filepath);
        }

        nlohmann::json config;
        input >> config;

        auto const base_path = std::filesystem::path(json_filepath).parent_path().string();
        return runJsonPipeline(data_manager, config, base_path, options);
    } catch (std::exception const & e) {
        return makeFailure(JsonPipelinePhase::Parse, e.what());
    }
}

}// namespace WhiskerToolbox::DataManagerPipeline
