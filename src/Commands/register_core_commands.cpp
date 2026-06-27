/**
 * @file register_core_commands.cpp
 * @brief Registers all built-in commands with the CommandRegistry singleton
 */

#include "register_core_commands.hpp"

#include "CommandUIHints.hpp"
#include "Core/CommandRegistry.hpp"
#include "DataObjects/DigitalTimeSeries/AddInterval.hpp"
#include "DataObjects/DigitalTimeSeries/FlipEventAtTime.hpp"
#include "DataObjects/DigitalTimeSeries/SetEventAtTime.hpp"
#include "DataObjects/Lines/ClearLineDataAtTime.hpp"
#include "IO/LoadData.hpp"
#include "IO/SaveData.hpp"

#include "AdvanceFrame.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "MoveByTimeRange.hpp"
#include "UpsampleTimeFrame.hpp"

namespace commands {

void register_core_commands() {
    auto & reg = CommandRegistry::instance();

    // Guard against double-registration
    if (reg.isKnown("MoveByTimeRange")) {
        return;
    }

    registerTypedCommand<MoveByTimeRange, MoveByTimeRangeParams>(
            reg,
            "MoveByTimeRange",
            {.name = "MoveByTimeRange",
             .description = "Move entities in a time range from one data object to another of the same type",
             .category = "data_mutation",
             .supports_undo = true,
             .supported_data_types = {"LineData", "PointData", "MaskData", "DigitalEventSeries"}});

    registerTypedCommand<CopyByTimeRange, CopyByTimeRangeParams>(
            reg,
            "CopyByTimeRange",
            {.name = "CopyByTimeRange",
             .description = "Copy entities in a time range from one data object to another",
             .category = "data_mutation",
             .supports_undo = false,
             .supported_data_types = {"LineData", "PointData", "MaskData", "DigitalEventSeries"}});

    registerTypedCommand<AddInterval, AddIntervalParams>(
            reg,
            "AddInterval",
            {.name = "AddInterval",
             .description = "Append an interval to a DigitalIntervalSeries, creating it if needed",
             .category = "data_mutation",
             .supports_undo = false,
             .supported_data_types = {"DigitalIntervalSeries"}});

    registerTypedCommand<AdvanceFrame, AdvanceFrameParams>(
            reg,
            "AdvanceFrame",
            {.name = "AdvanceFrame",
             .description = "Shift the current visualization frame by a signed delta",
             .category = "navigation",
             .supports_undo = false,
             .supported_data_types = {}});

    registerTypedCommand<SetEventAtTime, SetEventAtTimeParams>(
            reg,
            "SetEventAtTime",
            {.name = "SetEventAtTime",
             .description = "Set or clear DigitalIntervalSeries membership at a single time index",
             .category = "data_mutation",
             .supports_undo = false,
             .supported_data_types = {"DigitalIntervalSeries"}});

    registerTypedCommand<ClearLineDataAtTime, ClearLineDataAtTimeParams>(
            reg,
            "ClearLineDataAtTime",
            {.name = "ClearLineDataAtTime",
             .description = "Remove all LineData entries at a single time index",
             .category = "data_mutation",
             .supports_undo = false,
             .supported_data_types = {"LineData"}});

    registerTypedCommand<FlipEventAtTime, FlipEventAtTimeParams>(
            reg,
            "FlipEventAtTime",
            {.name = "FlipEventAtTime",
             .description = "Toggle DigitalIntervalSeries membership at a single time index",
             .category = "data_mutation",
             .supports_undo = false,
             .supported_data_types = {"DigitalIntervalSeries"}});

    registerTypedCommand<ForEachKey, ForEachKeyParams>(
            reg,
            "ForEachKey",
            {.name = "ForEachKey",
             .description = "Iterate a list of values, binding each to a template variable and executing sub-commands",
             .category = "meta",
             .supports_undo = false,
             .supported_data_types = {}});

    registerTypedCommand<SaveData, SaveDataParams>(
            reg,
            "SaveData",
            {.name = "SaveData",
             .description = "Save a data object from DataManager to disk via the LoaderRegistry",
             .category = "persistence",
             .supports_undo = false,
             .supported_data_types = {"PointData", "LineData", "MaskData", "AnalogTimeSeries", "DigitalEventSeries", "DigitalIntervalSeries"}});

    registerTypedCommand<LoadData, LoadDataParams>(
            reg,
            "LoadData",
            {.name = "LoadData",
             .description = "Load data from disk into DataManager via the LoaderRegistry",
             .category = "persistence",
             .supports_undo = true,
             .supported_data_types = {"PointData", "LineData", "MaskData", "AnalogTimeSeries", "DigitalEventSeries", "DigitalIntervalSeries", "TensorData"}});

    registerTypedCommand<UpsampleTimeFrame, UpsampleTimeFrameParams>(
            reg,
            "UpsampleTimeFrame",
            {.name = "UpsampleTimeFrame",
             .description = "Create an upsampled TimeFrame by linearly interpolating source clock values",
             .category = "timeframe",
             .supports_undo = false,
             .supported_data_types = {}});
}

}// namespace commands
