/**
 * @file FlipEventAtTime.cpp
 * @brief Implementation of FlipEventAtTime command
 */

#include "FlipEventAtTime.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

FlipEventAtTime::FlipEventAtTime(FlipEventAtTimeParams p)
    : _params(std::move(p)) {}

std::string FlipEventAtTime::commandName() const {
    return "FlipEventAtTime";
}

std::string FlipEventAtTime::toJson() const {
    return rfl::json::write(_params);
}

CommandResult FlipEventAtTime::execute(CommandContext const & ctx) {
    if (!ctx.data_manager) {
        return CommandResult::error("FlipEventAtTime requires CommandContext::data_manager");
    }

    auto series = ctx.data_manager->getData<DigitalIntervalSeries>(_params.data_key);
    if (!series) {
        return CommandResult::error(
                "FlipEventAtTime: no DigitalIntervalSeries at key '" + _params.data_key + "'");
    }

    TimeFrame unused_source_tf{};
    std::shared_ptr<TimeFrame> const series_tf = series->getTimeFrame();
    TimeFrame const & source_tf = series_tf ? *series_tf : unused_source_tf;

    bool const had_interval =
            series->hasIntervalAtTime(TimeFrameIndex(_params.time), source_tf);
    series->setEventAtTime(TimeFrameIndex(_params.time), !had_interval);
    return CommandResult::ok({_params.data_key});
}

}// namespace commands
