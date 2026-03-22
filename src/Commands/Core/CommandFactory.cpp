/**
 * @file CommandFactory.cpp
 * @brief Thin wrappers delegating to CommandRegistry for backward compatibility
 */

#include "CommandFactory.hpp"

#include "CommandRegistry.hpp"
#include "ICommand.hpp"
#include "VariableSubstitution.hpp"

#include <rfl/json.hpp>

namespace commands {

bool isKnownCommandName(std::string const & name) {
    return CommandRegistry::instance().isKnown(name);
}

std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params) {

    return createCommandFromJson(name, rfl::json::write(params));
}

std::unique_ptr<ICommand> createCommandFromJson(
        std::string const & name,
        std::string const & params_json) {

    auto const json = normalizeJsonNumbers(params_json);
    return CommandRegistry::instance().create(name, json);
}

std::vector<CommandInfo> getAvailableCommands() {
    return CommandRegistry::instance().availableCommands();
}

std::optional<CommandInfo> getCommandInfo(std::string const & name) {
    return CommandRegistry::instance().commandInfo(name);
}

}// namespace commands
