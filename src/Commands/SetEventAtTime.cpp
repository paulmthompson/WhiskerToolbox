/**
 * @file SetEventAtTime.cpp
 * @brief Implementation of SetEventAtTime command
 */

#include "SetEventAtTime.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

SetEventAtTime::SetEventAtTime(SetEventAtTimeParams p)
    : _params(std::move(p)) {}

std::string SetEventAtTime::commandName() const {
    return "SetEventAtTime";
}

std::string SetEventAtTime::toJson() const {
    return rfl::json::write(_params);
}

CommandResult SetEventAtTime::execute(CommandContext const & ctx) {
    if (!ctx.data_manager) {
        return CommandResult::error("SetEventAtTime requires CommandContext::data_manager");
    }

    auto series = ctx.data_manager->getData<DigitalIntervalSeries>(_params.data_key);
    if (!series) {
        return CommandResult::error(
                "SetEventAtTime: no DigitalIntervalSeries at key '" + _params.data_key + "'");
    }

    series->setEventAtTime(TimeFrameIndex(_params.time), _params.event);
    return CommandResult::ok({_params.data_key});
}

}// namespace commands
