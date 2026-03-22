/**
 * @file CommandRegistry.cpp
 * @brief Implementation of the CommandRegistry singleton
 */

#include "CommandRegistry.hpp"

#include <cassert>

namespace commands {

CommandRegistry & CommandRegistry::instance() {
    static CommandRegistry registry;
    return registry;
}

void CommandRegistry::registerCommand(std::string name, CommandCreator creator, CommandInfo info) {
    assert(!name.empty() && "CommandRegistry: name must not be empty");
    assert(_commands.find(name) == _commands.end() &&
           "CommandRegistry: duplicate command registration");

    _commands.emplace(std::move(name), Entry{std::move(creator), std::move(info)});
}

bool CommandRegistry::isKnown(std::string const & name) const {
    return _commands.contains(name);
}

std::unique_ptr<ICommand> CommandRegistry::create(
        std::string const & name,
        std::string const & params_json) const {
    auto const it = _commands.find(name);
    if (it == _commands.end()) {
        return nullptr;
    }
    return it->second.creator(params_json);
}

std::vector<CommandInfo> CommandRegistry::availableCommands() const {
    std::vector<CommandInfo> result;
    result.reserve(_commands.size());
    for (auto const & [key, entry]: _commands) {
        result.push_back(entry.info);
    }
    return result;
}

std::optional<CommandInfo> CommandRegistry::commandInfo(std::string const & name) const {
    auto const it = _commands.find(name);
    if (it == _commands.end()) {
        return std::nullopt;
    }
    return it->second.info;
}

std::size_t CommandRegistry::size() const {
    return _commands.size();
}

void CommandRegistry::clear() {
    _commands.clear();
}

}// namespace commands
