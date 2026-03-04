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

}// namespace commands

#endif// VARIABLE_SUBSTITUTION_HPP
