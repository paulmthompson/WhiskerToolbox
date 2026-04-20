/**
 * @file AdvanceFrame.cpp
 * @brief Implementation of AdvanceFrame command
 */

#include "AdvanceFrame.hpp"

#include "Core/CommandContext.hpp"

#include "TimeController/TimeController.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

AdvanceFrame::AdvanceFrame(AdvanceFrameParams p)
    : _params(std::move(p)) {}

std::string AdvanceFrame::commandName() const {
    return "AdvanceFrame";
}

std::string AdvanceFrame::toJson() const {
    return rfl::json::write(_params);
}

CommandResult AdvanceFrame::execute(CommandContext const & ctx) {
    if (ctx.time_controller == nullptr) {
        return CommandResult::error("AdvanceFrame requires CommandContext::time_controller");
    }

    auto const pos = ctx.time_controller->currentPosition();
    auto const new_index = TimeFrameIndex(pos.index.getValue() + _params.delta);
    ctx.time_controller->setCurrentTime(TimePosition(new_index, pos.time_frame));
    return CommandResult::ok({});
}

}// namespace commands
