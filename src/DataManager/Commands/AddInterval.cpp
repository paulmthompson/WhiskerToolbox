/**
 * @file AddInterval.cpp
 * @brief Implementation of the AddInterval command
 */

#include "AddInterval.hpp"

#include "CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

AddInterval::AddInterval(AddIntervalParams p)
    : _params(std::move(p)) {}

std::string AddInterval::commandName() const { return "AddInterval"; }

std::string AddInterval::toJson() const { return rfl::json::write(_params); }

CommandResult AddInterval::execute(CommandContext const & ctx) {
    auto series = ctx.data_manager->getData<DigitalIntervalSeries>(_params.interval_key);
    if (!series) {
        if (!_params.create_if_missing) {
            return CommandResult::error(
                    "Interval series '" + _params.interval_key + "' not found");
        }
        ctx.data_manager->setData<DigitalIntervalSeries>(
                _params.interval_key, TimeKey("time"));
        series = ctx.data_manager->getData<DigitalIntervalSeries>(_params.interval_key);
    }

    series->addEvent(
            TimeFrameIndex(_params.start_frame),
            TimeFrameIndex(_params.end_frame));

    return CommandResult::ok({_params.interval_key});
}

}// namespace commands
