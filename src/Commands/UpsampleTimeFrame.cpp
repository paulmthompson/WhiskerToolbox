/**
 * @file UpsampleTimeFrame.cpp
 * @brief Implementation of UpsampleTimeFrame command
 */

#include "UpsampleTimeFrame.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/utils/DerivedTimeFrame.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"

namespace commands {

UpsampleTimeFrame::UpsampleTimeFrame(UpsampleTimeFrameParams p)
    : _params(std::move(p)) {}

std::string UpsampleTimeFrame::commandName() const {
    return "UpsampleTimeFrame";
}

std::string UpsampleTimeFrame::toJson() const {
    return rfl::json::write(_params);
}

CommandResult UpsampleTimeFrame::execute(CommandContext const & ctx) {
    if (_params.source_time_key.empty()) {
        return CommandResult::error("UpsampleTimeFrame requires a non-empty source_time_key");
    }
    if (_params.output_time_key.empty()) {
        return CommandResult::error("UpsampleTimeFrame requires a non-empty output_time_key");
    }

    auto const factor = _params.upsampling_factor;
    if (factor <= 0) {
        return CommandResult::error(
                "UpsampleTimeFrame requires upsampling_factor > 0, got " +
                std::to_string(factor));
    }

    auto const source_tf = ctx.data_manager->getTime(TimeKey(_params.source_time_key));
    if (!source_tf) {
        return CommandResult::error(
                "Source TimeFrame '" + _params.source_time_key + "' not found");
    }

    DerivedTimeFrameFromUpsamplingOptions opts;
    opts.source_timeframe = source_tf;
    opts.upsampling_factor = static_cast<int>(factor);

    auto const result = createUpsampledTimeFrame(opts);
    if (!result) {
        return CommandResult::error(
                "Failed to create upsampled TimeFrame from '" + _params.source_time_key + "'");
    }

    if (!ctx.data_manager->setTime(
                TimeKey(_params.output_time_key), result, _params.overwrite)) {
        return CommandResult::error(
                "Failed to register TimeFrame '" + _params.output_time_key +
                "' (key may already exist; set overwrite=true to replace)");
    }

    return CommandResult::ok({_params.output_time_key});
}

}// namespace commands
