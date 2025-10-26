#include "TransformPipeline.hpp"

#include "DataManager.hpp"
#include "ParameterFactory.hpp"
#include "TransformRegistry.hpp"
#include "transforms/Lines/Line_Proximity_Grouping/line_proximity_grouping.hpp"
#include "transforms/grouping_transforms.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <future>
#include <type_traits>

TransformPipeline::TransformPipeline(DataManager* data_manager, TransformRegistry* registry)
    : data_manager_(data_manager), registry_(registry) {
    if (!data_manager_) {
        throw std::invalid_argument("DataManager cannot be null");
    }
    if (!registry_) {
        throw std::invalid_argument("TransformRegistry cannot be null");
    }
    auto& factory = ParameterFactory::getInstance();
    factory.initializeDefaultSetters();

}

bool TransformPipeline::loadFromJson(nlohmann::json const& json_config) {
    try {
        clear();
        
        // Load metadata
        if (json_config.contains("metadata")) {
            metadata_ = json_config["metadata"];
        }
        
        // Extract variables from metadata for substitution
        auto variables = extractVariables();
        
        // Create a mutable copy of the config for variable substitution
        nlohmann::json processed_config = json_config;
        
        // Perform variable substitution on the entire config
        // (but skip the metadata.variables section itself to avoid circular substitution)
        if (processed_config.contains("steps")) {
            substituteVariables(processed_config["steps"], variables);
        }
        
        // Load steps
        if (!processed_config.contains("steps") || !processed_config["steps"].is_array()) {
            std::cerr << "Pipeline JSON must contain a 'steps' array" << std::endl;
            return false;
        }
        
        auto const& steps_json = processed_config["steps"];
        steps_.reserve(steps_json.size());
        
        for (size_t i = 0; i < steps_json.size(); ++i) {
            auto [success, step] = parseStep(steps_json[i], static_cast<int>(i));
            if (!success) {
                std::cerr << "Failed to parse step " << i << std::endl;
                return false;
            }
            steps_.push_back(std::move(step));
        }
        
        // Validate the loaded pipeline
        auto validation_errors = validate();
        if (!validation_errors.empty()) {
            std::cerr << "Pipeline validation failed:" << std::endl;
            for (auto const& error : validation_errors) {
                std::cerr << "  - " << error << std::endl;
            }
            return false;
        }
        
        return true;
    } catch (std::exception const& e) {
        std::cerr << "Error loading pipeline from JSON: " << e.what() << std::endl;
        return false;
    }
}

bool TransformPipeline::loadFromJsonFile(std::string const& json_file_path) {
    try {
        std::ifstream file(json_file_path);
        if (!file.is_open()) {
            std::cerr << "Cannot open pipeline file: " << json_file_path << std::endl;
            return false;
        }
        
        nlohmann::json json_config;
        file >> json_config;
        return loadFromJson(json_config);
    } catch (std::exception const& e) {
        std::cerr << "Error reading pipeline file '" << json_file_path << "': " << e.what() << std::endl;
        return false;
    }
}

