/**
 * @file SequenceExecution.hpp
 * @brief Execution of command sequences with variable substitution
 */

#ifndef SEQUENCE_EXECUTION_HPP
#define SEQUENCE_EXECUTION_HPP

#include "CommandContext.hpp"
#include "CommandResult.hpp"

#include <memory>
#include <string>
#include <vector>

namespace commands {

class CommandRecorder;
class ICommand;
struct CommandSequenceDescriptor;

/// @brief Result of executing a command sequence
struct SequenceResult {
    CommandResult result;

    /// Commands that were successfully executed (retained for undo support)
    std::vector<std::unique_ptr<ICommand>> executed_commands;

    /// Index of the command that failed (-1 if all succeeded)
    int failed_index = -1;
};

/// @brief Execute a sequence of command descriptors.
///
/// Applies two-pass variable substitution:
///   1. Static pass from CommandSequenceDescriptor::variables
///   2. Runtime pass from CommandContext::runtime_variables
///
/// Then creates and executes each command in order.
/// @param seq The command sequence descriptor (JSON-deserialized)
/// @param ctx Runtime context with DataManager and runtime variables
/// @param stop_on_error If true, stop on the first command failure (default: true)
/// @param recorder Optional CommandRecorder to record each successfully executed command's
///        descriptor. Pass nullptr (default) to disable recording.
/// @return SequenceResult containing the overall result and executed commands
SequenceResult executeSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx,
        bool stop_on_error = true,
        CommandRecorder * recorder = nullptr);

}// namespace commands

#endif// SEQUENCE_EXECUTION_HPP
