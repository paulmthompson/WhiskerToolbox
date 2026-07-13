/**
 * @file CommandContext.hpp
 * @brief Runtime context provided to commands during execution.
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

/**
 * @brief Runtime context provided to every command at execution time.
 *
 * This struct is not serialized — it is the environment that commands run in.
 */
struct CommandContext {
    /**
     * @brief Shared DataManager that commands read from and mutate.
     *
     * Must be non-null for most commands.
     */
    std::shared_ptr<DataManager> data_manager;

    /**
     * @brief Optional visualization time controller.
     *
     * Typically sourced from `EditorRegistry::timeController()`.
     * Commands such as `AdvanceFrame` require this to be non-null.
     */
    TimeController * time_controller = nullptr;

    /**
     * @brief Runtime-resolved values for ${variable} substitution in command parameters.
     *
     * Examples include `current_frame` and `mark_frame`.
     */
    std::map<std::string, std::string> runtime_variables;

    /**
     * @brief Optional progress callback invoked during long-running commands.
     *
     * Receives values in the range [0.0, 1.0].
     */
    std::function<void(float)> on_progress;

    /**
     * @brief Optional recorder for capturing executed command descriptors.
     *
     * Used by executeSequence() as a fallback when no explicit recorder is passed.
     */
    CommandRecorder * recorder = nullptr;
};

}// namespace commands

#endif// COMMAND_CONTEXT_HPP
