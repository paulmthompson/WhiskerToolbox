#ifndef PIPELINE_JSON_TEST_HELPERS_HPP
#define PIPELINE_JSON_TEST_HELPERS_HPP

/**
 * @file pipeline_json_test_helpers.hpp
 * @brief Build and execute DataManager pipeline configs from typed reflect-cpp parameters
 */

#include "DataManager.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/io/ParameterIO.hpp"

#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pipeline_json_test {

using WhiskerToolbox::Transforms::V2::DataManagerPipelineDescriptor;
using WhiskerToolbox::Transforms::V2::DataManagerPipelineExecutor;
using WhiskerToolbox::Transforms::V2::DataManagerPipelineMetadata;
using WhiskerToolbox::Transforms::V2::DataManagerStepDescriptor;
using WhiskerToolbox::Transforms::V2::load_data_from_json_config_v2;
using WhiskerToolbox::Transforms::V2::V2PipelineResult;

/**
 * @brief Convert typed transform parameters to rfl::Generic via reflect-cpp JSON
 * @tparam ParamsT Reflect-cpp parameter struct (e.g. MaskAreaParams)
 * @param params Parameter values to serialize
 * @return Generic JSON representation suitable for pipeline step descriptors
 */
template<typename ParamsT>
inline rfl::Generic parametersAsGeneric(ParamsT const & params) {
    auto const json_str = WhiskerToolbox::Transforms::V2::Examples::saveParametersToJson(params);
    auto const result = rfl::json::read<rfl::Generic>(json_str);
    return result.value();
}

/**
 * @brief Build a DataManager pipeline step with typed parameters
 * @tparam ParamsT Reflect-cpp parameter struct for the transform
 */
template<typename ParamsT>
inline DataManagerStepDescriptor makeStep(std::string step_id,
                                          std::string transform_name,
                                          std::string input_key,
                                          std::string output_key,
                                          ParamsT const & params) {
    return DataManagerStepDescriptor{
            .step_id = std::move(step_id),
            .transform_name = std::move(transform_name),
            .input_key = std::move(input_key),
            .output_key = std::move(output_key),
            .parameters = parametersAsGeneric(params),
    };
}

/**
 * @brief Build a DataManager pipeline descriptor from steps and optional metadata
 */
inline DataManagerPipelineDescriptor makePipeline(
        std::vector<DataManagerStepDescriptor> steps,
        std::optional<DataManagerPipelineMetadata> metadata = std::nullopt) {
    return DataManagerPipelineDescriptor{
            .metadata = std::move(metadata),
            .steps = std::move(steps),
    };
}

/**
 * @brief Build a single-step pipeline with typed parameters
 * @tparam ParamsT Reflect-cpp parameter struct for the transform
 */
template<typename ParamsT>
inline DataManagerPipelineDescriptor makeSingleStepPipeline(std::string transform_name,
                                                            std::string input_key,
                                                            std::string output_key,
                                                            ParamsT const & params,
                                                            std::string step_id = "1") {
    return makePipeline({makeStep(std::move(step_id),
                                  std::move(transform_name),
                                  std::move(input_key),
                                  std::move(output_key),
                                  params)});
}

/**
 * @brief Serialize a pipeline descriptor to nlohmann::json (inner "steps" format)
 */
inline nlohmann::json pipelineToJson(DataManagerPipelineDescriptor const & pipeline) {
    return nlohmann::json::parse(rfl::json::write(pipeline));
}

/**
 * @brief Wrap a pipeline descriptor in the V1-compatible config array format
 *
 * Format: [{ "transformations": { "metadata": ..., "steps": [...] } }]
 */
inline nlohmann::json pipelineToV1ConfigArrayJson(DataManagerPipelineDescriptor const & pipeline) {
    nlohmann::json entry;
    entry["transformations"] = pipelineToJson(pipeline);
    return nlohmann::json::array({std::move(entry)});
}

/**
 * @brief Result of loading and executing a pipeline via DataManagerPipelineExecutor
 */
struct PipelineLoadExecuteResult {
    bool load_ok = false;
    V2PipelineResult execution{};
};

/**
 * @brief Load and execute a pipeline via DataManagerPipelineExecutor (in-memory, no filesystem)
 */
inline PipelineLoadExecuteResult executeViaExecutor(DataManager & dm,
                                                    DataManagerPipelineDescriptor const & pipeline) {
    DataManagerPipelineExecutor executor(&dm);
    PipelineLoadExecuteResult result;
    result.load_ok = executor.loadFromJson(pipelineToJson(pipeline));
    if (result.load_ok) {
        result.execution = executor.execute();
    }
    return result;
}

/**
 * @brief Load and execute a pipeline via load_data_from_json_config_v2 (in-memory, no filesystem)
 */
inline std::vector<DataInfo> executeViaLoadDataFromJsonConfigV2(
        DataManager & dm,
        DataManagerPipelineDescriptor const & pipeline) {
    return load_data_from_json_config_v2(&dm, pipelineToV1ConfigArrayJson(pipeline), "");
}

}// namespace pipeline_json_test

#endif// PIPELINE_JSON_TEST_HELPERS_HPP
