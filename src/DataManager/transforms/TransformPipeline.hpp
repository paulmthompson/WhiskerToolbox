#ifndef TRANSFORM_PIPELINE_HPP
#define TRANSFORM_PIPELINE_HPP

#include "TimeFrame/StrongTimeTypes.hpp"

#include "data_transforms.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class DataManager;
class TransformRegistry;

/**
 * @brief Progress callback for pipeline execution
 * 
 * @param step_index Current step being executed (0-based)
 * @param step_name Name of the current step
 * @param step_progress Progress of current step (0-100)
 * @param overall_progress Overall pipeline progress (0-100)
 */
using PipelineProgressCallback = std::function<void(int step_index, std::string const& step_name, int step_progress, int overall_progress)>;

/**
 * @brief Represents a single step in a transformation pipeline
 */
struct PipelineStep {
    std::string step_id;                    // Unique identifier for this step
    std::string transform_name;             // Name of the transform operation
    std::string input_key;                  // Input data key from DataManager
    std::string output_key;                 // Output data key (empty = temporary)
    nlohmann::json parameters;              // Parameters as JSON object
    int phase = 0;                          // Execution phase (0 = first)
    bool enabled = true;                    // Whether this step is enabled
    
    // Optional metadata
    std::string description;                // Human-readable description
    std::vector<std::string> tags;          // Tags for organization
};

/**
 * @brief Represents execution result of a pipeline step
 */
struct StepResult {
    bool success = false;
    std::string error_message;
    std::string output_key;                 // Key where result was stored (if any)
    DataTypeVariant result_data;            // The actual result data
    double execution_time_ms = 0.0;         // Execution time in milliseconds
};

/**
 * @brief Represents the complete pipeline execution result
 */
struct PipelineResult {
    bool success = false;
    std::string error_message;
    std::vector<StepResult> step_results;
    double total_execution_time_ms = 0.0;
    int steps_completed = 0;
    int total_steps = 0;
};

/**
 * @brief Factory and execution engine for transformation pipelines
 */
class TransformPipeline {
public:
    /**
     * @brief Construct a new Transform Pipeline object
     * 
     * @param data_manager Pointer to the data manager
     * @param registry Pointer to the transform registry
     */
    TransformPipeline(DataManager* data_manager, TransformRegistry* registry);

    /**
     * @brief Load pipeline configuration from JSON
     * 
     * @param json_config JSON configuration object
     * @return true if configuration was loaded successfully
     * @return false if there were errors in the configuration
     */
    bool loadFromJson(nlohmann::json const& json_config);

    /**
     * @brief Load pipeline configuration from JSON file
     * 
     * @param json_file_path Path to JSON configuration file
     * @return true if configuration was loaded successfully
     * @return false if file couldn't be read or parsed
     */
    bool loadFromJsonFile(std::string const& json_file_path);

    /**
     * @brief Execute the loaded pipeline
     * 
     * @param progress_callback Optional progress callback
     * @return PipelineResult containing execution results
     */
    PipelineResult execute(PipelineProgressCallback progress_callback = nullptr);

    /**
     * @brief Execute a single pipeline step
     * 
     * @param step The step to execute
     * @param progress_callback Optional progress callback for the step
     * @return StepResult containing execution result
     */
    StepResult executeStep(PipelineStep const& step, ProgressCallback progress_callback = nullptr);

    /**
     * @brief Validate the pipeline configuration
     * 
     * @return std::vector<std::string> List of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const;

    /**
     * @brief Get the loaded pipeline steps
     * 
     * @return std::vector<PipelineStep> const& Reference to the steps
     */
    std::vector<PipelineStep> const& getSteps() const { return steps_; }

    /**
     * @brief Get pipeline metadata
     * 
     * @return nlohmann::json const& Reference to metadata
     */
    nlohmann::json const& getMetadata() const { return metadata_; }

    /**
     * @brief Clear the current pipeline configuration
     */
    void clear();

