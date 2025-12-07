#ifndef WHISKERTOOLBOX_V2_DATAMANAGER_INTEGRATION_HPP
#define WHISKERTOOLBOX_V2_DATAMANAGER_INTEGRATION_HPP

#include "DataManager/DataManagerTypes.hpp" // DataTypeVariant

#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include <fstream>
#include <functional>

#include <optional>
#include <string>
#include <vector>

class DataManager;

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Pipeline Step Descriptor with DataManager Keys (V1-compatible format)
// ============================================================================

/**
 * @brief Descriptor for a pipeline step that includes DataManager integration
 * 
 * This extends the basic PipelineStepDescriptor with input_key and output_key
 * for DataManager integration, matching the V1 JSON format.
 * 
 * Example JSON:
 * ```json
 * {
 *   "step_id": "calculate_area",
 *   "transform_name": "CalculateMaskArea",
 *   "input_key": "mask_data",
 *   "output_key": "calculated_areas",
 *   "parameters": {
 *     "scale_factor": 1.5
 *   }
 * }
 * ```
 * 
 * For multi-input (binary) transforms:
 * ```json
 * {
 *   "step_id": "calculate_distance",
 *   "transform_name": "CalculateLineMinPointDistance",
 *   "input_key": "line_data",
 *   "additional_input_keys": ["point_data"],
 *   "output_key": "distances",
 *   "parameters": {}
 * }
 * ```
 */
struct DataManagerStepDescriptor {
    // Unique identifier for this step (for error reporting and dependencies)
    std::string step_id;

    // Name of the transform (must exist in ElementRegistry)
    std::string transform_name;

    // Key to retrieve primary input data from DataManager
    std::string input_key;

    // Additional input keys for multi-input (binary/n-ary) transforms
    // Combined with input_key to form tuple: (input_key, additional_input_keys[0], ...)
    std::optional<std::vector<std::string>> additional_input_keys;

    // Key to store output data in DataManager (optional - if empty, data is temporary)
    std::optional<std::string> output_key;

    // Raw JSON parameters - will be parsed based on transform_name
    std::optional<rfl::Generic> parameters;

    // Optional fields for organization and control
    std::optional<std::string> description;
    std::optional<bool> enabled;
    std::optional<int> phase;
    std::optional<std::vector<std::string>> tags;

    // Helper to check if this is a multi-input step
    bool isMultiInput() const {
        return additional_input_keys.has_value() && !additional_input_keys->empty();
    }

    // Get all input keys in order
    std::vector<std::string> getAllInputKeys() const {
        std::vector<std::string> keys;
        keys.push_back(input_key);
        if (additional_input_keys.has_value()) {
            for (auto const & key : *additional_input_keys) {
                keys.push_back(key);
            }
        }
        return keys;
    }
};

/**
 * @brief Metadata for a DataManager-integrated pipeline
 */
struct DataManagerPipelineMetadata {
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> version;
    std::optional<std::string> author;
    std::optional<std::vector<std::string>> tags;
};

/**
 * @brief Complete pipeline descriptor with DataManager integration
 * 
 * This matches the V1 JSON format used by TransformPipeline in V1.
 */
struct DataManagerPipelineDescriptor {
    std::optional<DataManagerPipelineMetadata> metadata;
    std::vector<DataManagerStepDescriptor> steps;
};

// ============================================================================
// Execution Result Types
// ============================================================================

/**
 * @brief Result of executing a single pipeline step
 */
struct V2StepResult {
    bool success = false;
    std::string error_message;
    std::string output_key;
    double execution_time_ms = 0.0;
};

/**
 * @brief Result of executing a complete pipeline
 */
struct V2PipelineResult {
    bool success = false;
    std::string error_message;
    std::vector<V2StepResult> step_results;
    double total_execution_time_ms = 0.0;
    int steps_completed = 0;
    int total_steps = 0;
};

/**
 * @brief Progress callback for V2 pipeline execution
 * 
 * @param step_index Current step being executed (0-based)
 * @param step_name Name of the current step
 * @param step_progress Progress of current step (0-100)
 * @param overall_progress Overall pipeline progress (0-100)
 */
using V2PipelineProgressCallback = std::function<void(
        int step_index,
        std::string const & step_name,
        int step_progress,
        int overall_progress)>;

// ============================================================================
// DataManager-Integrated Pipeline Executor
// ============================================================================

/**
 * @brief Executor that runs V2 pipelines with DataManager integration
 * 
 * This class bridges the gap between the V2 transform system and DataManager:
 * - Loads pipeline configuration from JSON (V1-compatible format)
 * - Retrieves input data from DataManager using input_key
 * - Executes transforms using V2 TransformPipeline
 * - Stores results back to DataManager using output_key
 * 
 * Example usage:
 * ```cpp
 * DataManager dm;
 * // ... populate dm with data ...
 * 
 * DataManagerPipelineExecutor executor(&dm);
 * if (executor.loadFromJson(json_config)) {
 *     auto result = executor.execute();
 *     if (result.success) {
 *         // Results are now in DataManager
 *     }
 * }
 * ```
 */
class DataManagerPipelineExecutor {
public:
    /**
     * @brief Construct executor with DataManager pointer
     * 
     * @param data_manager Pointer to the DataManager (must not be null)
     */
    explicit DataManagerPipelineExecutor(DataManager * data_manager);

    /**
     * @brief Load pipeline configuration from JSON object
     * 
     * @param json_config JSON configuration object
     * @return true if configuration was loaded successfully
     */
    bool loadFromJson(nlohmann::json const & json_config);

    /**
     * @brief Load pipeline configuration from JSON file
     * 
     * @param json_file_path Path to JSON configuration file
     * @return true if configuration was loaded successfully
     */
    bool loadFromJsonFile(std::string const & json_file_path);

    /**
     * @brief Execute the loaded pipeline
     * 
     * @param progress_callback Optional progress callback
     * @return V2PipelineResult containing execution results
     */
    V2PipelineResult execute(V2PipelineProgressCallback progress_callback = nullptr);

    /**
     * @brief Execute a single step
     * 
     * @param step_index Index of the step to execute
     * @param progress_callback Optional progress callback
     * @return V2StepResult containing step result
     */
    V2StepResult executeStep(size_t step_index, std::function<void(int)> progress_callback = nullptr);

    /**
     * @brief Validate the pipeline configuration
     * 
     * @return std::vector<std::string> List of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Get the loaded pipeline steps
     */
    std::vector<DataManagerStepDescriptor> const & getSteps() const { return steps_; }

    /**
     * @brief Get pipeline metadata
     */
    std::optional<DataManagerPipelineMetadata> const & getMetadata() const { return metadata_; }

    /**
     * @brief Clear the current pipeline configuration
     */
    void clear();

    // ========================================================================
    // Pipeline Segment Analysis and Fusion
    // ========================================================================

    /**
     * @brief Represents a segment of consecutive steps that can be fused
     * 
     * The executor analyzes steps and groups them into fusible segments:
     * - Element-wise steps can be fused together
     * - Multi-input steps start new segments
     * - Time-grouped and container transforms force materialization
     */
    struct PipelineSegment {
        size_t start_step;       // First step index (inclusive)
        size_t end_step;         // Last step index (exclusive)
        bool is_multi_input;     // First step has multiple inputs
        std::vector<std::string> input_keys;  // All input keys for this segment
        std::string output_key;  // Output key (from last step in segment)
        bool requires_materialization;  // True if segment contains non-fusible transform
    };

    /**
     * @brief Build fusible segments from consecutive steps
     * 
     * Analyzes the pipeline and groups consecutive fusible steps together.
     * Returns a vector of segments that can be executed independently.
     */
    std::vector<PipelineSegment> buildSegments() const;

    /**
     * @brief Check if a step can be fused with the previous step
     * 
     * A step can be fused if:
     * - It is an element-level transform (not time-grouped, not container)
     * - It has a single input
     * - Its input comes from the previous step's output
     */
    bool canFuseStep(size_t step_index) const;

    /**
     * @brief Check if consecutive steps form a data dependency chain
     * 
     * Returns true if step[i]'s input_key matches step[i-1]'s output_key
     * or step[i-1]'s step_id (for temporary data).
     */
    bool stepsAreChained(size_t prev_step, size_t curr_step) const;

private:
    DataManager * data_manager_;
    std::vector<DataManagerStepDescriptor> steps_;
    std::optional<DataManagerPipelineMetadata> metadata_;

    // Temporary data storage for intermediate results
    std::unordered_map<std::string, DataTypeVariant> temporary_data_;

    /**
     * @brief Parse the JSON format (handles both direct and V1-compatible formats)
     */
    bool parseJsonFormat(nlohmann::json const & json_config);

    /**
     * @brief Get input data from DataManager or temporary storage
     */
    std::optional<DataTypeVariant> getInputData(std::string const & input_key);

    /**
     * @brief Store output data to DataManager
     */
    void storeOutputData(std::string const & output_key,
                         DataTypeVariant const & data,
                         std::string const & step_id);

    /**
     * @brief Execute a transform on the input data using the V2 system
     */
    std::optional<DataTypeVariant> executeTransform(
            std::string const & transform_name,
            DataTypeVariant const & input_data,
            std::optional<rfl::Generic> const & parameters);

    /**
     * @brief Execute a container transform with dynamic dispatch
     * 
     * Helper for executeTransform when the transform is a container transform.
     */
    std::optional<DataTypeVariant> executeContainerTransformDynamic(
            std::string const & transform_name,
            DataTypeVariant const & input_data,
            std::optional<rfl::Generic> const & parameters);

    /**
     * @brief Execute a multi-input (binary) transform step
     * 
     * Handles steps with additional_input_keys by:
     * 1. Retrieving all input containers
     * 2. Creating a zipped view using FlatZipView
     * 3. Building a fused pipeline for consecutive element-wise steps
     * 4. Executing and materializing the result
     * 
     * @param step_index Index of the multi-input step
     * @return Result containing output data or error
     */
    std::optional<DataTypeVariant> executeMultiInputStep(size_t step_index);

    /**
     * @brief Execute a segment of fused steps
     * 
     * For segments with multiple element-wise steps, creates a single
     * TransformPipeline and executes all steps in one pass.
     */
    std::optional<DataTypeVariant> executeSegment(PipelineSegment const & segment);
};

// ============================================================================
// V2 Load Data From JSON Config
// ============================================================================

/**
 * @brief Load and execute transformation pipeline from JSON config (V2 version)
 * 
 * This function provides the same interface as the V1 load_data_from_json_config
 * but uses the V2 transformation system internally.
 * 
 * The JSON format matches V1:
 * ```json
 * [
 *   {
 *     "transformations": {
 *       "metadata": {
 *         "name": "Pipeline Name",
 *         "version": "1.0"
 *       },
 *       "steps": [
 *         {
 *           "step_id": "1",
 *           "transform_name": "CalculateMaskArea",
 *           "input_key": "mask_data",
 *           "output_key": "areas",
 *           "parameters": {}
 *         }
 *       ]
 *     }
 *   }
 * ]
 * ```
 * 
 * @param dm Pointer to the DataManager
 * @param json_filepath Path to the JSON configuration file
 * @return std::vector<DataInfo> List of loaded/transformed data info
 */
std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        std::string const & json_filepath);

/**
 * @brief Load and execute transformation pipeline from JSON object (V2 version)
 */
std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        nlohmann::json const & j,
        std::string const & base_path);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_DATAMANAGER_INTEGRATION_HPP