PipelineResult TransformPipeline::execute(PipelineProgressCallback progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PipelineResult result;
    result.total_steps = static_cast<int>(steps_.size());
    result.step_results.reserve(steps_.size());
    
    try {
        // Clear temporary data from previous executions
        temporary_data_.clear();
        
        // Group steps by phase
        auto phase_groups = groupStepsByPhase();
        
        int completed_steps = 0;
        
        // Execute each phase
        for (auto const& [phase_number, step_indices] : phase_groups) {
            if (progress_callback) {
                progress_callback(-1, "Starting phase " + std::to_string(phase_number), 0, 
                                (completed_steps * 100) / result.total_steps);
            }
            
            auto phase_results = executePhase(step_indices, phase_number, progress_callback);
            
            // Check if any step in this phase failed
            bool phase_failed = false;
            for (auto const& step_result : phase_results) {
                result.step_results.push_back(step_result);
                if (!step_result.success) {
                    phase_failed = true;
                    result.error_message = "Step failed: " + step_result.error_message;
                    break;
                }
                completed_steps++;
            }
            
            if (phase_failed) {
                result.success = false;
                result.steps_completed = completed_steps;
                break;
            }
        }
        
        if (result.step_results.size() == steps_.size()) {
            result.success = true;
            result.steps_completed = result.total_steps;
            
            if (progress_callback) {
                progress_callback(-1, "Pipeline completed", 100, 100);
            }
        }
        
    } catch (std::exception const& e) {
        result.success = false;
        result.error_message = "Pipeline execution error: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    return result;
}

StepResult TransformPipeline::executeStep(PipelineStep const& step, ProgressCallback progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    StepResult result;
    result.output_key = step.output_key;
    
    try {
        // Check if step is enabled
        if (!step.enabled) {
            result.success = true; // Disabled steps are considered successful
            return result;
        }
        
        // Get the transform operation
        auto* operation = registry_->findOperationByName(step.transform_name);
        if (!operation) {
            result.error_message = "Transform '" + step.transform_name + "' not found in registry";
            return result;
        }
        
        // Get input data
        auto [input_success, input_data] = getInputData(step.input_key);
        if (!input_success) {
            result.error_message = "Failed to get input data for key '" + step.input_key + "'";
            return result;
        }
        
        // Check if operation can apply to input data
        if (!operation->canApply(input_data)) {
            result.error_message = "Transform '" + step.transform_name + "' cannot be applied to input data";
            return result;
        }
        
        // Create parameters
        auto parameters = createParametersFromJson(step.transform_name, step.parameters);
        
        // Execute the transform
        DataTypeVariant output_data;
        if (progress_callback) {
            output_data = operation->execute(input_data, parameters.get(), progress_callback);
        } else {
            output_data = operation->execute(input_data, parameters.get());
        }
        
        // Check if execution was successful (assuming empty variant means failure)
        if (std::visit([](auto const& ptr) { return ptr == nullptr; }, output_data)) {
            result.error_message = "Transform execution returned null result";
            return result;
        }
        
        // Store output data
        auto time_key = data_manager_->getTimeKey(step.input_key);
        storeOutputData(step.output_key, output_data, step.step_id, time_key);
        result.result_data = output_data;
        result.success = true;
        
    } catch (std::exception const& e) {
        result.error_message = "Step execution error: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    return result;
}

std::vector<std::string> TransformPipeline::validate() const {
    std::vector<std::string> errors;
    
    // Check for duplicate step IDs
    std::unordered_map<std::string, int> step_id_counts;
    for (auto const& step : steps_) {
        step_id_counts[step.step_id]++;
    }
    for (auto const& [step_id, count] : step_id_counts) {
        if (count > 1) {
            errors.push_back("Duplicate step ID: " + step_id);
        }
    }
    
    // Validate each step
    for (size_t i = 0; i < steps_.size(); ++i) {
        auto const& step = steps_[i];
        std::string step_prefix = "Step " + std::to_string(i) + " (" + step.step_id + "): ";
        
        // Check if transform exists
        if (!registry_->findOperationByName(step.transform_name)) {
            errors.push_back(step_prefix + "Transform '" + step.transform_name + "' not found in registry");
        }
        
        // Check input key
        if (step.input_key.empty()) {
            errors.push_back(step_prefix + "Input key cannot be empty");
        }
        
        // Check step ID
        if (step.step_id.empty()) {
            errors.push_back(step_prefix + "Step ID cannot be empty");
        }
        
        // Check phase number
        if (step.phase < 0) {
            errors.push_back(step_prefix + "Phase number cannot be negative");
        }
    }
    
    return errors;
}

void TransformPipeline::clear() {
    steps_.clear();
    metadata_ = nlohmann::json::object();
    temporary_data_.clear();
}

nlohmann::json TransformPipeline::exportToJson() const {
    nlohmann::json result;
    
    // Export metadata
    result["metadata"] = metadata_;
    
    // Export steps
    result["steps"] = nlohmann::json::array();
    for (auto const& step : steps_) {
        nlohmann::json step_json;
        step_json["step_id"] = step.step_id;
        step_json["transform_name"] = step.transform_name;
        step_json["input_key"] = step.input_key;
        step_json["output_key"] = step.output_key;
        step_json["parameters"] = step.parameters;
        step_json["phase"] = step.phase;
        step_json["enabled"] = step.enabled;
        
        if (!step.description.empty()) {
            step_json["description"] = step.description;
        }
        if (!step.tags.empty()) {
            step_json["tags"] = step.tags;
        }
        
        result["steps"].push_back(step_json);
    }
    
    return result;
}

bool TransformPipeline::saveToJsonFile(std::string const& json_file_path) const {
    try {
        std::ofstream file(json_file_path);
        if (!file.is_open()) {
            std::cerr << "Cannot create pipeline file: " << json_file_path << std::endl;
            return false;
        }
        
        auto json_data = exportToJson();
        file << json_data.dump(2); // Pretty print with 2-space indentation
        return true;
    } catch (std::exception const& e) {
        std::cerr << "Error saving pipeline file '" << json_file_path << "': " << e.what() << std::endl;
        return false;
    }
}

// Private implementation methods

std::pair<bool, PipelineStep> TransformPipeline::parseStep(nlohmann::json const& step_json, int step_index) {
    PipelineStep step;
    
    try {
        // Required fields
        if (!step_json.contains("step_id") || !step_json["step_id"].is_string()) {
            std::cerr << "Step " << step_index << ": 'step_id' is required and must be a string" << std::endl;
            return {false, step};
        }
        step.step_id = step_json["step_id"];
        
        if (!step_json.contains("transform_name") || !step_json["transform_name"].is_string()) {
            std::cerr << "Step " << step_index << ": 'transform_name' is required and must be a string" << std::endl;
            return {false, step};
        }
        step.transform_name = step_json["transform_name"];
        
        if (!step_json.contains("input_key") || !step_json["input_key"].is_string()) {
            std::cerr << "Step " << step_index << ": 'input_key' is required and must be a string" << std::endl;
            return {false, step};
        }
        step.input_key = step_json["input_key"];
        
        // Optional fields
        if (step_json.contains("output_key")) {
            if (step_json["output_key"].is_string()) {
                step.output_key = step_json["output_key"];
            }
        }
        
        if (step_json.contains("parameters")) {
            step.parameters = step_json["parameters"];
        } else {
            step.parameters = nlohmann::json::object();
        }
        
        if (step_json.contains("phase")) {
            if (step_json["phase"].is_number_integer()) {
                step.phase = step_json["phase"];
            }
        }
        
        if (step_json.contains("enabled")) {
            if (step_json["enabled"].is_boolean()) {
                step.enabled = step_json["enabled"];
            }
        }
        
        if (step_json.contains("description")) {
            if (step_json["description"].is_string()) {
                step.description = step_json["description"];
            }
        }
        
        if (step_json.contains("tags")) {
            if (step_json["tags"].is_array()) {
                for (auto const& tag : step_json["tags"]) {
                    if (tag.is_string()) {
                        step.tags.push_back(tag);
                    }
                }
            }
        }
        
        // Validate parameters by attempting to create them (only if operation exists)
        if (!step.parameters.empty()) {
            auto* operation = registry_->findOperationByName(step.transform_name);
            if (operation) {
                // Only validate if operation is registered
                auto test_params = createParametersFromJson(step.transform_name, step.parameters);
                if (!test_params) {
                    std::cerr << "Step " << step_index << " (" << step.step_id 
                              << "): Failed to create valid parameters" << std::endl;
                    return {false, step};
                }
            }
            // If operation not found, validation will happen later in the validate() method
        }
        
        return {true, step};
    } catch (std::exception const& e) {
        std::cerr << "Error parsing step " << step_index << ": " << e.what() << std::endl;
        return {false, step};
    }
}

std::unique_ptr<TransformParametersBase> TransformPipeline::createParametersFromJson(
    std::string const& transform_name, 
    nlohmann::json const& param_json) {
    
    // Get the operation to get default parameters
    auto* operation = registry_->findOperationByName(transform_name);
    if (!operation) {
        return nullptr;
    }

    // Special handling for grouping operations that need EntityGroupManager
    if (transform_name == "Group Lines by Proximity") {
        auto* group_manager = data_manager_->getEntityGroupManager();
        if (!group_manager) {
            std::cerr << "Error: EntityGroupManager not available for grouping operation" << std::endl;
            return nullptr;
        }
        
        auto parameters = std::make_unique<LineProximityGroupingParameters>(group_manager);
        
        // Set parameters from JSON
        for (auto const& [param_name, param_value] : param_json.items()) {
            if (!setParameterValue(parameters.get(), param_name, param_value, transform_name)) {
                std::cerr << "Warning: Failed to set parameter '" << param_name 
                          << "' for transform '" << transform_name << "'" << std::endl;
            }
        }
        
        return std::move(parameters);
    }
    
    // Get default parameters first
    auto parameters = operation->getDefaultParameters();
    if (!parameters) {
        return nullptr;
    }
    
    // Check if this is a grouping operation that needs EntityGroupManager
    auto* grouping_params = dynamic_cast<GroupingTransformParametersBase*>(parameters.get());
    if (grouping_params) {
        auto* group_manager = data_manager_->getEntityGroupManager();
        if (!group_manager) {
            std::cerr << "Error: EntityGroupManager not available for grouping operation '" 
                      << transform_name << "'" << std::endl;
            return nullptr;
        }
        
        // Set the group manager
        grouping_params->setGroupManager(group_manager);
    }
    
    // Set parameters from JSON
    for (auto const& [param_name, param_value] : param_json.items()) {
        if (!setParameterValue(parameters.get(), param_name, param_value, transform_name)) {
            std::cerr << "Error: Failed to set parameter '" << param_name 
                      << "' for transform '" << transform_name << "'" << std::endl;
            return nullptr;  // Fail the pipeline loading if parameter setting fails
        }
    }
    
    return parameters;
}

bool TransformPipeline::setParameterValue(TransformParametersBase* param_obj, 
                                         std::string const& param_name, 
                                         nlohmann::json const& json_value,
                                         std::string const& transform_name) {
    
    // Use the parameter factory for type conversion
    auto& factory = ParameterFactory::getInstance();
    
    return factory.setParameter(transform_name, param_obj, param_name, json_value, data_manager_);
}

std::pair<bool, DataTypeVariant> TransformPipeline::getInputData(std::string const& input_key) {
    // First check temporary data
    auto temp_it = temporary_data_.find(input_key);
    if (temp_it != temporary_data_.end()) {
        return {true, temp_it->second};
    }
    
    // Then check data manager
    auto data_variant = data_manager_->getDataVariant(input_key);
    if (data_variant.has_value()) {
        return {true, data_variant.value()};
    }
    
    return {false, DataTypeVariant{}};
}

void TransformPipeline::storeOutputData(std::string const& output_key, 
                                       DataTypeVariant const& data, 
                                       std::string const& step_id,
                                       TimeKey const& time_key) {
    if (output_key.empty()) {
        // Store as temporary data using step_id
        temporary_data_[step_id + "_output"] = data;
    } else {
        // Store in data manager
        data_manager_->setData(output_key, data, time_key);
    }
}

std::map<int, std::vector<int>> TransformPipeline::groupStepsByPhase() const {
    std::map<int, std::vector<int>> phase_groups;
    
    for (size_t i = 0; i < steps_.size(); ++i) {
        phase_groups[steps_[i].phase].push_back(static_cast<int>(i));
    }
    
    return phase_groups;
}

std::vector<StepResult> TransformPipeline::executePhase(
    std::vector<int> const& phase_steps, 
    int,
    PipelineProgressCallback progress_callback) {
    
    std::vector<StepResult> results;
    results.reserve(phase_steps.size());
    
    if (phase_steps.size() == 1) {
        // Single step - execute directly
        int step_index = phase_steps[0];
        auto const& step = steps_[static_cast<size_t>(step_index)];
        
        ProgressCallback step_progress_callback;
        if (progress_callback) {
            step_progress_callback = [this, progress_callback, step_index, step](int step_progress) {
                int overall_progress = (step_index * 100) / static_cast<int>(steps_.size());
                progress_callback(step_index, step.step_id, step_progress, overall_progress);
            };
        }
        
        results.push_back(executeStep(step, step_progress_callback));
    } else {
        // Multiple steps - execute in parallel
        std::vector<std::future<StepResult>> futures;
        futures.reserve(phase_steps.size());
        
        for (int step_index : phase_steps) {
            auto const& step = steps_[static_cast<size_t>(step_index)];
            
            ProgressCallback step_progress_callback;
            if (progress_callback) {
                step_progress_callback = [this, progress_callback, step_index, step](int step_progress) {
                    int overall_progress = (step_index * 100) / static_cast<int>(steps_.size());
                    progress_callback(step_index, step.step_id, step_progress, overall_progress);
                };
            }
            
            futures.push_back(std::async(std::launch::async, 
                [this, step, step_progress_callback]() {
                    return executeStep(step, step_progress_callback);
                }));
        }
        
        // Collect results
        for (auto& future : futures) {
            results.push_back(future.get());
        }
    }
    
    return results;
}

std::unordered_map<std::string, std::string> TransformPipeline::extractVariables() const {
    std::unordered_map<std::string, std::string> variables;
    
    if (metadata_.contains("variables") && metadata_["variables"].is_object()) {
        for (auto const& [key, value] : metadata_["variables"].items()) {
            if (value.is_string()) {
                variables[key] = value.get<std::string>();
            } else if (value.is_number()) {
                variables[key] = std::to_string(value.get<double>());
            } else if (value.is_boolean()) {
                variables[key] = value.get<bool>() ? "true" : "false";
            }
        }
    }
    
    return variables;
}

void TransformPipeline::substituteVariables(nlohmann::json& json, 
                                            std::unordered_map<std::string, std::string> const& variables) const {
    if (json.is_string()) {
        std::string str = json.get<std::string>();
        
        // Replace all ${variable_name} patterns
        size_t pos = 0;
        while ((pos = str.find("${", pos)) != std::string::npos) {
            size_t end_pos = str.find("}", pos + 2);
            if (end_pos == std::string::npos) {
                // Malformed variable reference, skip it
                pos += 2;
                continue;
            }
            
            std::string var_name = str.substr(pos + 2, end_pos - pos - 2);
            auto it = variables.find(var_name);
            
            if (it != variables.end()) {
                // Replace the ${var_name} with the value
                str.replace(pos, end_pos - pos + 1, it->second);
                pos += it->second.length();
            } else {
                // Variable not found, leave it as is and move past it
                std::cerr << "Warning: Variable '${" << var_name << "}' not found in metadata.variables" << std::endl;
                pos = end_pos + 1;
            }
        }
        
        json = str;
    } else if (json.is_array()) {
        for (auto& element : json) {
            substituteVariables(element, variables);
        }
    } else if (json.is_object()) {
        for (auto& [key, value] : json.items()) {
            substituteVariables(value, variables);
        }
    }
}
