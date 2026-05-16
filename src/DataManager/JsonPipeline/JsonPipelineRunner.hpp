/**
 * @file JsonPipelineRunner.hpp
 * @brief Qt-free entry points for running DataManager JSON pipeline configs.
 */

#ifndef JSON_PIPELINE_RUNNER_HPP
#define JSON_PIPELINE_RUNNER_HPP

#include "DataManagerTypes.hpp"

#include "nlohmann/json_fwd.hpp"

#include <string>
#include <vector>

class DataManager;

namespace WhiskerToolbox::DataManagerPipeline {

/**
 * @brief Pipeline phase identifiers used for structured failure reporting.
 */
enum class JsonPipelinePhase {
    None,
    Parse,
    Normalize,
    LoadAndTransform
};

/**
 * @brief Options controlling JSON pipeline execution.
 */
struct JsonPipelineOptions {
    bool m_run_load_and_transform_phase = true;///< Execute legacy load/transform processing.
};

/**
 * @brief Result from running a JSON DataManager pipeline.
 */
struct JsonPipelineResult {
    bool m_success = false;                                    ///< True when all enabled phases complete.
    JsonPipelinePhase m_failed_phase = JsonPipelinePhase::None;///< Phase where execution failed.
    std::string m_error_message;                               ///< Human-readable failure message.
    std::vector<DataInfo> m_loaded_data;                       ///< Data reported by the load/transform phase.
};

/**
 * @brief Convert a pipeline phase to a stable diagnostic string.
 * @param phase Phase enum value to stringify.
 * @return String representation suitable for logs and error messages.
 * @post Return value is never empty.
 */
[[nodiscard]] std::string phaseToString(JsonPipelinePhase phase);

/**
 * @brief Run a DataManager JSON pipeline from an already parsed config.
 * @param data_manager DataManager instance to mutate.
 * @param config Parsed JSON config in legacy array form or object-root form.
 * @param base_path Directory used by existing relative-path loader semantics.
 * @param options Execution options.
 * @return Structured success/failure result.
 * @pre The DataManager object must remain valid for the duration of the call.
 * @post On success, all enabled phases have completed.
 */
[[nodiscard]] JsonPipelineResult runJsonPipeline(
        DataManager & data_manager,
        nlohmann::json const & config,
        std::string const & base_path,
        JsonPipelineOptions const & options = {});

/**
 * @brief Run a DataManager JSON pipeline from a config file.
 * @param data_manager DataManager instance to mutate.
 * @param json_filepath Path to a readable JSON configuration file.
 * @param options Execution options.
 * @return Structured success/failure result.
 * @pre The DataManager object must remain valid for the duration of the call.
 * @pre json_filepath must name a readable JSON file.
 * @post On success, all enabled phases have completed.
 */
[[nodiscard]] JsonPipelineResult runJsonPipelineFile(
        DataManager & data_manager,
        std::string const & json_filepath,
        JsonPipelineOptions const & options = {});

}// namespace WhiskerToolbox::DataManagerPipeline

#endif// JSON_PIPELINE_RUNNER_HPP
