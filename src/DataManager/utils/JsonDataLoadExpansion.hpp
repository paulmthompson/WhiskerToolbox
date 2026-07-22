/// @file JsonDataLoadExpansion.hpp
/// @brief Expands top-level JSON load-config loops into concrete data entries.

#ifndef JSON_DATA_LOAD_EXPANSION_HPP
#define JSON_DATA_LOAD_EXPANSION_HPP

#include "nlohmann/json.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

/// @brief Parse a single loop definition into iteration values as strings.
///
/// Accepts a JSON array of numbers or strings, or an inclusive range object
/// `{ "from": int, "to": int, "step": int? }` with default `step` of 1.
///
/// @param definition Loop JSON value from the root `"loops"` object.
/// @return Iteration values on success; empty optional on invalid input.
[[nodiscard]] std::optional<std::vector<std::string>>
parseLoopDefinition(nlohmann::json const & definition);

/// @brief Collect `{identifier}` placeholder names in a string (not `${...}`).
///
/// @param text Input string potentially containing brace placeholders.
/// @return Sorted unique placeholder names.
[[nodiscard]] std::set<std::string> collectBracePlaceholders(std::string const & text);

/// @brief Collect `{identifier}` placeholders across all string fields in a JSON value.
///
/// @param value JSON object or nested structure to scan.
/// @return Sorted unique placeholder names.
[[nodiscard]] std::set<std::string> collectBracePlaceholders(nlohmann::json const & value);

/// @brief Return a deep copy of @p value with `{name}` replaced using @p bindings.
///
/// @param value JSON value to copy and mutate in string fields.
/// @param bindings Map from placeholder name (without braces) to replacement text.
/// @return Copy with substitutions applied in all string fields.
[[nodiscard]] nlohmann::json substituteBracePlaceholders(
        nlohmann::json value,
        std::map<std::string, std::string> const & bindings);

/// @brief Expand templated entries in a data-load array using root loop definitions.
///
/// Entries without `{...}` placeholders are copied once. Entries with placeholders
/// must reference exactly one loop variable name that exists in @p loops. Objects
/// containing `"transformations"` are passed through unchanged.
///
/// @param data The `"data"` array from a load config.
/// @param loops The root `"loops"` object mapping names to loop definitions.
/// @return Expanded data array suitable for `${variable}` substitution.
[[nodiscard]] nlohmann::json expandDataArrayLoops(
        nlohmann::json const & data,
        nlohmann::json const & loops);

#endif// JSON_DATA_LOAD_EXPANSION_HPP