    /**
     * @brief Export current pipeline to JSON
     * 
     * @return nlohmann::json JSON representation of the pipeline
     */
    nlohmann::json exportToJson() const;

    /**
     * @brief Save current pipeline to JSON file
     * 
     * @param json_file_path Path where to save the pipeline
     * @return true if saved successfully
     * @return false if there was an error saving
     */
    bool saveToJsonFile(std::string const& json_file_path) const;

private:
    DataManager* data_manager_;
    TransformRegistry* registry_;
    std::vector<PipelineStep> steps_;
    nlohmann::json metadata_;
    
    // Execution state
    std::map<std::string, DataTypeVariant> temporary_data_;
    
    /**
     * @brief Parse a single step from JSON
     * 
     * @param step_json JSON object representing the step
     * @param step_index Index of the step (for error reporting)
     * @return std::pair<bool, PipelineStep> Success flag and parsed step
     */
    std::pair<bool, PipelineStep> parseStep(nlohmann::json const& step_json, int step_index);
    
    /**
     * @brief Create parameters object from JSON for a specific transform
     * 
     * @param transform_name Name of the transform
     * @param param_json JSON parameters
     * @return std::unique_ptr<TransformParametersBase> Created parameters object
     */
    std::unique_ptr<TransformParametersBase> createParametersFromJson(
        std::string const& transform_name, 
        nlohmann::json const& param_json);
    
    /**
     * @brief Set a parameter value with automatic type conversion
     * 
     * @param param_obj Parameter object to modify
     * @param param_name Name of the parameter
     * @param json_value JSON value to set
     * @param transform_name Name of the transform (for parameter factory lookup)
     * @return true if parameter was set successfully
     * @return false if there was a type conversion error
     */
    bool setParameterValue(TransformParametersBase* param_obj, 
                          std::string const& param_name, 
                          nlohmann::json const& json_value,
                          std::string const& transform_name);
    
    /**
     * @brief Get input data for a step (from DataManager or temporary storage)
     * 
     * @param input_key Input key to retrieve
     * @return std::pair<bool, DataTypeVariant> Success flag and data variant
     */
    std::pair<bool, DataTypeVariant> getInputData(std::string const& input_key);
    
    /**
     * @brief Store output data (to DataManager or temporary storage)
     * 
     * @param output_key Output key (empty = temporary)
     * @param data Data to store
     * @param step_id Step ID for temporary key generation
     */
    void storeOutputData(std::string const& output_key, 
                        DataTypeVariant const& data, 
                        std::string const& step_id,
                        TimeKey const& time_key);
    
    /**
     * @brief Group steps by execution phase
     * 
     * @return std::unordered_map<int, std::vector<int>> Map from phase to step indices
     */
    std::map<int, std::vector<int>> groupStepsByPhase() const;
    
    /**
     * @brief Execute steps in a specific phase (potentially in parallel)
     * 
     * @param phase_steps Indices of steps to execute
     * @param phase_number Current phase number
     * @param progress_callback Pipeline progress callback
     * @return std::vector<StepResult> Results for each step
     */
    std::vector<StepResult> executePhase(
        std::vector<int> const& phase_steps, 
        int phase_number,
        PipelineProgressCallback progress_callback);
    
    /**
     * @brief Perform variable substitution on JSON using variables from metadata
     * 
     * Substitutes ${variable_name} patterns with values from metadata.variables
     * 
     * @param json The JSON object to process
     * @param variables Map of variable names to values
     */
    void substituteVariables(nlohmann::json& json, std::unordered_map<std::string, std::string> const& variables) const;
    
    /**
     * @brief Extract variables from metadata.variables section
     * 
     * @return std::unordered_map<std::string, std::string> Map of variable names to values
     */
    std::unordered_map<std::string, std::string> extractVariables() const;
};

#endif // TRANSFORM_PIPELINE_HPP
