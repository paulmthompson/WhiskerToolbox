/**
 * @file register_datasynthesizer_commands.hpp
 * @brief Registration function for DataSynthesizer-owned commands
 */

#ifndef REGISTER_DATASYNTHESIZER_COMMANDS_HPP
#define REGISTER_DATASYNTHESIZER_COMMANDS_HPP

namespace WhiskerToolbox::DataSynthesizer {

/// @brief Register DataSynthesizer commands with the CommandRegistry singleton.
///
/// Must be called once at startup before any command creation or introspection.
/// Safe to call multiple times (subsequent calls are no-ops).
void register_datasynthesizer_commands();

}// namespace WhiskerToolbox::DataSynthesizer

#endif// REGISTER_DATASYNTHESIZER_COMMANDS_HPP
