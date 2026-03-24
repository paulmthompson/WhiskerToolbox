/**
 * @file register_core_commands.cpp
 * @brief Registers all built-in commands with the CommandRegistry singleton
 */

#include "Core/register_core_commands.hpp"

#include "CommandUIHints.hpp"
#include "Core/CommandRegistry.hpp"

#include "AddInterval.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "IO/LoadData.hpp"
#include "IO/SaveData.hpp"
#include "MoveByTimeRange.hpp"
#include "SynthesizeData.hpp"

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

    registerTypedCommand<SynthesizeData, SynthesizeDataParams>(
            reg,
            "SynthesizeData",
            {.name = "SynthesizeData",
             .description = "Generate synthetic data using a registered generator and store it in DataManager",
             .category = "data_generation",
             .supports_undo = false,
             .supported_data_types = {"AnalogTimeSeries"}});
}

}// namespace commands
