#ifndef WHISKERTOOLBOX_V2_REDUCTION_STEP_HPP
#define WHISKERTOOLBOX_V2_REDUCTION_STEP_HPP

/**
 * @file ReductionStep.hpp
 * @brief Represents a reduction that computes a value for the pipeline value store
 *
 * ReductionStep is used in the V2 pipeline architecture to compute scalar values
 * that can be bound to subsequent transform parameters. This enables pipelines
 * where statistics computed over a data range (mean, std, min, max) are automatically
 * injected into transform parameters.
 *
 * ## Typical Usage Flow
 *
 * ```
 * Pipeline Execution:
 * 1. Run all ReductionSteps → populate PipelineValueStore
 * 2. For each PipelineStep:
 *    a. Apply bindings from store
 *    b. Execute transform
 * ```
 *
 * ## Example: ZScore Normalization
 *
 * ```json
 * {
 *   "reductions": [
 *     {"reduction": "Mean", "output_key": "computed_mean"},
 *     {"reduction": "StandardDeviation", "output_key": "computed_std"}
 *   ],
 *   "steps": [
 *     {
 *       "transform": "ZScoreNormalize",
 *       "bindings": {
 *         "mean": "computed_mean",
 *         "std_dev": "computed_std"
 *       }
 *     }
 *   ]
 * }
 * ```
 *
 * @see PipelineValueStore.hpp for the value storage
 * @see ParameterBinding.hpp for applying bound values to parameters
 * @see TransformPipeline for integration with pipeline execution
 */

#include <rfl.hpp>

#include <any>
#include <map>
#include <optional>
#include <string>
#include <typeindex>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Represents a reduction that computes a value for the store
 *
 * A ReductionStep encapsulates:
 * - The name of the reduction to execute (from RangeReductionRegistry)
 * - The key under which to store the result
 * - Optional parameters for the reduction
 * - Optional bindings for the reduction's own parameters
 *
 * The result of executing a ReductionStep is a scalar value that gets
 * stored in the PipelineValueStore under `output_key`.
 */
struct ReductionStep {
    /// Name of the registered range reduction (e.g., "Mean", "StdDev")
    std::string reduction_name;
    
    /// Key under which to store the result in PipelineValueStore
    std::string output_key;
    
    /// Type-erased reduction parameters (optional)
    std::any params;
    
    /// Bindings for the reduction's own parameters (optional)
    /// Key: param field name, Value: store key
    std::map<std::string, std::string> param_bindings;

    // ========================================================================
    // Type information (populated during registration/loading)
    // ========================================================================
    
    /// Input element type for the reduction (e.g., typeid(float))
    std::type_index input_type = typeid(void);
    
    /// Output scalar type (e.g., typeid(float))
    std::type_index output_type = typeid(void);
    
    /// Parameter type (e.g., typeid(NoReductionParams))
    std::type_index params_type = typeid(void);

    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Default constructor
     */
    ReductionStep() = default;

    /**
     * @brief Construct a reduction step with name and output key
     * @param name Name of the registered reduction
     * @param key Store key for the result
     */
    ReductionStep(std::string name, std::string key)
        : reduction_name(std::move(name))
        , output_key(std::move(key))
    {}

    /**
     * @brief Construct a reduction step with parameters
     * @param name Name of the registered reduction
     * @param key Store key for the result
     * @param p Reduction parameters
     */
    template<typename Params>
    ReductionStep(std::string name, std::string key, Params p)
        : reduction_name(std::move(name))
        , output_key(std::move(key))
        , params(std::move(p))
        , params_type(typeid(Params))
    {}

    // ========================================================================
    // Query Methods
    // ========================================================================

    /**
     * @brief Check if this step has parameters
     */
    [[nodiscard]] bool hasParams() const noexcept {
        return params.has_value();
    }

    /**
     * @brief Check if this step has parameter bindings
     */
    [[nodiscard]] bool hasBindings() const noexcept {
        return !param_bindings.empty();
    }

    /**
     * @brief Check if type information has been populated
     */
    [[nodiscard]] bool hasTypeInfo() const noexcept {
        return input_type != typeid(void) && output_type != typeid(void);
    }
};

/**
 * @brief JSON descriptor for loading ReductionStep from pipelines
 *
 * This mirrors PipelineStepDescriptor for consistency with the JSON
 * pipeline loading infrastructure.
 */
struct ReductionStepDescriptor {
    /// Name of the reduction (must exist in RangeReductionRegistry)
    std::string reduction_name;
    
    /// Store key for the result
    std::string output_key;
    
    /// Raw JSON parameters (optional)
    std::optional<rfl::Generic> parameters;
    
    /// Bindings from store keys to param fields (optional)
    std::optional<std::map<std::string, std::string>> bindings;
    
    /// Optional description for documentation
    std::optional<std::string> description;
};

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_REDUCTION_STEP_HPP
