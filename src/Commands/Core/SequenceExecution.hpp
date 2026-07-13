/**
 * @file SequenceExecution.hpp
 * @brief Execution of command sequences with variable substitution.
 */

#ifndef SEQUENCE_EXECUTION_HPP
#define SEQUENCE_EXECUTION_HPP

#include "CommandContext.hpp"
#include "CommandResult.hpp"
#include "ICommand.hpp"

#include <memory>
#include <string>
#include <vector>

namespace commands {

class CommandRecorder;
struct CommandSequenceDescriptor;

/**
 * @brief Result of executing a command sequence.
 */
struct SequenceResult {
    /** @brief Overall success or failure result for the sequence. */
    CommandResult result;

    /**
     * @brief Commands that were successfully executed.
     *
     * Retained for undo support and partial-failure recovery.
     */
    std::vector<std::unique_ptr<ICommand>> executed_commands;

    /** @brief Index of the command that failed, or -1 if all succeeded. */
    int failed_index = -1;
};

/**
 * @brief Execute a sequence of command descriptors.
 *
 * Applies two-pass variable substitution:
 *   1. Static pass from CommandSequenceDescriptor::variables
 *   2. Runtime pass from CommandContext::runtime_variables
 *
 * Then creates and executes each command in order.
 *
 * @param seq The command sequence descriptor (JSON-deserialized).
 * @param ctx Runtime context with DataManager and runtime variables.
 * @param stop_on_error If true, stop on the first command failure (default: true).
 * @param recorder Optional CommandRecorder to record each successfully executed command's
 *        descriptor. Pass nullptr (default) to disable recording. Falls back to
 *        ctx.recorder when nullptr.
 * @return SequenceResult containing the overall result, executed commands, and failure index.
 */
SequenceResult executeSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx,
        bool stop_on_error = true,
        CommandRecorder * recorder = nullptr);

/**
 * @brief Execute a single command through the sequence execution pipeline.
 *
 * Convenience wrapper that routes a single command through executeSequence(),
 * enabling recording (via ctx.recorder) and MutationGuard support.
 *
 * @param command_name The command factory name (e.g., "SaveData", "SynthesizeData").
 * @param params_json JSON-serialized command parameters.
 * @param ctx Runtime context with DataManager, runtime variables, and optional recorder.
 * @return CommandResult indicating success or failure.
 */
CommandResult executeSingleCommand(
        std::string const & command_name,
        std::string const & params_json,
        CommandContext const & ctx);

}// namespace commands

#endif// SEQUENCE_EXECUTION_HPP
