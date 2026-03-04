/**
 * @file VariableSubstitution.cpp
 * @brief Implementation of ${variable} substitution for command sequences
 */

#include "VariableSubstitution.hpp"

namespace commands {

std::string substituteVariables(
        std::string const & input,
        std::map<std::string, std::string> const & variables) {

    std::string result = input;
    std::string::size_type pos = 0;

    while ((pos = result.find("${", pos)) != std::string::npos) {
        auto const end_pos = result.find('}', pos + 2);
        if (end_pos == std::string::npos) {
            // Malformed variable reference — skip past "${"
            pos += 2;
            continue;
        }

        auto const var_name = result.substr(pos + 2, end_pos - pos - 2);
        auto const it = variables.find(var_name);

        if (it != variables.end()) {
            result.replace(pos, end_pos - pos + 1, it->second);
            pos += it->second.length();
        } else {
            // Variable not found — leave as-is and skip past it
            pos = end_pos + 1;
        }
    }

    return result;
}

}// namespace commands
