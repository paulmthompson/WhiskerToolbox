/**
 * @file MoveByTimeRange.hpp
 * @brief Command to move entities in a time range between same-type data objects
 */

#ifndef MOVE_BY_TIME_RANGE_HPP
#define MOVE_BY_TIME_RANGE_HPP

#include "Core/CommandResult.hpp"
#include "Core/ICommand.hpp"

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <string>
#include <unordered_set>

#include <rfl/json.hpp>

namespace commands {

struct MoveByTimeRangeParams {
    std::string source_key;
    std::string destination_key;
    int64_t start_frame;
    int64_t end_frame;
};

class MoveByTimeRange : public ICommand {
public:
    explicit MoveByTimeRange(MoveByTimeRangeParams p);

    [[nodiscard]] std::string commandName() const override;
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] bool isUndoable() const override;

    CommandResult execute(CommandContext const & ctx) override;
    CommandResult undo(CommandContext const & ctx) override;

private:
    MoveByTimeRangeParams _params;
    std::unordered_set<EntityId> _moved_ids;
};

}// namespace commands

#endif// MOVE_BY_TIME_RANGE_HPP
