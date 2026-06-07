/**
 * @file register_transformsv2_commands.hpp
 * @brief Registration function for TransformsV2-owned commands
 */

#ifndef REGISTER_TRANSFORMSV2_COMMANDS_HPP
#define REGISTER_TRANSFORMSV2_COMMANDS_HPP

namespace WhiskerToolbox::Transforms::V2 {

/// @brief Register TransformsV2 commands with the CommandRegistry singleton.
///
/// Must be called once at startup before any command creation or introspection.
/// Safe to call multiple times (subsequent calls are no-ops once commands exist).
void register_transformsv2_commands();

}// namespace WhiskerToolbox::Transforms::V2

#endif// REGISTER_TRANSFORMSV2_COMMANDS_HPP
