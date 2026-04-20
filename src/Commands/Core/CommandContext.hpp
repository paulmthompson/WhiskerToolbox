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
class TimeController;

namespace commands {

class CommandRecorder;

/// @brief Runtime context provided to every command at execution time.
///        This is not serialized — it is the environment that commands run in.
struct CommandContext {
    std::shared_ptr<DataManager> data_manager;

    /// Optional visualization time (e.g. from `EditorRegistry::timeController()`).
    /// Commands such as `AdvanceFrame` require this to be non-null.
    TimeController * time_controller = nullptr;

    /// Runtime-resolved values (current_frame, mark_frame, etc.)
    /// Used for ${variable} substitution in command parameters
    std::map<std::string, std::string> runtime_variables;

    /// Optional progress reporting
    std::function<void(float)> on_progress;

    /// Optional recorder for capturing executed command descriptors.
    /// Used by executeSequence() as a fallback when no explicit recorder is passed.
    CommandRecorder * recorder = nullptr;
};

}// namespace commands

#endif// COMMAND_CONTEXT_HPP
