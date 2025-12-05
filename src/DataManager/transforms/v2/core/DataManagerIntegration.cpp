#include "DataManagerIntegration.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// DataManagerPipelineExecutor Implementation
// ============================================================================

DataManagerPipelineExecutor::DataManagerPipelineExecutor(DataManager * data_manager)
    : data_manager_(data_manager) {
    if (!data_manager_) {
        throw std::invalid_argument("DataManager cannot be null");
    }
}

bool DataManagerPipelineExecutor::loadFromJson(nlohmann::json const & json_config) {
    try {
        clear();
        return parseJsonFormat(json_config);
    } catch (std::exception const & e) {
        std::cerr << "Error loading V2 pipeline from JSON: " << e.what() << std::endl;
        return false;
    }
}

bool DataManagerPipelineExecutor::loadFromJsonFile(std::string const & json_file_path) {
    try {
        std::ifstream file(json_file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open JSON file: " << json_file_path << std::endl;
            return false;
        }

        nlohmann::json json_config;
        file >> json_config;
        return loadFromJson(json_config);
    } catch (std::exception const & e) {
        std::cerr << "Error loading V2 pipeline from file: " << e.what() << std::endl;
        return false;
    }
}

bool DataManagerPipelineExecutor::parseJsonFormat(nlohmann::json const & json_config) {
    // Handle the nested "steps" format (V1-compatible)
    if (json_config.contains("steps") && json_config["steps"].is_array()) {
        // Parse metadata if present
        if (json_config.contains("metadata") && json_config["metadata"].is_object()) {
            auto const & meta_json = json_config["metadata"];
            DataManagerPipelineMetadata meta;
            if (meta_json.contains("name"))
                meta.name = meta_json["name"].get<std::string>();
            if (meta_json.contains("description"))
                meta.description = meta_json["description"].get<std::string>();
            if (meta_json.contains("version"))
                meta.version = meta_json["version"].get<std::string>();
            if (meta_json.contains("author"))
                meta.author = meta_json["author"].get<std::string>();
            metadata_ = meta;
        }

        // Parse steps
        for (auto const & step_json: json_config["steps"]) {
            DataManagerStepDescriptor step;

            // Required fields
            if (!step_json.contains("step_id") || !step_json["step_id"].is_string()) {
                std::cerr << "Step missing required 'step_id' field" << std::endl;
                return false;
            }
            step.step_id = step_json["step_id"].get<std::string>();

            if (!step_json.contains("transform_name") || !step_json["transform_name"].is_string()) {
                std::cerr << "Step '" << step.step_id << "' missing required 'transform_name' field" << std::endl;
                return false;
            }
            step.transform_name = step_json["transform_name"].get<std::string>();

            if (!step_json.contains("input_key") || !step_json["input_key"].is_string()) {
                std::cerr << "Step '" << step.step_id << "' missing required 'input_key' field" << std::endl;
                return false;
            }
            step.input_key = step_json["input_key"].get<std::string>();

            // Optional fields
            if (step_json.contains("output_key") && step_json["output_key"].is_string()) {
                step.output_key = step_json["output_key"].get<std::string>();
            }

            if (step_json.contains("parameters") && step_json["parameters"].is_object()) {
                // Convert nlohmann::json to rfl::Generic
                std::string params_str = step_json["parameters"].dump();
                auto generic_result = rfl::json::read<rfl::Generic>(params_str);
                if (generic_result) {
                    step.parameters = generic_result.value();
                }
            }

            if (step_json.contains("description") && step_json["description"].is_string()) {
                step.description = step_json["description"].get<std::string>();
            }

            if (step_json.contains("enabled") && step_json["enabled"].is_boolean()) {
                step.enabled = step_json["enabled"].get<bool>();
            }

            if (step_json.contains("phase") && step_json["phase"].is_number_integer()) {
                step.phase = step_json["phase"].get<int>();
            }

            steps_.push_back(std::move(step));
        }

        return true;
    }

    std::cerr << "V2 Pipeline JSON must contain a 'steps' array" << std::endl;
    return false;
}

