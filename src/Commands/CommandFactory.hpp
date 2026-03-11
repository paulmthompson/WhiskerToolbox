/**
 * @file CommandFactory.hpp
 * @brief Factory function mapping command name strings to concrete ICommand instances,
 *        plus introspection query functions for command metadata
 */

#ifndef COMMAND_FACTORY_HPP
#define COMMAND_FACTORY_HPP

#include "CommandInfo.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <rfl.hpp>

namespace commands {

class ICommand;

/// @brief Check if a command name is recognized by the factory.
///
/// Returns true for all command names that createCommand() can instantiate.
bool isKnownCommandName(std::string const & name);

/// @brief Create a command from a descriptor's name and rfl::Generic parameters.
///
/// Returns nullptr if the command name is unknown or parameters fail to parse.
/// Delegates to createCommandFromJson() after serializing and normalizing the JSON.
std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params);

/// @brief Create a command from a name and a pre-built JSON parameter string.
///
/// Applies integer normalization to handle the rfl::Generic round-trip issue
/// (whole-number doubles like 10.0 are converted to 10 before deserialization).
/// Prefer this overload when a JSON string is already available to avoid an
/// unnecessary rfl::Generic round-trip.
///
/// Returns nullptr if the command name is unknown or parameters fail to parse.
std::unique_ptr<ICommand> createCommandFromJson(
        std::string const & name,
        std::string const & params_json);

/// @brief Return metadata for all registered commands.
///
/// Each entry includes the command's name, description, category,
/// undo support flag, supported data types, and a ParameterSchema
/// generated via extractParameterSchema<T>().
std::vector<CommandInfo> getAvailableCommands();

/// @brief Return metadata for a single command by name.
///
/// Returns std::nullopt if the command name is not recognized.
std::optional<CommandInfo> getCommandInfo(std::string const & name);

}// namespace commands

#endif// COMMAND_FACTORY_HPP
