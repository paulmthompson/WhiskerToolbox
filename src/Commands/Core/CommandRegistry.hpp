/**
 * @file CommandRegistry.hpp
 * @brief Singleton registry mapping command names to creator functions and metadata,
 *        plus a convenience helper for registering typed commands with automatic
 *        JSON deserialization and schema extraction.
 */

#ifndef COMMAND_REGISTRY_HPP
#define COMMAND_REGISTRY_HPP

#include "CommandInfo.hpp"
#include "ICommand.hpp"
#include "VariableSubstitution.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/json.hpp>

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace commands {

/// Type-erased command creator: takes normalized JSON, returns ICommand.
using CommandCreator = std::function<std::unique_ptr<ICommand>(std::string const &)>;

/// @brief Singleton registry mapping command name strings to creator functions and metadata.
///
/// Replaces the hardcoded if-else chain in CommandFactory. Libraries register their
/// commands via explicit dispatcher functions (e.g., register_core_commands()) that
/// call registerCommand() on this singleton.
///
/// @note Thread-safety: Registration is expected to happen once at startup, before
/// any concurrent access. Query methods are safe for concurrent reads after
/// registration is complete.
class CommandRegistry {
public:
    /// @brief Access the singleton instance.
    static CommandRegistry & instance();

    /// @brief Register a command with its creator function and metadata.
    /// @pre name must not already be registered (asserts in debug builds).
    /// @pre name must not be empty (asserts in debug builds).
    void registerCommand(std::string name, CommandCreator creator, CommandInfo info);

    /// @brief Check if a command name has been registered.
    [[nodiscard]] bool isKnown(std::string const & name) const;

    /// @brief Create a command instance from its name and a normalized JSON parameter string.
    /// @return The constructed command, or nullptr if the name is unknown or params fail to parse.
    [[nodiscard]] std::unique_ptr<ICommand> create(
            std::string const & name,
            std::string const & params_json) const;

    /// @brief Return metadata for all registered commands.
    [[nodiscard]] std::vector<CommandInfo> availableCommands() const;

    /// @brief Return metadata for a single command by name.
    /// @return The CommandInfo, or std::nullopt if the name is not registered.
    [[nodiscard]] std::optional<CommandInfo> commandInfo(std::string const & name) const;

    /// @brief Return the number of registered commands.
    [[nodiscard]] std::size_t size() const;

    /// @brief Remove all registered commands.
    ///
    /// Intended for test isolation only — not for production use.
    void clear();

    // Non-copyable, non-movable
    CommandRegistry(CommandRegistry const &) = delete;
    CommandRegistry & operator=(CommandRegistry const &) = delete;
    CommandRegistry(CommandRegistry &&) = delete;
    CommandRegistry & operator=(CommandRegistry &&) = delete;

private:
    CommandRegistry() = default;

    struct Entry {
        CommandCreator creator;
        CommandInfo info;
    };

    std::unordered_map<std::string, Entry> _commands;
};

/// @brief Convenience helper: register a command with automatic JSON deserialization
///        and ParameterSchema extraction.
///
/// The creator function deserializes the JSON string into @p Params via rfl::json::read,
/// then constructs a @p CommandT from the parsed params. The parameter_schema field
/// of @p info is automatically populated via extractParameterSchema<Params>().
///
/// @tparam CommandT  Concrete ICommand subclass (must accept Params in its constructor)
/// @tparam Params    reflect-cpp–annotated parameter struct
/// @param reg   The registry to register into
/// @param name  Command name string (must match CommandT::commandName())
/// @param info  Metadata — parameter_schema will be overwritten
template<typename CommandT, typename Params>
void registerTypedCommand(CommandRegistry & reg, std::string name, CommandInfo info) {
    info.parameter_schema = WhiskerToolbox::Transforms::V2::extractParameterSchema<Params>();
    reg.registerCommand(
            std::move(name),
            [](std::string const & params_json) -> std::unique_ptr<ICommand> {
                auto const json = normalizeJsonNumbers(params_json);
                auto p = rfl::json::read<Params>(json);
                if (!p) {
                    return nullptr;
                }
                return std::make_unique<CommandT>(std::move(*p));
            },
            std::move(info));
}

}// namespace commands

#endif// COMMAND_REGISTRY_HPP
