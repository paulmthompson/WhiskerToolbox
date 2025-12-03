#ifndef WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP
#define WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP

#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <fstream>
#include <optional>
#include <rfl.hpp>
#include <rfl/json.hpp>
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
// Pipeline Step Factory Registry
// ============================================================================

/**
 * @brief Registry of PipelineStep factory functions
 * 
 * Maps type_index -> factory function that creates PipelineStep from std::any.
 * Factories are registered automatically via static initialization when
 * parameter types are used with RegisterTransform.
 */
inline std::unordered_map<
        std::type_index,
        std::function<PipelineStep(std::string const &, std::any const &)>> &
getPipelineStepFactoryRegistry() {
    static std::unordered_map<
            std::type_index,
            std::function<PipelineStep(std::string const &, std::any const &)>>
            registry;
    return registry;
}

/**
 * @brief Helper to register a PipelineStep factory for a parameter type
 * 
 * Call this once for each parameter type to register its factory.
 * Template automatically deduces the parameter type.
 */
template<typename Params>
inline void registerPipelineStepFactoryFor() {
    auto & registry = getPipelineStepFactoryRegistry();
    auto type_idx = std::type_index(typeid(Params));

    // Only register if not already present
    if (registry.find(type_idx) == registry.end()) {
        registry[type_idx] = [](std::string const & name, std::any const & params_any) {
            auto params = std::any_cast<Params>(params_any);
            return PipelineStep(name, params);
        };
    }
}

/**
 * @brief Auto-register a PipelineStep factory for a parameter type
 * 
 * This template class registers a factory function during static initialization.
 * Used by transform registration to automatically register factories.
 */
template<typename Params>
struct RegisterPipelineStepFactory {
    RegisterPipelineStepFactory() {
        registerPipelineStepFactoryFor<Params>();
    }
};

/**
 * @brief Create PipelineStep using the factory registry
 * 
 * Looks up the appropriate factory based on parameter type from metadata.
 * 
 * @param registry Element registry (for metadata lookup)
 * @param transform_name Transform name
 * @param params_any Type-erased parameters
 * @return PipelineStep with correctly typed parameters
 */
PipelineStep createPipelineStepFromRegistry(
        ElementRegistry const & registry,
        std::string const & transform_name,
        std::any const & params_any);

// ============================================================================
// Pipeline Loading Functions
// ============================================================================

/**
 * @brief Load a single pipeline step from JSON descriptor
 * 
 * This function:
 * 1. Validates that transform_name exists in the registry
 * 2. Loads parameters using the registry's automatic deserializer
 * 3. Creates a PipelineStep using the factory registry
 * 
 * @param descriptor Step descriptor from JSON
 * @return rfl::Result<PipelineStep> Success or error message
 */
rfl::Result<PipelineStep> loadStepFromDescriptor(PipelineStepDescriptor const & descriptor);

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
rfl::Result<TransformPipeline> loadPipelineFromJson(std::string const & json_str);

/**
 * @brief Load pipeline from JSON file
 * 
 * @param filepath Path to JSON file
 * @return rfl::Result<TransformPipeline> Loaded pipeline or error
 */
rfl::Result<TransformPipeline> loadPipelineFromFile(std::string const & filepath);

/**
 * @brief Save pipeline descriptor to JSON string
 * 
 * Note: This saves the descriptor, not the executable pipeline.
 * Parameter values are preserved but executor functions are not serialized.
 * 
 * @param descriptor Pipeline descriptor to save
 * @return std::string JSON representation
 */
std::string savePipelineToJson(PipelineDescriptor const & descriptor);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP
