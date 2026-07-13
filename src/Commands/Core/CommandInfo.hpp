/**
 * @file CommandInfo.hpp
 * @brief Static metadata for a registered command, including its ParameterSchema.
 */

#ifndef COMMAND_INFO_HPP
#define COMMAND_INFO_HPP

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>
#include <vector>

namespace commands {

/**
 * @brief Static metadata describing a registered command.
 *
 * One CommandInfo exists per command type. It provides the information
 * needed for UI command pickers, guided editors, and introspection.
 */
struct CommandInfo {
    /** @brief Command display and registry name (must match ICommand::commandName()). */
    std::string name;

    /** @brief Human-readable description shown in command pickers and editors. */
    std::string description;

    /**
     * @brief Logical grouping for UI organization.
     *
     * Examples: "data_mutation", "meta", "persistence", "navigation".
     */
    std::string category;

    /** @brief True if the command implements undo via ICommand::undo(). */
    bool supports_undo = false;

    /**
     * @brief Data types this command can operate on.
     *
     * Empty when the command is not tied to a specific data object type.
     */
    std::vector<std::string> supported_data_types;

    /**
     * @brief Schema describing the command's JSON parameters.
     *
     * Populated automatically by registerTypedCommand() or set manually
     * when registering via CommandRegistry::registerCommand().
     */
    ParameterSchema parameter_schema;
};

}// namespace commands

#endif// COMMAND_INFO_HPP
