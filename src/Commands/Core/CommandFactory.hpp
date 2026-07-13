/**
 * @file CommandFactory.hpp
 * @brief Factory functions mapping command name strings to concrete ICommand instances,
 *        plus introspection query functions for command metadata.
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

/**
 * @brief Check if a command name is recognized by the factory.
 *
 * Returns true for all command names that createCommand() can instantiate.
 *
 * @param name Command name to query.
 * @return True if @p name is registered in CommandRegistry.
 */
bool isKnownCommandName(std::string const & name);

/**
 * @brief Create a command from a descriptor's name and rfl::Generic parameters.
 *
 * Delegates to createCommandFromJson() after serializing and normalizing the JSON.
 *
 * @param name Registered command name.
 * @param params Command parameters as a generic JSON value.
 * @return The constructed command, or nullptr if @p name is unknown or @p params
 *         fail to parse.
 */
std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params);

/**
 * @brief Create a command from a name and a pre-built JSON parameter string.
 *
 * Applies integer normalization to handle the rfl::Generic round-trip issue
 * (whole-number doubles like 10.0 are converted to 10 before deserialization).
 * Prefer this overload when a JSON string is already available to avoid an
 * unnecessary rfl::Generic round-trip.
 *
 * @param name Registered command name.
 * @param params_json JSON parameter string for the command.
 * @return The constructed command, or nullptr if @p name is unknown or @p params_json
 *         fails to parse.
 */
std::unique_ptr<ICommand> createCommandFromJson(
        std::string const & name,
        std::string const & params_json);

/**
 * @brief Return metadata for all registered commands.
 *
 * Each entry includes the command's name, description, category,
 * undo support flag, supported data types, and a ParameterSchema
 * generated via extractParameterSchema<T>().
 *
 * @return Vector of CommandInfo for every registered command.
 */
std::vector<CommandInfo> getAvailableCommands();

/**
 * @brief Return metadata for a single command by name.
 *
 * @param name Registered command name to look up.
 * @return The CommandInfo for @p name, or std::nullopt if the name is not recognized.
 */
std::optional<CommandInfo> getCommandInfo(std::string const & name);

}// namespace commands

#endif// COMMAND_FACTORY_HPP
