/**
 * @file CommandFactory.hpp
 * @brief Factory function mapping command name strings to concrete ICommand instances
 */

#ifndef COMMAND_FACTORY_HPP
#define COMMAND_FACTORY_HPP

#include <memory>
#include <string>

#include <rfl.hpp>

namespace commands {

class ICommand;

/// @brief Create a command from a descriptor's name and rfl::Generic parameters.
///
/// Returns nullptr if the command name is unknown or parameters fail to parse.
/// This is the single place that maps command name strings to concrete command types.
std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params);

}// namespace commands

#endif// COMMAND_FACTORY_HPP
