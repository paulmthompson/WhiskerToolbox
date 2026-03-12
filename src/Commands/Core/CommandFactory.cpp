/**
 * @file CommandFactory.cpp
 * @brief Implementation of the command factory and introspection query functions
 */

#include "CommandFactory.hpp"

#include "AddInterval.hpp"
#include "CommandUIHints.hpp"
#include "CopyByTimeRange.hpp"
#include "ForEachKey.hpp"
#include "ICommand.hpp"
#include "IO/LoadData.hpp"
#include "IO/SaveData.hpp"
#include "MoveByTimeRange.hpp"
#include "SynthesizeData.hpp"
#include "VariableSubstitution.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <rfl/json.hpp>

using WhiskerToolbox::Transforms::V2::extractParameterSchema;

namespace commands {

bool isKnownCommandName(std::string const & name) {
    return name == "MoveByTimeRange" || name == "CopyByTimeRange" ||
           name == "AddInterval" || name == "ForEachKey" ||
           name == "SaveData" || name == "LoadData" ||
           name == "SynthesizeData";
}

std::unique_ptr<ICommand> createCommand(
        std::string const & name,
        rfl::Generic const & params) {

    return createCommandFromJson(name, rfl::json::write(params));
}

std::unique_ptr<ICommand> createCommandFromJson(
        std::string const & name,
        std::string const & params_json) {

    auto const json = normalizeJsonNumbers(params_json);

    if (name == "MoveByTimeRange") {
        auto p = rfl::json::read<MoveByTimeRangeParams>(json);
        if (!p) return nullptr;
        return std::make_unique<MoveByTimeRange>(std::move(*p));
    }

    if (name == "CopyByTimeRange") {
        auto p = rfl::json::read<CopyByTimeRangeParams>(json);
        if (!p) return nullptr;
        return std::make_unique<CopyByTimeRange>(std::move(*p));
    }

    if (name == "AddInterval") {
        auto p = rfl::json::read<AddIntervalParams>(json);
        if (!p) return nullptr;
        return std::make_unique<AddInterval>(std::move(*p));
    }

    if (name == "ForEachKey") {
        auto p = rfl::json::read<ForEachKeyParams>(json);
        if (!p) return nullptr;
        return std::make_unique<ForEachKey>(std::move(*p));
    }

    if (name == "SaveData") {
        auto p = rfl::json::read<SaveDataParams>(json);
        if (!p) return nullptr;
        return std::make_unique<SaveData>(std::move(*p));
    }

    if (name == "LoadData") {
        auto p = rfl::json::read<LoadDataParams>(json);
        if (!p) return nullptr;
        return std::make_unique<LoadData>(std::move(*p));
    }

    if (name == "SynthesizeData") {
        auto p = rfl::json::read<SynthesizeDataParams>(json);
        if (!p) return nullptr;
        return std::make_unique<SynthesizeData>(std::move(*p));
    }

    return nullptr;
}

std::vector<CommandInfo> getAvailableCommands() {
    return {
            CommandInfo{
                    .name = "MoveByTimeRange",
                    .description = "Move entities in a time range from one data object to another of the same type",
                    .category = "data_mutation",
                    .supports_undo = true,
                    .supported_data_types = {"LineData", "PointData", "MaskData", "DigitalEventSeries"},
                    .parameter_schema = extractParameterSchema<MoveByTimeRangeParams>(),
            },
            CommandInfo{
                    .name = "CopyByTimeRange",
                    .description = "Copy entities in a time range from one data object to another",
                    .category = "data_mutation",
                    .supports_undo = false,
                    .supported_data_types = {"LineData", "PointData", "MaskData", "DigitalEventSeries"},
                    .parameter_schema = extractParameterSchema<CopyByTimeRangeParams>(),
            },
            CommandInfo{
                    .name = "AddInterval",
                    .description = "Append an interval to a DigitalIntervalSeries, creating it if needed",
                    .category = "data_mutation",
                    .supports_undo = false,
                    .supported_data_types = {"DigitalIntervalSeries"},
                    .parameter_schema = extractParameterSchema<AddIntervalParams>(),
            },
            CommandInfo{
                    .name = "ForEachKey",
                    .description = "Iterate a list of values, binding each to a template variable and executing sub-commands",
                    .category = "meta",
                    .supports_undo = false,
                    .supported_data_types = {},
                    .parameter_schema = extractParameterSchema<ForEachKeyParams>(),
            },
            CommandInfo{
                    .name = "SaveData",
                    .description = "Save a data object from DataManager to disk via the LoaderRegistry",
                    .category = "persistence",
                    .supports_undo = false,
                    .supported_data_types = {"PointData", "LineData", "MaskData", "AnalogTimeSeries", "DigitalEventSeries", "DigitalIntervalSeries"},
                    .parameter_schema = extractParameterSchema<SaveDataParams>(),
            },
            CommandInfo{
                    .name = "LoadData",
                    .description = "Load data from disk into DataManager via the LoaderRegistry",
                    .category = "persistence",
                    .supports_undo = true,
                    .supported_data_types = {"PointData", "LineData", "MaskData", "AnalogTimeSeries", "DigitalEventSeries", "DigitalIntervalSeries", "TensorData"},
                    .parameter_schema = extractParameterSchema<LoadDataParams>(),
            },
            CommandInfo{
                    .name = "SynthesizeData",
                    .description = "Generate synthetic data using a registered generator and store it in DataManager",
                    .category = "data_generation",
                    .supports_undo = false,
                    .supported_data_types = {"AnalogTimeSeries"},
                    .parameter_schema = extractParameterSchema<SynthesizeDataParams>(),
            },
    };
}

std::optional<CommandInfo> getCommandInfo(std::string const & name) {
    auto const all = getAvailableCommands();
    for (auto & info: all) {
        if (info.name == name) {
            return info;
        }
    }
    return std::nullopt;
}

}// namespace commands
