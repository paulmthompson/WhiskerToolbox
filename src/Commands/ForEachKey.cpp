/**
 * @file ForEachKey.cpp
 * @brief Implementation of the ForEachKey meta-command
 */

#include "ForEachKey.hpp"

#include "Core/CommandContext.hpp"
#include "Core/CommandFactory.hpp"
#include "Core/VariableSubstitution.hpp"

#include <algorithm>

#include <ranges>
#include <rfl/json.hpp>

namespace commands {

ForEachKey::ForEachKey(ForEachKeyParams p)
    : _params(std::move(p)) {}

std::string ForEachKey::commandName() const { return "ForEachKey"; }

std::string ForEachKey::toJson() const { return rfl::json::write(_params); }

bool ForEachKey::isUndoable() const {
    return std::ranges::any_of(_executed_commands,
                               [](auto const & cmd) { return cmd->isUndoable(); });
}

CommandResult ForEachKey::execute(CommandContext const & ctx) {
    _executed_commands.clear();
    std::vector<std::string> all_affected_keys;

    for (auto const & item: _params.items) {
        // Build substitution map: bind current item to ${variable}
        std::map<std::string, std::string> item_vars = ctx.runtime_variables;
        item_vars[_params.variable] = item;

        for (auto const & desc: _params.commands) {
            // Substitute variables in parameters
            rfl::Generic resolved_params;
            if (desc.parameters.has_value()) {
                auto json_str = rfl::json::write(desc.parameters.value());
                json_str = substituteVariables(json_str, item_vars);

                auto parsed = rfl::json::read<rfl::Generic>(json_str);
                if (!parsed) {
                    return CommandResult::error(
                            "Failed to parse parameters for sub-command '" +
                            desc.command_name + "' with " + _params.variable +
                            "=" + item);
                }
                resolved_params = std::move(parsed.value());
            }

            // Substitute variables in command name
            auto resolved_name = substituteVariables(desc.command_name, item_vars);

            auto cmd = createCommand(resolved_name, resolved_params);
            if (!cmd) {
                std::string msg = "Unknown sub-command '";
                msg += resolved_name;
                msg += "' with ";
                msg += _params.variable;
                msg += "=";
                msg += item;
                return CommandResult::error(std::move(msg));
            }

            auto result = cmd->execute(ctx);
            if (!result.success) {
                _executed_commands.push_back(std::move(cmd));
                std::string msg = "Sub-command '";
                msg += resolved_name;
                msg += "' failed with ";
                msg += _params.variable;
                msg += "=";
                msg += item;
                msg += ": ";
                msg += result.error_message;
                return CommandResult::error(std::move(msg));
            }

            all_affected_keys.insert(
                    all_affected_keys.end(),
                    result.affected_keys.begin(),
                    result.affected_keys.end());

            _executed_commands.push_back(std::move(cmd));
        }
    }

    return CommandResult::ok(std::move(all_affected_keys));
}

CommandResult ForEachKey::undo(CommandContext const & ctx) {
    // Undo in reverse order, skipping non-undoable commands
    for (auto & _executed_command : std::ranges::reverse_view(_executed_commands)) {
        if (_executed_command->isUndoable()) {
            auto result = _executed_command->undo(ctx);
            if (!result.success) {
                return CommandResult::error(
                        "Undo failed for sub-command '" +
                        _executed_command->commandName() + "': " + result.error_message);
            }
        }
    }
    _executed_commands.clear();
    return CommandResult::ok();
}

}// namespace commands
