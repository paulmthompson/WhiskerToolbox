/**
 * @file SequenceExecution.cpp
 * @brief Implementation of command sequence execution with variable substitution
 */

#include "SequenceExecution.hpp"

#include "CommandDescriptor.hpp"
#include "CommandFactory.hpp"
#include "CommandRecorder.hpp"
#include "ICommand.hpp"
#include "MutationGuard.hpp"
#include "VariableSubstitution.hpp"

#include <rfl/json.hpp>

namespace commands {

SequenceResult executeSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx,
        bool stop_on_error,
        CommandRecorder * recorder) {

    SequenceResult seq_result;
    std::vector<std::string> all_affected_keys;

    for (int i = 0; i < static_cast<int>(seq.commands.size()); ++i) {
        auto const & desc = seq.commands[static_cast<size_t>(i)];

        // Resolve parameters with two-pass variable substitution
        std::string resolved_json;
        if (desc.parameters.has_value()) {
            // Serialize to JSON string for substitution
            resolved_json = rfl::json::write(desc.parameters.value());

            // Pass 1: Static variables from the sequence descriptor
            if (seq.variables.has_value()) {
                resolved_json = substituteVariables(resolved_json, seq.variables.value());
            }

            // Pass 2: Runtime variables from the context
            if (!ctx.runtime_variables.empty()) {
                resolved_json = substituteVariables(resolved_json, ctx.runtime_variables);
            }
        }

        // Also substitute variables in the command name itself
        auto resolved_name = desc.command_name;
        if (seq.variables.has_value()) {
            resolved_name = substituteVariables(resolved_name, seq.variables.value());
        }
        if (!ctx.runtime_variables.empty()) {
            resolved_name = substituteVariables(resolved_name, ctx.runtime_variables);
        }

        // Create the command via factory (using JSON string directly to avoid
        // the rfl::Generic round-trip that converts integers to doubles)
        auto cmd = createCommandFromJson(resolved_name, resolved_json);
        if (!cmd) {
            seq_result.result = CommandResult::error(
                    "Unknown command '" + resolved_name + "' (index " +
                    std::to_string(i) + ")");
            seq_result.failed_index = i;
            if (stop_on_error) {
                return seq_result;
            }
            continue;
        }

        // Execute the command inside a CommandGuard scope
        CommandResult cmd_result;
        {
            CommandGuard const guard;
            cmd_result = cmd->execute(ctx);
        }
        if (!cmd_result.success) {
            seq_result.result = CommandResult::error(
                    "Command '" + resolved_name + "' failed (index " +
                    std::to_string(i) + "): " + cmd_result.error_message);
            seq_result.failed_index = i;
            if (stop_on_error) {
                // Still retain the command for potential partial undo
                seq_result.executed_commands.push_back(std::move(cmd));
                return seq_result;
            }
        }

        // Record the resolved descriptor on success
        if (cmd_result.success && recorder != nullptr) {
            CommandDescriptor resolved_desc;
            resolved_desc.command_name = resolved_name;
            resolved_desc.description = desc.description;
            // Parse the resolved JSON to rfl::Generic for storage in the descriptor.
            // This may convert integers to doubles, but createCommandFromJson()
            // applies normalizeJsonNumbers() on replay, so the round-trip is safe.
            auto parsed_for_record = rfl::json::read<rfl::Generic>(resolved_json);
            if (parsed_for_record) {
                resolved_desc.parameters = std::move(parsed_for_record.value());
            }
            recorder->record(std::move(resolved_desc));
        }

        all_affected_keys.insert(
                all_affected_keys.end(),
                cmd_result.affected_keys.begin(),
                cmd_result.affected_keys.end());

        seq_result.executed_commands.push_back(std::move(cmd));
    }

    // Only report success if no command failed
    if (seq_result.failed_index == -1) {
        seq_result.result = CommandResult::ok(std::move(all_affected_keys));
    } else {
        // Preserve the error from the last failure, but merge affected keys from
        // commands that did succeed
        seq_result.result.affected_keys = std::move(all_affected_keys);
    }
    return seq_result;
}

}// namespace commands
