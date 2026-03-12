/**
 * @file MoveByTimeRange.cpp
 * @brief Implementation of the MoveByTimeRange command
 */

#include "MoveByTimeRange.hpp"

#include "Core/CommandContext.hpp"
#include "GetEntityIdsInRange.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

MoveByTimeRange::MoveByTimeRange(MoveByTimeRangeParams p)
    : _params(std::move(p)) {}

std::string MoveByTimeRange::commandName() const { return "MoveByTimeRange"; }

std::string MoveByTimeRange::toJson() const { return rfl::json::write(_params); }

bool MoveByTimeRange::isUndoable() const { return true; }

CommandResult MoveByTimeRange::execute(CommandContext const & ctx) {
    auto src_var = ctx.data_manager->getDataVariant(_params.source_key);
    auto dst_var = ctx.data_manager->getDataVariant(_params.destination_key);
    if (!src_var || !dst_var) {
        return CommandResult::error("Key not found");
    }
    if (src_var->index() != dst_var->index()) {
        return CommandResult::error("Source and destination must be the same data type");
    }

    auto const start = TimeFrameIndex(_params.start_frame);
    auto const end = TimeFrameIndex(_params.end_frame);

    return std::visit(
            [&](auto & src_ptr) -> CommandResult {
                using DataT = std::decay_t<decltype(*src_ptr)>;
                auto dst_ptr = ctx.data_manager->getData<DataT>(_params.destination_key);

                if constexpr (requires(DataT & s, DataT & d,
                                       std::unordered_set<EntityId> const & ids) {
                                  s.moveByEntityIds(d, ids, NotifyObservers::Yes);
                              }) {
                    _moved_ids = getEntityIdsInRange(*src_ptr, start, end);
                    if (_moved_ids.empty()) {
                        return CommandResult::ok({_params.source_key, _params.destination_key});
                    }
                    src_ptr->moveByEntityIds(*dst_ptr, _moved_ids, NotifyObservers::Yes);
                    return CommandResult::ok({_params.source_key, _params.destination_key});
                } else {
                    return CommandResult::error("Data type does not support move operations");
                }
            },
            *src_var);
}

CommandResult MoveByTimeRange::undo(CommandContext const & ctx) {
    if (_moved_ids.empty()) {
        return CommandResult::ok({_params.source_key, _params.destination_key});
    }

    auto dst_var = ctx.data_manager->getDataVariant(_params.destination_key);
    if (!dst_var) {
        return CommandResult::error("Destination key not found during undo");
    }

    return std::visit(
            [&](auto & dst_ptr) -> CommandResult {
                using DataT = std::decay_t<decltype(*dst_ptr)>;
                auto src_ptr = ctx.data_manager->getData<DataT>(_params.source_key);
                if (!src_ptr) {
                    return CommandResult::error("Source key not found during undo");
                }

                if constexpr (requires(DataT & s, DataT & d,
                                       std::unordered_set<EntityId> const & ids) {
                                  s.moveByEntityIds(d, ids, NotifyObservers::Yes);
                              }) {
                    dst_ptr->moveByEntityIds(*src_ptr, _moved_ids, NotifyObservers::Yes);
                    _moved_ids.clear();
                    return CommandResult::ok({_params.source_key, _params.destination_key});
                } else {
                    return CommandResult::error("Data type does not support move operations");
                }
            },
            *dst_var);
}

}// namespace commands
