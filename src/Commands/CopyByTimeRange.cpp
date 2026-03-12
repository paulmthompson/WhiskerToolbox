/**
 * @file CopyByTimeRange.cpp
 * @brief Implementation of the CopyByTimeRange command
 */

#include "CopyByTimeRange.hpp"

#include "Core/CommandContext.hpp"
#include "GetEntityIdsInRange.hpp"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

namespace commands {

CopyByTimeRange::CopyByTimeRange(CopyByTimeRangeParams p)
    : _params(std::move(p)) {}

std::string CopyByTimeRange::commandName() const { return "CopyByTimeRange"; }

std::string CopyByTimeRange::toJson() const { return rfl::json::write(_params); }

CommandResult CopyByTimeRange::execute(CommandContext const & ctx) {
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
                                  s.copyByEntityIds(d, ids, NotifyObservers::Yes);
                              }) {
                    auto ids = getEntityIdsInRange(*src_ptr, start, end);
                    if (ids.empty()) {
                        return CommandResult::ok({_params.source_key, _params.destination_key});
                    }
                    src_ptr->copyByEntityIds(*dst_ptr, ids, NotifyObservers::Yes);
                    return CommandResult::ok({_params.source_key, _params.destination_key});
                } else {
                    return CommandResult::error("Data type does not support copy operations");
                }
            },
            *src_var);
}

}// namespace commands
