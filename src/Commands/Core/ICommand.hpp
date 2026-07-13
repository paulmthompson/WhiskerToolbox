/**
 * @file ICommand.hpp
 * @brief Abstract base class for all commands operating on DataManager.
 */

#ifndef ICOMMAND_HPP
#define ICOMMAND_HPP

#include "CommandContext.hpp"
#include "CommandResult.hpp"

#include <memory>
#include <string>

namespace commands {

/**
 * @brief Base class for all commands operating on DataManager.
 *
 * Each command holds its own parameters, implements execute(), and optionally
 * implements undo(). There is one interface for both execution and undo —
 * no separate IUndoableCommand hierarchy.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Execute the command against the DataManager in @p ctx.
     *
     * @param ctx Runtime execution context (DataManager, time controller, etc.).
     * @return Success or failure result, including affected data keys on success.
     */
    virtual CommandResult execute(CommandContext const & ctx) = 0;

    /**
     * @brief Return the human-readable command name.
     *
     * Used for journal entries and UI display. Must match the name registered
     * in CommandRegistry.
     *
     * @return Stable command name string.
     */
    [[nodiscard]] virtual std::string commandName() const = 0;

    /**
     * @brief Serialize command parameters to JSON.
     *
     * Typically implemented via `rfl::json::write` on the command's params struct.
     *
     * @return JSON string representing the command parameters.
     */
    [[nodiscard]] virtual std::string toJson() const = 0;

    /**
     * @brief Report whether this command supports undo.
     *
     * @return True if undo() is implemented; false by default.
     */
    [[nodiscard]] virtual bool isUndoable() const { return false; }

    /**
     * @brief Revert the effects of a prior execute() call.
     *
     * @param ctx Runtime execution context (DataManager, time controller, etc.).
     * @return Success or failure result. Default implementation returns an error
     *         indicating undo is not supported.
     */
    virtual CommandResult undo(CommandContext const & ctx) {
        return CommandResult::error("Command does not support undo");
    }

    /**
     * @brief Return an identifier used for undo merging.
     *
     * Commands with the same non-negative merge id may be coalesced into a
     * single undo step. A value of -1 means the command is never mergeable.
     *
     * @return Merge identifier, or -1 if not mergeable.
     */
    [[nodiscard]] virtual int mergeId() const { return -1; }

    /**
     * @brief Attempt to merge this command with a subsequent command of the same type.
     *
     * Called when two consecutive executed commands share the same mergeId().
     * If merge succeeds, only one undo step is retained.
     *
     * @param other The subsequent command candidate for merging.
     * @return True if @p other was absorbed into this command; false by default.
     */
    virtual bool mergeWith(ICommand const & other) { return false; }
};

}// namespace commands

#endif// ICOMMAND_HPP
