/**
 * @file SequenceExecution.cpp
 * @brief Implementation of command sequence execution with variable substitution
 */

#include "SequenceExecution.hpp"

#include "CommandDescriptor.hpp"
#include "CommandFactory.hpp"
#include "ICommand.hpp"
#include "VariableSubstitution.hpp"

#include <rfl/json.hpp>

namespace commands {

SequenceResult executeSequence(
        CommandSequenceDescriptor const & seq,
        CommandContext const & ctx,
        bool stop_on_error) {

    SequenceResult seq_result;
    std::vector<std::string> all_affected_keys;

    for (int i = 0; i < static_cast<int>(seq.commands.size()); ++i) {
        auto const & desc = seq.commands[static_cast<size_t>(i)];

        // Resolve parameters with two-pass variable substitution
        rfl::Generic resolved_params;
        if (desc.parameters.has_value()) {
            // Serialize to JSON string for substitution
            auto json_str = rfl::json::write(desc.parameters.value());

            // Pass 1: Static variables from the sequence descriptor
            if (seq.variables.has_value()) {
                json_str = substituteVariables(json_str, seq.variables.value());
            }

            // Pass 2: Runtime variables from the context
            if (!ctx.runtime_variables.empty()) {
                json_str = substituteVariables(json_str, ctx.runtime_variables);
            }

            // Deserialize back to rfl::Generic
            auto parsed = rfl::json::read<rfl::Generic>(json_str);
            if (!parsed) {
                seq_result.result = CommandResult::error(
                        "Failed to parse substituted parameters for command '" +
                        desc.command_name + "' (index " + std::to_string(i) + ")");
                seq_result.failed_index = i;
                return seq_result;
            }
            resolved_params = std::move(parsed.value());
        }

        // Also substitute variables in the command name itself
        auto resolved_name = desc.command_name;
        if (seq.variables.has_value()) {
            resolved_name = substituteVariables(resolved_name, seq.variables.value());
        }
        if (!ctx.runtime_variables.empty()) {
            resolved_name = substituteVariables(resolved_name, ctx.runtime_variables);
        }

        // Create the command via factory
        auto cmd = createCommand(resolved_name, resolved_params);
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

        // Execute the command
        auto cmd_result = cmd->execute(ctx);
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
