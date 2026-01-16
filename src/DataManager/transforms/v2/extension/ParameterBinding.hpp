#ifndef WHISKERTOOLBOX_V2_PARAMETER_BINDING_HPP
#define WHISKERTOOLBOX_V2_PARAMETER_BINDING_HPP

/**
 * @file ParameterBinding.hpp
 * @brief Utilities for applying value store bindings to transform parameters
 *
 * This file provides the binding mechanism that connects PipelineValueStore values
 * to transform parameter fields. It uses reflect-cpp's JSON serialization to achieve
 * type-safe binding without requiring compile-time knowledge of all parameter types.
 *
 * ## Binding Mechanism
 *
 * 1. Serialize parameters to JSON
 * 2. For each binding (field_name -> store_key), replace the JSON value
 * 3. Deserialize back to the parameter type
 *
 * This approach leverages reflect-cpp's existing serialization infrastructure and
 * handles type conversions automatically via JSON.
 *
 * ## Example
 *
 * ```cpp
 * struct ZScoreParams {
 *     float mean = 0.0f;
 *     float std_dev = 1.0f;
 *     bool clamp_outliers = false;
 * };
 *
 * PipelineValueStore store;
 * store.set("computed_mean", 0.5f);
 * store.set("computed_std", 0.1f);
 *
 * ZScoreParams base_params{};
 * std::map<std::string, std::string> bindings = {
 *     {"mean", "computed_mean"},     // field_name -> store_key
 *     {"std_dev", "computed_std"}
 * };
 *
 * auto bound_params = applyBindings(base_params, bindings, store);
 * // bound_params.mean == 0.5f
 * // bound_params.std_dev == 0.1f
 * ```
 *
 * ## Type-Erased Binding
 *
 * For runtime pipeline execution where parameter types aren't known at compile time,
 * use the registry-based `applyBindingsErased()` function. Transform registration
 * automatically registers a binding applicator for each parameter type.
 *
 * @see PipelineValueStore.hpp for the value storage
 * @see PipelineStep.hpp for integration with pipeline execution
 */

#include "transforms/v2/core/PipelineValueStore.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <nlohmann/json.hpp>

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Binding Application (Templated - for compile-time known types)
// ============================================================================

/**
 * @brief Apply value store bindings to parameters using reflect-cpp
 *
 * Bindings map parameter field names to value store keys.
 * Uses JSON as the interchange format for type-safe conversion.
 *
 * @tparam Params Parameter type (must be reflect-cpp serializable)
 * @param base_params The base parameters to modify
 * @param bindings Map from param field names to store keys
 * @param store The value store containing bound values
 * @return Parameters with bound values applied
 * @throws std::runtime_error if a binding key is not found in store or JSON manipulation fails
 *
 * @example
 * ```cpp
 * NormalizeTimeParams params{};
 * std::map<std::string, std::string> bindings = {{"alignment_time", "trial_start"}};
 * auto bound = applyBindings(params, bindings, store);
 * ```
 */
template<typename Params>
Params applyBindings(
    Params const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store)
{
    if (bindings.empty()) {
        return base_params;
    }

    // Serialize to JSON
    std::string json_str = rfl::json::write(base_params);
    nlohmann::json json_obj = nlohmann::json::parse(json_str);

    // Apply bindings
    for (auto const& [field_name, store_key] : bindings) {
        auto value_json = store.getJson(store_key);
        if (!value_json) {
            throw std::runtime_error(
                "Binding failed: store key '" + store_key +
                "' not found for field '" + field_name + "'");
        }

        // Parse the value JSON and insert into the object
        // This handles type conversion automatically
        nlohmann::json value = nlohmann::json::parse(*value_json);
        json_obj[field_name] = value;
    }

    // Deserialize back to Params
    std::string modified_json = json_obj.dump();
    auto result = rfl::json::read<Params>(modified_json);

    if (!result) {
        throw std::runtime_error(
            "Failed to deserialize parameters after binding: " +
            std::string(result.error().value().what()));
    }

    return result.value();
}

/**
 * @brief Try to apply bindings, returning nullopt on failure
 *
 * Non-throwing version of applyBindings for cases where binding failure
 * should be handled gracefully.
 *
 * @tparam Params Parameter type
 * @param base_params The base parameters to modify
 * @param bindings Map from param field names to store keys
 * @param store The value store containing bound values
 * @return Parameters with bound values applied, or nullopt on failure
 */