V2PipelineResult DataManagerPipelineExecutor::execute(V2PipelineProgressCallback progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();

    V2PipelineResult result;
    result.total_steps = static_cast<int>(steps_.size());
    result.step_results.reserve(steps_.size());

    try {
        // Clear temporary data from previous executions
        temporary_data_.clear();

        for (size_t i = 0; i < steps_.size(); ++i) {
            auto const & step = steps_[i];

            // Check if step is enabled
            if (step.enabled.has_value() && !step.enabled.value()) {
                continue;
            }

            // Report progress
            if (progress_callback) {
                int overall_progress = static_cast<int>((i * 100) / steps_.size());
                progress_callback(static_cast<int>(i), step.transform_name, 0, overall_progress);
            }

            // Execute this step
            auto step_result = executeStep(i);
            result.step_results.push_back(step_result);

            if (!step_result.success) {
                result.success = false;
                result.error_message = "Step '" + step.step_id + "' failed: " + step_result.error_message;
                break;
            }

            result.steps_completed++;
        }

        if (result.steps_completed == result.total_steps) {
            result.success = true;
        }

        // Report completion
        if (progress_callback) {
            progress_callback(result.total_steps - 1, "Complete", 100, 100);
        }

    } catch (std::exception const & e) {
        result.success = false;
        result.error_message = std::string("Exception during pipeline execution: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_execution_time_ms =
            std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

V2StepResult DataManagerPipelineExecutor::executeStep(
        size_t step_index,
        std::function<void(int)> progress_callback) {
    auto start_time = std::chrono::high_resolution_clock::now();

    V2StepResult result;

    if (step_index >= steps_.size()) {
        result.success = false;
        result.error_message = "Step index out of range";
        return result;
    }

    auto const & step = steps_[step_index];
    result.output_key = step.output_key.value_or("");

    try {
        // 1. Get input data from DataManager or temporary storage
        auto input_data = getInputData(step.input_key);
        if (!input_data.has_value()) {
            result.success = false;
            result.error_message = "Input data '" + step.input_key + "' not found in DataManager";
            return result;
        }

        // 2. Execute the transform using V2 system
        auto output_data = executeTransform(step.transform_name, input_data.value(), step.parameters);
        if (!output_data.has_value()) {
            result.success = false;
            result.error_message = "Transform '" + step.transform_name + "' failed to execute";
            return result;
        }

        // 3. Store output data
        if (step.output_key.has_value() && !step.output_key.value().empty()) {
            storeOutputData(step.output_key.value(), output_data.value(), step.step_id);
        } else {
            // Store as temporary data using step_id as key
            temporary_data_[step.step_id] = output_data.value();
        }

        result.success = true;

    } catch (std::exception const & e) {
        result.success = false;
        result.error_message = std::string("Exception: ") + e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

std::vector<std::string> DataManagerPipelineExecutor::validate() const {
    std::vector<std::string> errors;

    auto & registry = ElementRegistry::instance();

    for (size_t i = 0; i < steps_.size(); ++i) {
        auto const & step = steps_[i];

        // Check transform exists
        if (!registry.hasTransform(step.transform_name)) {
            errors.push_back("Step " + std::to_string(i) + " ('" + step.step_id +
                             "'): Transform '" + step.transform_name + "' not found in V2 registry");
        }

        // Check input_key is not empty
        if (step.input_key.empty()) {
            errors.push_back("Step " + std::to_string(i) + " ('" + step.step_id +
                             "'): input_key is empty");
        }
    }

    return errors;
}

void DataManagerPipelineExecutor::clear() {
    steps_.clear();
    metadata_.reset();
    temporary_data_.clear();
}

std::optional<DataTypeVariant> DataManagerPipelineExecutor::getInputData(std::string const & input_key) {
    // First check temporary data
    auto temp_it = temporary_data_.find(input_key);
    if (temp_it != temporary_data_.end()) {
        return temp_it->second;
    }

    // Then check DataManager
    auto data_variant = data_manager_->getDataVariant(input_key);
    if (data_variant.has_value()) {
        return data_variant.value();
    }

    return std::nullopt;
}

void DataManagerPipelineExecutor::storeOutputData(
        std::string const & output_key,
        DataTypeVariant const & data,
        std::string const & step_id) {
    // Get the default TimeKey from the DataManager
    TimeKey time_key("default");

    // Visit the variant and store in DataManager
    std::visit([this, &output_key, &time_key](auto const & data_ptr) {
        if (data_ptr) {
            using T = typename std::remove_reference_t<decltype(*data_ptr)>;
            data_manager_->setData<T>(output_key, data_ptr, time_key);
        }
    },
               data);
}

std::optional<DataTypeVariant> DataManagerPipelineExecutor::executeTransform(
        std::string const & transform_name,
        DataTypeVariant const & input_data,
        std::optional<rfl::Generic> const & parameters) {

    auto & registry = ElementRegistry::instance();

    // Get transform metadata
    auto const * meta = registry.getMetadata(transform_name);
    if (!meta) {
        std::cerr << "Transform '" << transform_name << "' not found in V2 registry" << std::endl;
        return std::nullopt;
    }

    // Build a single-step pipeline
    TransformPipeline pipeline;

    // Load parameters if provided
    if (parameters.has_value()) {
        std::string params_json = rfl::json::write(parameters.value());
        auto params_any = Examples::loadParametersForTransform(transform_name, params_json);
        if (!params_any.has_value()) {
            std::cerr << "Failed to load parameters for transform '" << transform_name << "'" << std::endl;
            return std::nullopt;
        }

        // Create pipeline step using factory
        try {
            auto step = Examples::createPipelineStepFromRegistry(registry, transform_name, params_any);
            pipeline.addStep(std::move(step));
        } catch (std::exception const & e) {
            std::cerr << "Failed to create pipeline step: " << e.what() << std::endl;
            return std::nullopt;
        }
    } else {
        // No parameters - use default
        pipeline.addStep(transform_name);
    }

    // Execute pipeline on the input data
    try {
        return executePipeline(input_data, pipeline);
    } catch (std::exception const & e) {
        std::cerr << "Pipeline execution failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// ============================================================================
// V2 Load Data From JSON Config
// ============================================================================

std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        std::string const & json_filepath) {
    // Open JSON file
    std::ifstream ifs(json_filepath);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open JSON file: " << json_filepath << std::endl;
        return {};
    }

    // Parse JSON
    nlohmann::json j;
    ifs >> j;

    // Get base path of filepath
    std::string const base_path = std::filesystem::path(json_filepath).parent_path().string();
    return load_data_from_json_config_v2(dm, j, base_path);
}

std::vector<DataInfo> load_data_from_json_config_v2(
        DataManager * dm,
        nlohmann::json const & j,
        std::string const & base_path) {
    std::vector<DataInfo> data_info_list;

    // Process transformation objects in the JSON array
    // This matches the V1 format: an array with objects containing "transformations"
    for (auto const & item: j) {
        if (item.contains("transformations")) {
            std::cout << "[V2] Found transformations section, executing pipeline..." << std::endl;

            try {
                // Create executor with DataManager
                DataManagerPipelineExecutor executor(dm);

                // Load the pipeline configuration from JSON
                if (!executor.loadFromJson(item["transformations"])) {
                    std::cerr << "[V2] Failed to load pipeline configuration from JSON" << std::endl;
                    continue;
                }

                // Validate the pipeline
                auto errors = executor.validate();
                if (!errors.empty()) {
                    std::cerr << "[V2] Pipeline validation errors:" << std::endl;
                    for (auto const & error: errors) {
                        std::cerr << "  - " << error << std::endl;
                    }
                    continue;
                }

                // Execute the pipeline with progress callback
                auto result = executor.execute([](int step_index, std::string const & step_name, int step_progress, int overall_progress) {
                    std::cout << "[V2] Step " << step_index << " ('" << step_name << "'): "
                              << step_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
                });

                if (result.success) {
                    std::cout << "[V2] Pipeline executed successfully!" << std::endl;
                    std::cout << "[V2] Steps completed: " << result.steps_completed << "/" << result.total_steps << std::endl;
                    std::cout << "[V2] Total execution time: " << result.total_execution_time_ms << " ms" << std::endl;

                    // Add output keys to data_info_list
                    for (auto const & step: executor.getSteps()) {
                        if (step.output_key.has_value() && !step.output_key.value().empty()) {
                            data_info_list.push_back({step.output_key.value(), "V2Transform", ""});
                        }
                    }
                } else {
                    std::cerr << "[V2] Pipeline execution failed: " << result.error_message << std::endl;
                }

            } catch (std::exception const & e) {
                std::cerr << "[V2] Exception during pipeline execution: " << e.what() << std::endl;
            }
        }
    }

    return data_info_list;
}

}// namespace WhiskerToolbox::Transforms::V2
