/**
 * @file CommandInfo.hpp
 * @brief Static metadata for a registered command, including its ParameterSchema
 */

#ifndef COMMAND_INFO_HPP
#define COMMAND_INFO_HPP

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>
#include <vector>

namespace commands {

/// @brief Static metadata describing a registered command.
///
/// One CommandInfo exists per command type. It provides the information
/// needed for UI command pickers, guided editors, and introspection.
struct CommandInfo {
    std::string name;
    std::string description;
    std::string category;///< "data_mutation", "meta", "persistence", etc.
    bool supports_undo = false;
    std::vector<std::string> supported_data_types;
    WhiskerToolbox::Transforms::V2::ParameterSchema parameter_schema;
};

}// namespace commands

#endif// COMMAND_INFO_HPP