template<typename Params>
std::optional<Params> tryApplyBindings(
    Params const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store) noexcept
{
    try {
        return applyBindings(base_params, bindings, store);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================================
// Binding Applicator Registry (for type-erased runtime binding)
// ============================================================================

/**
 * @brief Type-erased binding applicator function signature
 *
 * Takes type-erased parameters (std::any), applies bindings, returns modified params.
 */
using BindingApplicatorFn = std::function<std::any(
    std::any const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store)>;

/**
 * @brief Registry for binding applicators by parameter type
 *
 * Each parameter type registers an applicator function during transform registration.
 * This enables type-erased binding application in pipeline runtime.
 */
inline std::unordered_map<std::type_index, BindingApplicatorFn>&
getBindingApplicatorRegistry() {
    static std::unordered_map<std::type_index, BindingApplicatorFn> registry;
    return registry;
}

/**
 * @brief Register a binding applicator for a parameter type
 *
 * This is called automatically during transform registration via
 * RegisterBindingApplicator helper class.
 *
 * @tparam Params Parameter type to register
 */
template<typename Params>
void registerBindingApplicatorFor() {
    auto& registry = getBindingApplicatorRegistry();
    auto type_idx = std::type_index(typeid(Params));

    if (registry.find(type_idx) == registry.end()) {
        registry[type_idx] = [](
            std::any const& base_params,
            std::map<std::string, std::string> const& bindings,
            PipelineValueStore const& store) -> std::any
        {
            auto const& typed_params = std::any_cast<Params const&>(base_params);
            return std::any{applyBindings(typed_params, bindings, store)};
        };
    }
}

/**
 * @brief RAII helper for registering binding applicators at static initialization
 *
 * Usage in parameter header or transform registration:
 * ```cpp
 * namespace {
 *     inline auto const reg_binding = RegisterBindingApplicator<MyParams>();
 * }
 * ```
 */
template<typename Params>
class RegisterBindingApplicator {
public:
    RegisterBindingApplicator() {
        registerBindingApplicatorFor<Params>();
    }
};

// ============================================================================
// Type-Erased Binding Application
// ============================================================================

/**
 * @brief Apply bindings to type-erased parameters
 *
 * Uses the binding applicator registry to apply bindings at runtime
 * when the parameter type is only known via type_index.
 *
 * @param params_type Type index of the parameter type
 * @param base_params Type-erased base parameters (std::any)
 * @param bindings Map from param field names to store keys
 * @param store The value store containing bound values
 * @return Type-erased parameters with bound values applied
 * @throws std::runtime_error if no applicator is registered for the type
 */
inline std::any applyBindingsErased(
    std::type_index params_type,
    std::any const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store)
{
    if (bindings.empty()) {
        return base_params;
    }

    auto& registry = getBindingApplicatorRegistry();
    auto it = registry.find(params_type);

    if (it == registry.end()) {
        throw std::runtime_error(
            "No binding applicator registered for parameter type: " +
            std::string(params_type.name()));
    }

    return it->second(base_params, bindings, store);
}

/**
 * @brief Try to apply bindings to type-erased parameters
 *
 * Non-throwing version that returns the original parameters if binding fails.
 *
 * @param params_type Type index of the parameter type
 * @param base_params Type-erased base parameters
 * @param bindings Map from param field names to store keys
 * @param store The value store containing bound values
 * @return Parameters with bound values applied, or original params on failure
 */
inline std::any tryApplyBindingsErased(
    std::type_index params_type,
    std::any const& base_params,
    std::map<std::string, std::string> const& bindings,
    PipelineValueStore const& store) noexcept
{
    try {
        return applyBindingsErased(params_type, base_params, bindings, store);
    } catch (...) {
        return base_params;
    }
}

/**
 * @brief Check if a binding applicator is registered for a parameter type
 *
 * @param params_type Type index to check
 * @return true if an applicator is registered
 */
inline bool hasBindingApplicator(std::type_index params_type) {
    auto& registry = getBindingApplicatorRegistry();
    return registry.find(params_type) != registry.end();
}

}  // namespace WhiskerToolbox::Transforms::V2

#endif  // WHISKERTOOLBOX_V2_PARAMETER_BINDING_HPP
