/**
 * @file ICommand.hpp
 * @brief Abstract base class for all commands operating on DataManager
 */

#ifndef ICOMMAND_HPP
#define ICOMMAND_HPP

#include "CommandContext.hpp"
#include "CommandResult.hpp"

#include <memory>
#include <string>

namespace commands {

/// @brief Base class for all commands operating on DataManager.
///
/// Each command holds its own parameters, implements execute(), and optionally
/// implements undo(). There is one interface for both execution and undo —
/// no separate IUndoableCommand hierarchy.
class ICommand {
public:
    virtual ~ICommand() = default;

    /// Execute the command against the DataManager in ctx
    virtual CommandResult execute(CommandContext const & ctx) = 0;

    /// Human-readable name used for journal entries and UI display
    [[nodiscard]] virtual std::string commandName() const = 0;

    /// Serialize parameters to JSON (via rfl::json::write on the params struct)
    [[nodiscard]] virtual std::string toJson() const = 0;

    /// Undo support (opt-in). Default: not undoable.
    [[nodiscard]] virtual bool isUndoable() const { return false; }

    virtual CommandResult undo(CommandContext const & ctx) {
        return CommandResult::error("Command does not support undo");
    }

    /// For undo merging (Phase 5). Same id = mergeable. -1 = never merge.
    [[nodiscard]] virtual int mergeId() const { return -1; }

    virtual bool mergeWith(ICommand const & /*other*/) { return false; }
};

}// namespace commands

#endif// ICOMMAND_HPP
