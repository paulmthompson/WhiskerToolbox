/**
 * @file CommandContext.hpp
 * @brief Runtime context provided to commands during execution
 */

#ifndef COMMAND_CONTEXT_HPP
#define COMMAND_CONTEXT_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>

class DataManager;

namespace commands {

/// @brief Runtime context provided to every command at execution time.
///        This is not serialized — it is the environment that commands run in.
struct CommandContext {
    std::shared_ptr<DataManager> data_manager;

    /// Runtime-resolved values (current_frame, mark_frame, etc.)
    /// Used for ${variable} substitution in command parameters
    std::map<std::string, std::string> runtime_variables;

    /// Optional progress reporting
    std::function<void(float)> on_progress;
};

}// namespace commands

#endif// COMMAND_CONTEXT_HPP
