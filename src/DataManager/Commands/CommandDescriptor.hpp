/**
 * @file CommandDescriptor.hpp
 * @brief JSON-serializable descriptors for command invocations and sequences
 */

#ifndef COMMAND_DESCRIPTOR_HPP
#define COMMAND_DESCRIPTOR_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <rfl.hpp>
#include <rfl/json.hpp>

namespace commands {

/// @brief A single command invocation as it appears in a JSON file
struct CommandDescriptor {
    std::string command_name;              // Factory lookup key
    std::optional<rfl::Generic> parameters;// Command-specific params
    std::optional<std::string> description;// Human-readable
};

/// @brief A sequence of commands, optionally with template variables
struct CommandSequenceDescriptor {
    std::optional<std::string> name;
    std::optional<std::string> version;
    std::optional<std::map<std::string, std::string>> variables;// Static template vars
    std::vector<CommandDescriptor> commands;
};

}// namespace commands

#endif// COMMAND_DESCRIPTOR_HPP
