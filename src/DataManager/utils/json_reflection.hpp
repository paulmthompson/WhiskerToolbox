#ifndef JSON_REFLECTION_HPP
#define JSON_REFLECTION_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <iostream>
#include <optional>

namespace WhiskerToolbox {
namespace Reflection {

/**
 * @brief Convert nlohmann::json to reflect-cpp compatible format
 * 
 * This helper bridges the gap between our existing nlohmann::json usage
 * and reflect-cpp's JSON parsing. It converts the JSON to a string and
 * uses reflect-cpp's parser to deserialize into a reflected type.
 * 
 * @tparam T The reflected type to parse into
 * @param json The nlohmann::json object to parse
 * @return rfl::Result<T> containing either the parsed object or an error
 */
template<typename T>
rfl::Result<T> parseJson(nlohmann::json const& json) {
    try {
        std::string json_str = json.dump();
        return rfl::json::read<T>(json_str);
    } catch (std::exception const& e) {
        return rfl::Error(std::string("JSON parse error: ") + e.what());
    }
}

/**
 * @brief Convert reflected object to nlohmann::json
 * 
 * Useful for maintaining compatibility with existing code that expects
 * nlohmann::json objects.
 * 
 * @tparam T The reflected type to serialize
 * @param obj The object to serialize
 * @return nlohmann::json representation of the object
 */
template<typename T>
nlohmann::json toJson(T const& obj) {
    std::string json_str = rfl::json::write(obj);
    return nlohmann::json::parse(json_str);
}

/**
 * @brief Print reflection field information for debugging
 * 
 * Useful for understanding what fields a reflected type has.
 * Note: Field introspection is done at compile time in reflect-cpp.
 * This is a placeholder for debugging output.
 * 
 * @tparam T The reflected type to inspect
 */
template<typename T>
void printFieldInfo() {
    std::cout << "Type: " << typeid(T).name() << "\n";
    std::cout << "(Field introspection is compile-time only in reflect-cpp)\n";
}

/**
 * @brief Generate JSON schema for a reflected type
 * 
 * This is extremely useful for:
 * - Documentation generation
 * - Validation
 * - UI generation
 * - Fuzz testing corpus generation
 * 
 * @tparam T The reflected type to generate schema for
 * @return std::string JSON schema as a formatted string
 */
template<typename T>
std::string generateSchema() {
    // rfl::json::to_schema already returns a formatted string
    return rfl::json::to_schema<T>();
}

/**
 * @brief Try to parse JSON with reflected type, with detailed error reporting
 * 
 * This variant provides more user-friendly error messages by catching
 * and formatting validation errors.
 * 
 * @tparam T The reflected type to parse into
 * @param json The nlohmann::json object to parse
 * @param context_name Optional context for error messages (e.g., "BinaryAnalogLoader")
 * @return std::optional<T> containing the parsed object if successful
 */
template<typename T>
std::optional<T> tryParseWithErrors(nlohmann::json const& json, 
                                    std::string const& context_name = "") {
    auto result = parseJson<T>(json);
    
    if (!result) {
        std::string prefix = context_name.empty() ? "" : context_name + ": ";
        std::cerr << prefix << "Failed to parse JSON: " << result.error()->what() << "\n";
        return std::nullopt;
    }
    
    return result.value();
}

} // namespace Reflection
} // namespace WhiskerToolbox

#endif // JSON_REFLECTION_HPP
