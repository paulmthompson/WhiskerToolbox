/**
 * @file register_core_commands.hpp
 * @brief Registration function for all built-in commands (MoveByTimeRange, CopyByTimeRange, etc.)
 */

#ifndef REGISTER_CORE_COMMANDS_HPP
#define REGISTER_CORE_COMMANDS_HPP

namespace commands {

/// @brief Register all built-in commands with the CommandRegistry singleton.
///
/// Must be called once at startup before any command creation or introspection.
/// Safe to call multiple times (subsequent calls are no-ops).
void register_core_commands();

}// namespace commands

#endif// REGISTER_CORE_COMMANDS_HPP
