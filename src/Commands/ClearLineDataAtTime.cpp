/**
 * @file ClearLineDataAtTime.cpp
 * @brief Implementation of ClearLineDataAtTime command
 */

#include "ClearLineDataAtTime.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeController/TimeController.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

namespace {

/// @brief Build a time index and frame for `clearAtTime` timeframe conversion.
[[nodiscard]] TimeIndexAndFrame makeTimeIndexAndFrame(
        CommandContext const & ctx,
        ClearLineDataAtTimeParams const & params) {
    auto const time_index = TimeFrameIndex(params.time);

    if (ctx.time_controller != nullptr) {
        return TimeIndexAndFrame(time_index, ctx.time_controller->currentTimeFrame().get());
    }

    auto const time_key = ctx.data_manager->getTimeKey(params.data_key);
    auto const time_frame = ctx.data_manager->getTime(time_key);
    return TimeIndexAndFrame(time_index, time_frame.get());
}

}// namespace

ClearLineDataAtTime::ClearLineDataAtTime(ClearLineDataAtTimeParams p)
    : _params(std::move(p)) {}

std::string ClearLineDataAtTime::commandName() const {
    return "ClearLineDataAtTime";
}

std::string ClearLineDataAtTime::toJson() const {
    return rfl::json::write(_params);
}

CommandResult ClearLineDataAtTime::execute(CommandContext const & ctx) {
    if (!ctx.data_manager) {
        return CommandResult::error("ClearLineDataAtTime requires CommandContext::data_manager");
    }

    auto line_data = ctx.data_manager->getData<LineData>(_params.data_key);
    if (!line_data) {
        return CommandResult::error(
                "ClearLineDataAtTime: no LineData at key '" + _params.data_key + "'");
    }

    auto const time_index_and_frame = makeTimeIndexAndFrame(ctx, _params);
    static_cast<void>(line_data->clearAtTime(time_index_and_frame, NotifyObservers::Yes));

    return CommandResult::ok({_params.data_key});
}

}// namespace commands
