/**
 * @file ForEachKey.hpp
 * @brief Meta-command that iterates a list of values and executes sub-commands for each
 */

#ifndef FOR_EACH_KEY_COMMAND_HPP
#define FOR_EACH_KEY_COMMAND_HPP

#include "Core/CommandDescriptor.hpp"
#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include <memory>
#include <string>
#include <vector>

#include <rfl/json.hpp>

namespace commands {

struct ForEachKeyParams {
    std::vector<std::string> items;
    std::string variable;
    std::vector<CommandDescriptor> commands;
};

class ForEachKey : public ICommand {
public:
    explicit ForEachKey(ForEachKeyParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] bool isUndoable() const override;

    CommandResult execute(CommandContext const & ctx) override;
    CommandResult undo(CommandContext const & ctx) override;

private:
    ForEachKeyParams _params;
    std::vector<std::unique_ptr<ICommand>> _executed_commands;
};

}// namespace commands

#endif// FOR_EACH_KEY_COMMAND_HPP
