/**
 * @file VariableSubstitution.hpp
 * @brief Two-pass ${variable} substitution for command sequences
 */

#ifndef VARIABLE_SUBSTITUTION_HPP
#define VARIABLE_SUBSTITUTION_HPP

#include <map>
#include <string>

namespace commands {

/// @brief Replace all ${variable_name} patterns in a string with values from the variables map.
///
/// Unresolved variables are left as-is.
/// @param input The string potentially containing ${variable} references
/// @param variables Map of variable names to their replacement values
/// @return The string with all resolved variables substituted
std::string substituteVariables(
        std::string const & input,
        std::map<std::string, std::string> const & variables);

/// @brief Normalize whole-number floating-point values in a JSON string to integers.
///
/// Converts values like `10.0` to `10` while leaving `10.5` and strings untouched.
/// This fixes an rfl::Generic round-trip issue where integers become doubles
/// after `rfl::json::write(Generic) → rfl::json::read<Generic>`, producing
/// values like `10.0` that fail to deserialize into int64_t fields.
std::string normalizeJsonNumbers(std::string const & json);

}// namespace commands

#endif// VARIABLE_SUBSTITUTION_HPP
