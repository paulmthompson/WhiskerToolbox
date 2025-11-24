#ifndef WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP
#define WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP

#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/examples/ParameterIO.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

// ============================================================================
// Pipeline JSON Schema using reflect-cpp
// ============================================================================

/**
 * @brief Metadata for a pipeline
 * 
 * All fields are optional to allow minimal pipeline definitions.
 */
struct PipelineMetadata {
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> version;
    std::optional<std::string> author;
    std::optional<std::string> created;
    std::optional<std::vector<std::string>> tags;
};

/**
 * @brief Descriptor for a single pipeline step
 * 
 * This is the JSON representation of a transform step before it's converted
 * to a PipelineStep with concrete parameter types.
 * 
 * Example JSON:
 * ```json
 * {
 *   "step_id": "calculate_area",
 *   "transform_name": "CalculateMaskArea",
 *   "parameters": {
 *     "scale_factor": 1.5,
 *     "min_area": 10.0,
 *     "exclude_holes": false
 *   }
 * }
 * ```
 */
struct PipelineStepDescriptor {
    // Unique identifier for this step (for error reporting and dependencies)
    std::string step_id;
    
    // Name of the transform (must exist in ElementRegistry)
    std::string transform_name;
    
    // Raw JSON parameters - will be parsed based on transform_name
    // Using rfl::Generic to preserve arbitrary JSON structure
    std::optional<rfl::Generic> parameters;
    
    // Optional fields for organization and control
    std::optional<std::string> description;
    std::optional<bool> enabled;
    std::optional<std::vector<std::string>> tags;
};

/**
 * @brief Complete pipeline descriptor
 * 
 * Example JSON:
 * ```json
 * {
 *   "metadata": {
 *     "name": "Mask Analysis Pipeline",
 *     "version": "1.0"
 *   },
 *   "steps": [
 *     {
 *       "step_id": "area_calculation",
 *       "transform_name": "CalculateMaskArea",
 *       "parameters": {
 *         "scale_factor": 1.5
 *       }
 *     }
 *   ]
 * }
 * ```
 */
struct PipelineDescriptor {
    std::optional<PipelineMetadata> metadata;
    std::vector<PipelineStepDescriptor> steps;
};

// ============================================================================
// Pipeline Loading Functions
// ============================================================================

/**
 * @brief Load a single pipeline step from JSON descriptor
 * 
 * This function:
 * 1. Validates that transform_name exists in the registry
 * 2. Loads parameters using the appropriate type from ParameterIO
 * 3. Creates a PipelineStep with type-erased parameters
 * 
 * @param descriptor Step descriptor from JSON
 * @return rfl::Result<PipelineStep> Success or error message
 */
inline rfl::Result<PipelineStep> loadStepFromDescriptor(PipelineStepDescriptor const& descriptor) {
    auto& registry = ElementRegistry::instance();
    
    // Validate transform exists
    auto const* metadata = registry.getMetadata(descriptor.transform_name);
    if (!metadata) {
        return rfl::Error("Transform '" + descriptor.transform_name + "' not found in registry");
    }
    
    // Check if step is disabled
    if (descriptor.enabled.has_value() && !descriptor.enabled.value()) {
        return rfl::Error("Step '" + descriptor.step_id + "' is disabled");
    }
    
    // Load parameters if provided
    if (descriptor.parameters.has_value()) {
        // Convert rfl::Generic back to JSON string for loadParameterVariant
        auto json_str = rfl::json::write(descriptor.parameters.value());
        
        auto param_variant = loadParameterVariant(descriptor.transform_name, json_str);
        if (!param_variant.has_value()) {
            return rfl::Error("Failed to load parameters for transform '" + 
                            descriptor.transform_name + "' in step '" + 
                            descriptor.step_id + "'");
        }
        
        // Create PipelineStep based on parameter type
        return std::visit([&](auto const& params) -> rfl::Result<PipelineStep> {
            using ParamType = std::decay_t<decltype(params)>;
            return rfl::Result<PipelineStep>(PipelineStep(descriptor.transform_name, params));
        }, param_variant.value());
    } else {
        // No parameters - create step with NoParams
        return rfl::Result<PipelineStep>(PipelineStep(descriptor.transform_name));
    }
}

/**
 * @brief Load a complete pipeline from JSON string
 * 
 * This function parses the JSON, validates all steps, and creates a TransformPipeline
 * with properly typed parameters.
 * 
 * Example usage:
 * ```cpp
 * auto pipeline_result = loadPipelineFromJson(json_str);
 * if (pipeline_result) {
 *     auto pipeline = pipeline_result.value();
 *     // Use pipeline...
 * } else {
 *     std::cerr << "Error: " << pipeline_result.error()->what() << std::endl;
 * }
 * ```
 * 
 * @param json_str JSON string containing pipeline descriptor
 * @return rfl::Result<TransformPipeline> Loaded pipeline or error
 */
inline rfl::Result<TransformPipeline> loadPipelineFromJson(std::string const& json_str) {
    // Parse JSON into descriptor
    auto descriptor_result = rfl::json::read<PipelineDescriptor>(json_str);
    if (!descriptor_result) {
        return rfl::Error("Failed to parse pipeline JSON: " + 
                         std::string(descriptor_result.error()->what()));
    }
    
    auto const& descriptor = descriptor_result.value();
    
    // Validate we have at least one step
    if (descriptor.steps.empty()) {
        return rfl::Error("Pipeline must have at least one step");
    }
    
    // Create empty pipeline
    TransformPipeline pipeline;
    
    // Load each step
    for (size_t i = 0; i < descriptor.steps.size(); ++i) {
        auto const& step_desc = descriptor.steps[i];
        
        auto step_result = loadStepFromDescriptor(step_desc);
        if (!step_result) {
            return rfl::Error("Failed to load step " + std::to_string(i) + 
                            " ('" + step_desc.step_id + "'): " +
                            std::string(step_result.error()->what()));
        }
        
        // Add step to pipeline
        pipeline.addStep(step_result.value());
    }
    
    return rfl::Result<TransformPipeline>(std::move(pipeline));
}

/**
 * @brief Load pipeline from JSON file
 * 
 * @param filepath Path to JSON file
 * @return rfl::Result<TransformPipeline> Loaded pipeline or error
 */
inline rfl::Result<TransformPipeline> loadPipelineFromFile(std::string const& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return rfl::Error("Failed to open file: " + filepath);
        }
        
        std::string json_str((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        return loadPipelineFromJson(json_str);
    } catch (std::exception const& e) {
        return rfl::Error("Exception reading file: " + std::string(e.what()));
    }
}

/**
 * @brief Save pipeline descriptor to JSON string
 * 
 * Note: This saves the descriptor, not the executable pipeline.
 * Parameter values are preserved but executor functions are not serialized.
 * 
 * @param descriptor Pipeline descriptor to save
 * @return std::string JSON representation
 */
inline std::string savePipelineToJson(PipelineDescriptor const& descriptor) {
    return rfl::json::write(descriptor);
}

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP
