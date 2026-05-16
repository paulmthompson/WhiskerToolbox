/**
 * @file JsonPipelineRunner.cpp
 * @brief Implementation of Qt-free DataManager JSON pipeline execution.
 */

#include "JsonPipelineRunner.hpp"

#include "DataManager.hpp"

#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace WhiskerToolbox::DataManagerPipeline {

namespace {

/**
 * @brief Convert the supported root schemas into the legacy loader schema.
 * @param config Parsed input configuration.
 * @return JSON value accepted by load_data_from_json_config().
 * @pre config must be an array, or an object containing "data" and/or "transformations".
 * @post Returned JSON is either an array or an object with a "data" array.
 */
[[nodiscard]] nlohmann::json normalizeForLegacyLoad(nlohmann::json const & config) {
    if (config.is_array()) {
        return config;
    }

    if (!config.is_object()) {
        throw std::invalid_argument("Pipeline config root must be an object or array");
    }

    if (!config.contains("data") && !config.contains("transformations")) {
        throw std::invalid_argument("Pipeline config object must contain 'data' or 'transformations'");
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
