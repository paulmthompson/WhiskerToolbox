#include "DataManagerIntegration.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"
#include "transforms/v2/algorithms/DigitalIntervalBoolean/DigitalIntervalBoolean.hpp"
#include "transforms/v2/core/ContainerTraits.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/FlatZipView.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <ranges>

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

            // Parse additional_input_keys for multi-input transforms
            if (step_json.contains("additional_input_keys") && step_json["additional_input_keys"].is_array()) {
                std::vector<std::string> additional_keys;
                for (auto const & key_json : step_json["additional_input_keys"]) {
                    if (key_json.is_string()) {
                        additional_keys.push_back(key_json.get<std::string>());
                    }
                }
                if (!additional_keys.empty()) {
                    step.additional_input_keys = std::move(additional_keys);
                }
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
        std::optional<DataTypeVariant> output_data;

        // Check if this is a multi-input step
        if (step.isMultiInput()) {
            // Use specialized multi-input execution path
            output_data = executeMultiInputStep(step_index);
            if (!output_data.has_value()) {
                result.success = false;
                result.error_message = "Multi-input transform '" + step.transform_name + "' failed to execute";
                return result;
            }
        } else {
            // Single-input execution path (existing logic)
            // 1. Get input data from DataManager or temporary storage
            auto input_data = getInputData(step.input_key);
            if (!input_data.has_value()) {
                result.success = false;
                result.error_message = "Input data '" + step.input_key + "' not found in DataManager";
                return result;
            }

            // 2. Execute the transform using V2 system
            output_data = executeTransform(step.transform_name, input_data.value(), step.parameters);
            if (!output_data.has_value()) {
                result.success = false;
                result.error_message = "Transform '" + step.transform_name + "' failed to execute";
                return result;
            }
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

    // Check if this is a container transform first
    if (registry.isContainerTransform(transform_name)) {
        return executeContainerTransformDynamic(transform_name, input_data, parameters);
    }

    // Get element transform metadata
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
        // No parameters provided - use default-constructed params with proper type
        // We need to deserialize "{}" to get the correctly typed default params
        auto params_any = Examples::loadParametersForTransform(transform_name, "{}");
        if (!params_any.has_value()) {
            std::cerr << "Failed to create default parameters for transform '" << transform_name << "'" << std::endl;
            return std::nullopt;
        }

        try {
            auto step = Examples::createPipelineStepFromRegistry(registry, transform_name, params_any);
            pipeline.addStep(std::move(step));
        } catch (std::exception const & e) {
            std::cerr << "Failed to create pipeline step with default params: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    // Execute pipeline on the input data
    try {
        return executePipeline(input_data, pipeline);
    } catch (std::exception const & e) {
        std::cerr << "Pipeline execution failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<DataTypeVariant> DataManagerPipelineExecutor::executeContainerTransformDynamic(
        std::string const & transform_name,
        DataTypeVariant const & input_data,
        std::optional<rfl::Generic> const & parameters) {

    auto & registry = ElementRegistry::instance();

    // Get container transform metadata
    auto const * meta = registry.getContainerMetadata(transform_name);
    if (!meta) {
        std::cerr << "Container transform '" << transform_name << "' metadata not found" << std::endl;
        return std::nullopt;
    }

    try {
        std::any params_any;
        
        // Load parameters if provided, otherwise use default
        if (parameters.has_value()) {
            std::string params_json = rfl::json::write(parameters.value());
            params_any = Examples::loadParametersForTransform(transform_name, params_json);
            if (!params_any.has_value()) {
                std::cerr << "Failed to load parameters for container transform '" << transform_name << "'" << std::endl;
                return std::nullopt;
            }
        } else {
            // Use the default parameter deserializer with empty JSON
            params_any = Examples::loadParametersForTransform(transform_name, "{}");
            if (!params_any.has_value()) {
                std::cerr << "Failed to create default parameters for container transform '" << transform_name << "'" << std::endl;
                return std::nullopt;
            }
        }

        // Execute using the registry's dynamic container execution
        ComputeContext ctx;
        auto result = registry.executeContainerTransformDynamic(transform_name, input_data, params_any, ctx);
        return result;

    } catch (std::exception const & e) {
        std::cerr << "Container transform execution failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// ============================================================================
// Multi-Input Pipeline Execution Helpers
// ============================================================================

namespace {

/**
 * @brief Helper to execute binary transform on two containers with elements()
 * 
 * This is only instantiated for container types that have ElementFor specializations.
 * Uses SFINAE to ensure proper compile-time dispatch.
 */
template<typename Container1, typename Container2>
auto executeBinaryTransformImpl(
    std::shared_ptr<Container1> const & data1_ptr,
    std::shared_ptr<Container2> const & data2_ptr,
    std::string const & transform_name,
    std::any const & params_any,
    std::vector<DataManagerStepDescriptor> const & steps,
    size_t step_index,
    DataManagerPipelineExecutor const & executor)
    -> std::enable_if_t<
        has_element_type_v<Container1> && has_element_type_v<Container2>,
        std::optional<DataTypeVariant>>
{
    auto & registry = ElementRegistry::instance();
    auto const & step = steps[step_index];
    
    using Element1 = ElementForSafe_t<Container1>;
    using Element2 = ElementForSafe_t<Container2>;
    using TupleType = std::tuple<Element1, Element2>;

    // Create zip view
    FlatZipView zip_view(data1_ptr->elements(), data2_ptr->elements());

    // Adapt to (time, tuple) format for pipeline
    auto pipeline_input = zip_view | std::views::transform([](auto const & triplet) {
        auto const & [time, e1, e2] = triplet;
        return std::make_pair(time, std::make_tuple(e1, e2));
    });

    // Build pipeline - include this step and any following fusible steps
    TransformPipeline pipeline;

    // Add current step
    try {
        auto pipeline_step = Examples::createPipelineStepFromRegistry(
            registry, transform_name, params_any);
        pipeline.addStep(std::move(pipeline_step));
    } catch (std::exception const & e) {
        std::cerr << "Failed to create pipeline step: " << e.what() << std::endl;
        return std::nullopt;
    }

    // Check if following steps can be fused (they use this step's output)
    for (size_t i = step_index + 1; i < steps.size(); ++i) {
        if (!executor.canFuseStep(i)) break;
        if (!executor.stepsAreChained(i - 1, i)) break;

        auto const & next_step = steps[i];
        std::any next_params;
        if (next_step.parameters.has_value()) {
            std::string params_json = rfl::json::write(next_step.parameters.value());
            next_params = Examples::loadParametersForTransform(next_step.transform_name, params_json);
        } else {
            next_params = Examples::loadParametersForTransform(next_step.transform_name, "{}");
        }

        if (!next_params.has_value()) break;

        try {
            auto next_pipeline_step = Examples::createPipelineStepFromRegistry(
                registry, next_step.transform_name, next_params);
            pipeline.addStep(std::move(next_pipeline_step));
        } catch (...) {
            break;
        }
    }

    // Execute the fused pipeline
    try {
        auto result_view = pipeline.executeFromView<TupleType>(pipeline_input);

        // Materialize results into output container
        // Determine output type from the last step's metadata
        auto last_step_idx = step_index + pipeline.size() - 1;
        auto const & last_step = steps[last_step_idx];
        auto const * meta = registry.getMetadata(last_step.transform_name);

        if (!meta) {
            std::cerr << "Transform metadata not found for " << last_step.transform_name << std::endl;
            return std::nullopt;
        }

        // Create output container based on output type
        if (meta->output_type == typeid(float)) {
            auto output = std::make_shared<RaggedAnalogTimeSeries>();
            if (data1_ptr->getTimeFrame()) {
                output->setTimeFrame(data1_ptr->getTimeFrame());
            }

            for (auto const & [time, result_variant] : result_view) {
                if (auto const * val = std::get_if<float>(&result_variant)) {
                    output->appendAtTime(time, std::vector<float>{*val}, NotifyObservers::No);
                }
            }

            return DataTypeVariant{output};
        }

        // Handle Line2D output type (for binary transforms like LineClip)
        if (meta->output_type == typeid(Line2D)) {
            auto output = std::make_shared<LineData>();
            if (data1_ptr->getTimeFrame()) {
                output->setTimeFrame(data1_ptr->getTimeFrame());
            }

            for (auto const & [time, result_variant] : result_view) {
                if (auto const * val = std::get_if<Line2D>(&result_variant)) {
                    if (!val->empty()) {
                        output->addAtTime(time, *val, NotifyObservers::No);
                    }
                }
            }

            return DataTypeVariant{output};
        }

        // Add more output type handlers as needed
        std::cerr << "Unsupported output type for multi-input transform" << std::endl;
        return std::nullopt;

    } catch (std::exception const & e) {
        std::cerr << "Pipeline execution failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

/**
 * @brief Fallback for unsupported container types
 */
template<typename Container1, typename Container2>
auto executeBinaryTransformImpl(
    std::shared_ptr<Container1> const &,
    std::shared_ptr<Container2> const &,
    std::string const &,
    std::any const &,
    std::vector<DataManagerStepDescriptor> const &,
    size_t,
    DataManagerPipelineExecutor const &)
    -> std::enable_if_t<
        !(has_element_type_v<Container1> && has_element_type_v<Container2>),
        std::optional<DataTypeVariant>>
{
    std::cerr << "Container types do not support element-level transforms" << std::endl;
    return std::nullopt;
}

// ============================================================================
// Binary Container Transform Execution Helper
// ============================================================================

/**
 * @brief Execute a binary container transform with type dispatch
 * 
 * This helper handles binary container transforms (like DigitalIntervalBoolean)
 * that operate on two whole containers rather than element-by-element.
 * 
 * Uses std::visit to dispatch to the correct typed execution.
 */
template<typename Container1, typename Container2, typename Params>
std::optional<DataTypeVariant> tryExecuteBinaryContainerTransform(
    std::shared_ptr<Container1> const & data1_ptr,
    std::shared_ptr<Container2> const & data2_ptr,
    std::string const & transform_name,
    Params const & params) {
    
    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;
    
    // Check if this specific combination is registered
    auto const * meta = registry.getContainerMetadata(transform_name);
    if (!meta || !meta->is_multi_input || meta->input_arity != 2) {
        return std::nullopt;
    }
    
    // Verify input types match what's registered
    bool types_match = false;
    if (meta->individual_input_types.size() >= 2) {
        types_match = 
            (meta->individual_input_types[0] == std::type_index(typeid(Container1))) &&
            (meta->individual_input_types[1] == std::type_index(typeid(Container2)));
    }
    
    if (!types_match) {
        return std::nullopt;
    }
    
    // For DigitalIntervalSeries x DigitalIntervalSeries -> DigitalIntervalSeries
    // This is the specific case we need to handle
    if constexpr (std::is_same_v<Container1, DigitalIntervalSeries> && 
                  std::is_same_v<Container2, DigitalIntervalSeries>) {
        try {
            auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalIntervalSeries,
                DigitalIntervalSeries,
                Params>(
                transform_name,
                *data1_ptr,
                *data2_ptr,
                params,
                ctx);
            
            if (result) {
                return DataTypeVariant{result};
            }
        } catch (std::exception const & e) {
            std::cerr << "Binary container transform failed: " << e.what() << std::endl;
        }
    }
    
    return std::nullopt;
}

/**
 * @brief Overload for type-erased params (std::any)
 */
template<typename Container1, typename Container2>
std::optional<DataTypeVariant> tryExecuteBinaryContainerTransformAny(
    std::shared_ptr<Container1> const & data1_ptr,
    std::shared_ptr<Container2> const & data2_ptr,
    std::string const & transform_name,
    std::any const & params_any) {
    
    // Try known parameter types for binary container transforms
    // DigitalIntervalBoolean uses DigitalIntervalBooleanParams
    if constexpr (std::is_same_v<Container1, DigitalIntervalSeries> && 
                  std::is_same_v<Container2, DigitalIntervalSeries>) {
        try {
            auto const & params = std::any_cast<DigitalIntervalBooleanParams const &>(params_any);
            return tryExecuteBinaryContainerTransform(data1_ptr, data2_ptr, transform_name, params);
        } catch (std::bad_any_cast const &) {
            // Not the right param type, continue
        }
    }
    
    // Add more binary container transform parameter types here as needed
    
    return std::nullopt;
}

} // anonymous namespace

// ============================================================================
// Multi-Input Pipeline Execution
// ============================================================================

std::optional<DataTypeVariant> DataManagerPipelineExecutor::executeMultiInputStep(size_t step_index) {
    if (step_index >= steps_.size()) {
        return std::nullopt;
    }

    auto const & step = steps_[step_index];
    auto & registry = ElementRegistry::instance();

    // Get all input keys
    auto input_keys = step.getAllInputKeys();
    if (input_keys.size() < 2) {
        std::cerr << "Multi-input step requires at least 2 inputs, got " << input_keys.size() << std::endl;
        return std::nullopt;
    }

    // For now, only support binary (2-input) transforms
    if (input_keys.size() > 2) {
        std::cerr << "Currently only binary (2-input) transforms are supported" << std::endl;
        return std::nullopt;
    }

    // Get both input containers
    auto input1_opt = getInputData(input_keys[0]);
    auto input2_opt = getInputData(input_keys[1]);

    if (!input1_opt.has_value()) {
        std::cerr << "Input data '" << input_keys[0] << "' not found" << std::endl;
        return std::nullopt;
    }
    if (!input2_opt.has_value()) {
        std::cerr << "Input data '" << input_keys[1] << "' not found" << std::endl;
        return std::nullopt;
    }

    // Load parameters
    std::any params_any;
    if (step.parameters.has_value()) {
        std::string params_json = rfl::json::write(step.parameters.value());
        params_any = Examples::loadParametersForTransform(step.transform_name, params_json);
    } else {
        params_any = Examples::loadParametersForTransform(step.transform_name, "{}");
    }

    if (!params_any.has_value()) {
        std::cerr << "Failed to load parameters for transform '" << step.transform_name << "'" << std::endl;
        return std::nullopt;
    }

    // Check if this is a binary CONTAINER transform (operates on whole containers)
    // vs a binary ELEMENT transform (operates element-by-element with FlatZipView)
    auto const * container_meta = registry.getContainerMetadata(step.transform_name);
    bool is_binary_container_transform = container_meta && 
                                          container_meta->is_multi_input && 
                                          container_meta->input_arity == 2;

    // Type-dispatch to execute the binary transform
    return std::visit([&](auto const & data1_ptr) -> std::optional<DataTypeVariant> {
        return std::visit([&](auto const & data2_ptr) -> std::optional<DataTypeVariant> {
            if (!data1_ptr || !data2_ptr) {
                return std::nullopt;
            }
            
            // First, try binary container transform (whole-container operations)
            if (is_binary_container_transform) {
                auto result = tryExecuteBinaryContainerTransformAny(
                    data1_ptr, data2_ptr,
                    step.transform_name, params_any);
                if (result.has_value()) {
                    return result;
                }
                // If container transform didn't match types, fall through to element-level
            }
            
            // Fall back to element-level binary transform (FlatZipView approach)
            // This is used for transforms like LineMinPointDist that work on
            // std::tuple<Element1, Element2> -> OutputElement
            return executeBinaryTransformImpl(
                data1_ptr, data2_ptr,
                step.transform_name, params_any,
                steps_, step_index, *this);
        }, *input2_opt);
    }, *input1_opt);
}

bool DataManagerPipelineExecutor::canFuseStep(size_t step_index) const {
    if (step_index >= steps_.size()) return false;

    auto const & step = steps_[step_index];
    auto & registry = ElementRegistry::instance();

    // Multi-input steps cannot be fused (they start new segments)
    if (step.isMultiInput()) return false;

    // Container transforms cannot be fused
    if (registry.isContainerTransform(step.transform_name)) return false;

    // Check element transform metadata
    auto const * meta = registry.getMetadata(step.transform_name);
    if (!meta) return false;

    // Time-grouped transforms cannot be fused (need all values at a time)
    if (meta->is_time_grouped) return false;

    // Element-level transform - can fuse!
    return true;
}

bool DataManagerPipelineExecutor::stepsAreChained(size_t prev_step, size_t curr_step) const {
    if (prev_step >= steps_.size() || curr_step >= steps_.size()) return false;
    if (curr_step <= prev_step) return false;

    auto const & prev = steps_[prev_step];
    auto const & curr = steps_[curr_step];

    // Current step's input must come from previous step
    std::string prev_output = prev.output_key.value_or(prev.step_id);
    return curr.input_key == prev_output;
}

std::vector<DataManagerPipelineExecutor::PipelineSegment> DataManagerPipelineExecutor::buildSegments() const {
    std::vector<PipelineSegment> segments;
    auto & registry = ElementRegistry::instance();

    size_t i = 0;
    while (i < steps_.size()) {
        PipelineSegment seg;
        seg.start_step = i;
        seg.is_multi_input = steps_[i].isMultiInput();
        seg.input_keys = steps_[i].getAllInputKeys();

        // Check if first step requires materialization
        bool first_requires_mat = registry.isContainerTransform(steps_[i].transform_name);
        if (auto const * meta = registry.getMetadata(steps_[i].transform_name)) {
            first_requires_mat = first_requires_mat || meta->is_time_grouped;
        }

        // Greedily extend segment while steps are fusible
        size_t j = i + 1;
        while (j < steps_.size() && canFuseStep(j) && stepsAreChained(j - 1, j)) {
            ++j;
        }

        seg.end_step = j;
        seg.output_key = steps_[j - 1].output_key.value_or(steps_[j - 1].step_id);
        seg.requires_materialization = first_requires_mat || (j == i + 1);

        segments.push_back(seg);
        i = j;
    }

    return segments;
}

std::optional<DataTypeVariant> DataManagerPipelineExecutor::executeSegment(
        PipelineSegment const & segment) {
    // For now, delegate to existing step-by-step execution
    // Full segment fusion will be implemented in a future iteration
    
    if (segment.is_multi_input) {
        return executeMultiInputStep(segment.start_step);
    }

    // Get input data for first step
    auto input_data = getInputData(steps_[segment.start_step].input_key);
    if (!input_data.has_value()) {
        return std::nullopt;
    }

    // Execute transforms in segment
    DataTypeVariant current_data = *input_data;
    for (size_t i = segment.start_step; i < segment.end_step; ++i) {
        auto const & step = steps_[i];
        auto result = executeTransform(step.transform_name, current_data, step.parameters);
        if (!result.has_value()) {
            return std::nullopt;
        }
        current_data = *result;
    }

    return current_data;
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
