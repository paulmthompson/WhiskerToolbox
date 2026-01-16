#ifndef WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP
#define WHISKERTOOLBOX_V2_PIPELINE_LOADER_HPP

#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/RangeReductionRegistry.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/detail/ReductionStep.hpp"

#include <fstream>
#include <map>
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
 * 
 * Example JSON with parameter bindings:
 * ```json
 * {
 *   "step_id": "zscore_normalize",
 *   "transform_name": "ZScoreNormalize",
 *   "parameters": {},
 *   "param_bindings": {
 *     "mean": "computed_mean",
 *     "std_dev": "computed_std"
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

    // Parameter bindings from PipelineValueStore keys to parameter fields
    // Key: parameter field name, Value: store key to bind from
    std::optional<std::map<std::string, std::string>> param_bindings;

    // Optional fields for organization and control
    std::optional<std::string> description;
    std::optional<bool> enabled;
    std::optional<std::vector<std::string>> tags;
};

/**
 * @brief Descriptor for a pre-execution reduction step
 * 
 * Pre-reductions compute values from the input data before any transforms
 * run. The computed values are stored in the PipelineValueStore and can be
 * bound to transform parameters.
 * 
 * Example JSON:
 * ```json
 * {
 *   "reduction_name": "Mean",
 *   "output_key": "computed_mean",
 *   "parameters": {}
 * }
 * ```
 * 
 * Example JSON with bindings for reduction parameters:
 * ```json
 * {
 *   "reduction_name": "Percentile",
 *   "output_key": "p95_value",
 *   "parameters": {
 *     "percentile": 95.0
 *   },
 *   "param_bindings": {
 *     "baseline": "some_other_key"
 *   }
 * }
 * ```
 */
struct PreReductionStepDescriptor {
    // Name of the reduction (must exist in RangeReductionRegistry)
    std::string reduction_name;

    // Key under which to store the result in PipelineValueStore
    std::string output_key;

    // Raw JSON parameters - will be parsed based on reduction_name
    std::optional<rfl::Generic> parameters;

    // Parameter bindings for the reduction's own parameters (optional)
    // Key: param field name, Value: store key
    std::optional<std::map<std::string, std::string>> param_bindings;

    // Optional description for documentation
    std::optional<std::string> description;
};

/**
 * @brief Descriptor for a terminal range reduction step
 * 
 * Range reductions consume an entire range of elements and produce a scalar.
 * This is used for trial-based analysis where each trial needs to be reduced
 * to a single value (e.g., for sorting, partitioning, coloring).
 * 
 * Example JSON:
 * ```json
 * {
 *   "reduction_name": "FirstPositiveLatency",
 *   "parameters": {
 *     "normalize_by": "trial_duration"
 *   }
 * }
 * ```
 */
struct RangeReductionStepDescriptor {
    // Name of the reduction (must exist in RangeReductionRegistry)
    std::string reduction_name;

    // Raw JSON parameters - will be parsed based on reduction_name
    std::optional<rfl::Generic> parameters;

    // Optional description for documentation
    std::optional<std::string> description;
};

/**
 * @brief Complete pipeline descriptor
 * 
 * A pipeline consists of element transform steps and an optional terminal
 * range reduction. The steps are applied in order to transform elements,
 * and the range reduction (if present) collapses the result to a scalar.
 * 
 * Example JSON without range reduction:
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
 * 
 * Example JSON with range reduction:
 * ```json
 * {
 *   "metadata": {
 *     "name": "Spike Latency Pipeline",
 *     "version": "1.0"
 *   },
 *   "steps": [
 *     {
 *       "step_id": "normalize",
 *       "transform_name": "NormalizeTimeValue"
 *     }
 *   ],
 *   "range_reduction": {
 *     "reduction_name": "FirstPositiveLatency",
 *     "description": "First spike latency after alignment"
 *   }
 * }
 * ```
 * 
 * Example JSON with pre-reductions and parameter bindings:
 * ```json
 * {
 *   "metadata": {
 *     "name": "ZScore Normalization Pipeline",
 *     "version": "1.0"
 *   },
 *   "pre_reductions": [
 *     {"reduction_name": "Mean", "output_key": "computed_mean"},
 *     {"reduction_name": "StandardDeviation", "output_key": "computed_std"}
 *   ],
 *   "steps": [
 *     {
 *       "step_id": "zscore",
 *       "transform_name": "ZScoreNormalize",
 *       "param_bindings": {
 *         "mean": "computed_mean",
 *         "std_dev": "computed_std"
 *       }
 *     }
 *   ]
 * }
 * ```
 */
struct PipelineDescriptor {
    std::optional<PipelineMetadata> metadata;
    std::optional<std::vector<PreReductionStepDescriptor>> pre_reductions;
    std::vector<PipelineStepDescriptor> steps;
    std::optional<RangeReductionStepDescriptor> range_reduction;
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
 * @brief Load a pre-reduction from JSON descriptor
 * 
 * This function:
 * 1. Validates that reduction_name exists in RangeReductionRegistry
 * 2. Loads parameters using the registry's automatic deserializer
 * 3. Returns a ReductionStep ready to be added to the pipeline
 * 
 * @param descriptor Pre-reduction step descriptor from JSON
 * @return rfl::Result<ReductionStep> Success or error message
 */
rfl::Result<ReductionStep> loadPreReductionFromDescriptor(
        PreReductionStepDescriptor const & descriptor);

/**
 * @brief Load a range reduction from JSON descriptor
 * 
 * This function:
 * 1. Validates that reduction_name exists in RangeReductionRegistry
 * 2. Loads parameters using the registry's automatic deserializer
 * 3. Returns metadata needed to configure the pipeline's range reduction
 * 
 * @param descriptor Range reduction descriptor from JSON
 * @return rfl::Result containing (name, params_any) pair or error message
 */
rfl::Result<std::pair<std::string, std::any>> loadRangeReductionFromDescriptor(
        RangeReductionStepDescriptor const & descriptor);

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
