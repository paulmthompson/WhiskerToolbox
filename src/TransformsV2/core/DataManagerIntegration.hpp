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
     * @param data_manager Non-owning pointer to the DataManager; the executor stores
     *                     this pointer but does not take ownership or delete the object.
     *
     * @pre data_manager must not be null (enforcement: exception)
     * @pre The DataManager object pointed to by data_manager must remain valid and
     *      not be destroyed for the entire lifetime of this executor
     *      (enforcement: none) [IMPORTANT]
     */
    explicit DataManagerPipelineExecutor(DataManager * data_manager);

    /**
     * @brief Load pipeline configuration from JSON object
     *
     * Any previously loaded configuration is cleared before parsing begins.
     * Any valid nlohmann::json value is accepted; values that do not conform
     * to the expected schema cause the function to return false rather than throw.
     * All exceptions from parsing are caught internally and result in a false return.
     *
     * @param json_config JSON value to parse. No structural precondition is imposed
     *                    by this function — schema validation is performed internally
     *                    and failure is reported via the return value.
     * @return true if the complete configuration was loaded successfully.
     *
     * @post If returns true, getSteps() returns the complete parsed pipeline
     *       (enforcement: runtime_check — parseJsonFormat validates all required fields).
     * @post If returns false, steps_ may be partially populated (steps before the
     *       first failing step are retained). The executor must not be used for
     *       execution without a subsequent successful loadFromJson() or clear() call
     *       (enforcement: none) [IMPORTANT]
     */
    bool loadFromJson(nlohmann::json const & json_config);

    /**
     * @brief Load pipeline configuration from JSON file
     *
     * Opens the file at @p json_file_path, parses its contents as JSON, and
     * delegates to loadFromJson(). Any previously loaded configuration is
     * cleared before parsing begins. All I/O and parse errors are caught
     * internally and result in a false return rather than a throw.
     *
     * @param json_file_path Path to a readable file containing a valid JSON
     *                       pipeline configuration.
     * @return true if the file was opened, parsed, and the pipeline loaded
     *         successfully.
     *
     * @pre json_file_path must not be empty (enforcement: runtime_check —
     *      empty string causes ifstream::open to fail, triggering the
     *      is_open() guard)
     * @pre The file at json_file_path must exist and be readable by the
     *      current process (enforcement: runtime_check — guarded by
     *      !file.is_open() early return)
     * @pre The file content must be valid JSON (enforcement: exception —
     *      nlohmann::json parse errors are caught and converted to false return)
     * @pre The parsed JSON must contain a "steps" array conforming to the
     *      pipeline schema (enforcement: runtime_check — propagated from
     *      loadFromJson/parseJsonFormat)
     *
     * @post If returns false, steps_ may be partially populated (same
     *       postcondition as loadFromJson). The executor must not be used for
     *       execution without a subsequent successful load or clear() call
     *       (enforcement: none) [IMPORTANT]
     */
    bool loadFromJsonFile(std::string const & json_file_path);

    /**
     * @brief Execute the loaded pipeline
     *
     * Iterates over all enabled steps, delegating each to executeStep().
     * Steps whose `enabled` field is explicitly set to false are skipped.
     * All exceptions from individual steps are caught internally; the result
     * struct carries success/failure state and per-step error messages.
     *
     * @param progress_callback Optional callable invoked before each step and
     *        once on completion. May be null/empty (tested before every call).
     *        If provided, must not capture references that may be invalidated
     *        during execution (enforcement: none) [LOW]
     * @return V2PipelineResult describing overall and per-step outcomes.
     *
     * @pre A successful loadFromJson() or loadFromJsonFile() call must have
     *      completed before calling execute(); executing a partially-loaded
     *      pipeline (from a failed load) produces partial or incorrect results
     *      (enforcement: none) [IMPORTANT]
     * @pre The DataManager passed to the constructor must still be valid
     *      (enforcement: none — propagated from constructor) [IMPORTANT]
     *
     * @post If steps_ is empty, returns success=true with steps_completed=0
     *       and total_steps=0 (empty pipeline is considered trivially successful).
     * @post If progress_callback is provided and steps_ is empty, it is
     *       invoked with step_index=-1 (completion sentinel). Callbacks should
     *       handle negative step_index values (enforcement: none) [LOW]
     */
    V2PipelineResult execute(V2PipelineProgressCallback progress_callback = nullptr);

    /**
     * @brief Execute a single step by index
     *
     * Retrieves the step at @p step_index, fetches its input data from the
     * DataManager or from temporary storage (outputs of prior steps), runs the
     * registered transform, and stores the result. All errors are returned in
     * the result struct rather than thrown.
     *
     * @note The @p progress_callback parameter is accepted but currently unused —
     *       it is never invoked inside this function. Per-step progress reporting
     *       is handled by the enclosing execute() call.
     *
     * @param step_index Zero-based index of the step to execute.
     * @param progress_callback Currently unused. Reserved for future per-step
     *        progress reporting.
     * @return V2StepResult describing success/failure and timing.
     *
     * @pre step_index must be less than steps_.size() (enforcement: runtime_check —
     *      guarded by bounds check returning failure result)
     * @pre steps_[step_index].input_key must name a key present in the DataManager
     *      or in temporary_data_ (for single-input steps) (enforcement: runtime_check —
     *      missing key is detected by getInputData() and reported as failure)
     * @pre steps_[step_index].transform_name must be registered in ElementRegistry
     *      (enforcement: runtime_check — unregistered name causes executeTransform()
     *      to return nullopt, reported as failure)
     * @pre The DataManager passed to the constructor must still be valid
     *      (enforcement: none — propagated from constructor) [IMPORTANT]
     * @pre For multi-input steps, all keys in additional_input_keys must also be
     *      present in DataManager or temporary_data_ (enforcement: runtime_check —
     *      propagated through executeMultiInputStep())
     */
    V2StepResult executeStep(size_t step_index, std::function<void(int)> progress_callback = nullptr);

    /**
     * @brief Validate the loaded pipeline configuration
     *
     * Checks each step against the ElementRegistry and verifies that required
     * fields are non-empty. Does not access the DataManager — input key
     * existence is not verified at this stage (that would require runtime data).
     * Does not throw; always returns a (possibly empty) vector.
     *
     * @return Empty vector if the pipeline is valid; otherwise one diagnostic
     *         string per error. An empty steps_ vector is considered valid and
     *         returns an empty vector.
     *
     * @pre Must be called after a successful loadFromJson() or loadFromJsonFile()
     *      to be meaningful. Calling on an empty or partially-loaded pipeline
     *      returns the errors for whatever steps_ currently contains
     *      (enforcement: none) [LOW]
     *
     * @post Return value is empty if and only if all of the following hold for
     *       every step: transform_name is registered in ElementRegistry, and
     *       input_key is non-empty (enforcement: runtime_check).
     * @post Does not modify any member state; safe to call multiple times
     *       (enforcement: static — declared const).
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
     *
     * Resets steps_, metadata_, and temporary_data_ to empty/default state.
     * Does not affect the DataManager pointer or any data stored in the
     * DataManager itself. Safe to call at any time, including on a
     * default-constructed or partially-loaded executor.
     *
     * @post steps_ is empty (enforcement: static — std::vector::clear() guarantee).
     * @post metadata_ has no value (enforcement: static — std::optional::reset() guarantee).
     * @post temporary_data_ is empty (enforcement: static — std::unordered_map::clear() guarantee).
     * @post data_manager_ is unchanged (enforcement: static — not touched by this function).
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
     *
     * @note This function only inspects the transform type of the step at
     *       @p step_index. It does NOT verify that a previous step exists or
     *       that the two steps are actually chained — callers must additionally
     *       call stepsAreChained() to confirm data dependency. buildSegments()
     *       handles both checks correctly.
     *
     * @note A step_index of 0 will return true for a fusible element transform
     *       even though there is no preceding step. Callers are responsible for
     *       not calling canFuseStep(0) as the first step in a segment
     *       (enforcement: none) [LOW]
     *
     * @param step_index Zero-based index of the step to test.
     * @return false if step_index is out of bounds, the step has multiple inputs,
     *         the transform_name is not registered, or the transform is a
     *         container or time-grouped transform. Returns true only for
     *         single-input element-level transforms.
     *
     * @pre step_index < steps_.size() is not required — out-of-bounds returns
     *      false (enforcement: runtime_check).
     * @pre steps_ must have been populated by a successful loadFromJson() call
     *      for the result to be meaningful (enforcement: none) [LOW]
     */
    bool canFuseStep(size_t step_index) const;

    /**
     * @brief Check if consecutive steps form a data dependency chain
     *
     * Returns true if step[i]'s input_key matches step[i-1]'s output_key
     * or step[i-1]'s step_id (for temporary data).
     *
     * The previous step's effective output key is:
     * - steps_[prev_step].output_key if set and non-empty, otherwise
     * - steps_[prev_step].step_id (temporary storage key convention).
     *
     * @param prev_step Zero-based index of the upstream step.
     * @param curr_step Zero-based index of the downstream step.
     * @return true if and only if both indices are in bounds, curr_step >
     *         prev_step, and steps_[curr_step].input_key equals the effective
     *         output key of steps_[prev_step].
     *
     * @pre Neither index is required to be in bounds — out-of-bounds returns
     *      false (enforcement: runtime_check).
     * @pre curr_step > prev_step is not required — equal or reversed indices
     *      return false (enforcement: runtime_check).
     * @pre steps_ must have been populated by a successful loadFromJson() call
     *      for the result to be meaningful (enforcement: none) [LOW]
     *
     * @post Does not modify any member state; safe to call multiple times
     *       (enforcement: static — declared const).
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
 * @param dm Non-owning pointer to the DataManager (caller retains ownership).
 * @param json_filepath Path to the JSON configuration file
 * @return std::vector<DataInfo> List of loaded/transformed data info
 *
 * @pre dm must not be null (enforcement: exception — propagated from
 *      DataManagerPipelineExecutor constructor)
 * @pre The DataManager object pointed to by dm must remain valid for the
 *      duration of this call (enforcement: none) [IMPORTANT]
 */
std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        std::string const & json_filepath);

/**
 * @brief Load and execute transformation pipeline from JSON object (V2 version)
 *
 * @param dm Non-owning pointer to the DataManager (caller retains ownership).
 * @param j Parsed JSON configuration object
 * @param base_path Base filesystem path for resolving relative paths in the config
 *
 * @pre dm must not be null (enforcement: exception — propagated from
 *      DataManagerPipelineExecutor constructor)
 * @pre The DataManager object pointed to by dm must remain valid for the
 *      duration of this call (enforcement: none) [IMPORTANT]
 */
std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        nlohmann::json const & j,
        std::string const & base_path);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_DATAMANAGER_INTEGRATION_HPP
