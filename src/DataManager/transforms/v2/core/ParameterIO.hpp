#ifndef WHISKERTOOLBOX_V2_PARAMETER_IO_HPP
#define WHISKERTOOLBOX_V2_PARAMETER_IO_HPP

#include "transforms/v2/core/ElementRegistry.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <any>
#include <fstream>
#include <optional>
#include <string>

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Load parameters from JSON string
 * 
 * @tparam ParamsT Parameter type (e.g., MaskAreaParams)
 * @param json_str JSON string
 * @return Result containing parameters or error
 * 
 * @example
 * ```cpp
 * auto result = loadParametersFromJson<MaskAreaParams>(R"({"scale_factor": 2.5})");
 * if (result) {
 *     auto params = result.value();
 *     // use params...
 * } else {
 *     std::cerr << "Error: " << result.error().what() << std::endl;
 * }
 * ```
 */
template<typename ParamsT>
rfl::Result<ParamsT> loadParametersFromJson(std::string const& json_str) {
    return rfl::json::read<ParamsT>(json_str);
}

/**
 * @brief Load parameters from JSON file
 * 
 * @tparam ParamsT Parameter type
 * @param file_path Path to JSON file
 * @return Result containing parameters or error
 */
template<typename ParamsT>
rfl::Result<ParamsT> loadParametersFromFile(std::string const& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return rfl::Error("Cannot open file: " + file_path);
    }
    
    std::string json_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    return loadParametersFromJson<ParamsT>(json_str);
}

/**
 * @brief Save parameters to JSON string
 * 
 * @tparam ParamsT Parameter type
 * @param params Parameters to save
 * @return JSON string representation
 */
template<typename ParamsT>
std::string saveParametersToJson(ParamsT const& params) {
    return rfl::json::write(params);
}

/**
 * @brief Save parameters to JSON file
 * 
 * @tparam ParamsT Parameter type
 * @param params Parameters to save
 * @param file_path Path to output file
 * @param pretty_print Whether to pretty-print the JSON (default: true)
 * @return true if saved successfully
 */
template<typename ParamsT>
bool saveParametersToFile(ParamsT const& params, 
                          std::string const& file_path,
                          bool pretty_print = true) {
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }
        
        std::string json_str;
        if (pretty_print) {
            json_str = rfl::json::write(params);  // rfl uses indentation by default
        } else {
            json_str = rfl::json::write(params);
        }
        
        file << json_str;
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Load parameters for a transform using registry-based dispatch
 * 
 * This function uses the ElementRegistry's metadata to automatically determine
 * the correct parameter type and deserializer. The deserializers are automatically
 * registered when transforms are registered, so no manual registration is needed.
 * 
 * @param transform_name Name of the transform (must be registered)
 * @param json_str JSON string containing parameters
 * @return std::any containing typed parameters, or empty on failure
 * 
 * @example
 * ```cpp
 * auto params_any = loadParametersForTransform("CalculateMaskArea", json);
 * if (params_any.has_value()) {
 *     auto params = std::any_cast<MaskAreaParams>(params_any);
 *     // use params...\n * }\n * ```
 */
inline std::any loadParametersForTransform(
    std::string const& transform_name,
    std::string const& json_str)
{
    auto& registry = ElementRegistry::instance();
    return registry.deserializeParameters(transform_name, json_str);
}

} // namespace WhiskerToolbox::Transforms::V2::Examples

#endif // WHISKERTOOLBOX_V2_PARAMETER_IO_HPP
