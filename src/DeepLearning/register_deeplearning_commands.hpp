/**
 * @file register_deeplearning_commands.hpp
 * @brief Registration function for DeepLearning-owned commands
 */
#ifndef REGISTER_DEEPLEARNING_COMMANDS_HPP
#define REGISTER_DEEPLEARNING_COMMANDS_HPP

namespace dl {

/// @brief Register DeepLearning commands with the CommandRegistry singleton.
///
/// Must be called once at startup before any command creation or introspection.
/// Safe to call multiple times (subsequent calls are no-ops).
void register_deeplearning_commands();

}// namespace dl

#endif// REGISTER_DEEPLEARNING_COMMANDS_HPP
