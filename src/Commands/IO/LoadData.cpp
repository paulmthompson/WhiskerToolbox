/**
 * @file LoadData.cpp
 * @brief Implementation of the LoadData command
 */

#include "LoadData.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "IO/core/LoaderRegistry.hpp"

#include <nlohmann/json.hpp>

namespace commands {

namespace {

/// Map a human-readable data type string to IODataType
std::optional<IODataType> parseDataType(std::string const & data_type) {
    if (data_type == "PointData") return IODataType::Points;
    if (data_type == "LineData") return IODataType::Line;
    if (data_type == "MaskData") return IODataType::Mask;
    if (data_type == "AnalogTimeSeries") return IODataType::Analog;
    if (data_type == "DigitalEventSeries") return IODataType::DigitalEvent;
    if (data_type == "DigitalIntervalSeries") return IODataType::DigitalInterval;
    if (data_type == "TensorData") return IODataType::Tensor;
    return std::nullopt;
}

/// Convert a LoadedDataVariant to a DataTypeVariant.
/// Returns std::nullopt if the loaded type cannot be stored in DataManager.
std::optional<DataTypeVariant> toDataTypeVariant(LoadedDataVariant && loaded) {
    return std::visit(
            []<typename T>(std::shared_ptr<T> && ptr) -> std::optional<DataTypeVariant> {
                if constexpr (std::is_same_v<T, ImageData>) {
                    // ImageData is not stored in DataManager
                    return std::nullopt;
                } else {
                    return DataTypeVariant{std::move(ptr)};
                }
            },
            std::move(loaded));
}

}// namespace

LoadData::LoadData(LoadDataParams p)
    : _params(std::move(p)) {}

std::string LoadData::commandName() const { return "LoadData"; }

std::string LoadData::toJson() const { return rfl::json::write(_params); }

bool LoadData::isUndoable() const { return true; }

CommandResult LoadData::execute(CommandContext const & ctx) {
    auto const io_type = parseDataType(_params.data_type);
    if (!io_type) {
        return CommandResult::error(
                "Unknown data_type '" + _params.data_type + "'");
    }

    // Build nlohmann::json config from format_options
    nlohmann::json config = nlohmann::json::object();
    if (_params.format_options) {
        auto const json_str = rfl::json::write(*_params.format_options);
        config = nlohmann::json::parse(json_str, nullptr, false);
        if (config.is_discarded()) {
            return CommandResult::error("Failed to parse format_options as JSON");
        }
    }

    // Ensure filepath is present in config for loaders that expect it
    config["filepath"] = _params.filepath;

    // Dispatch through the registry
    auto & registry = LoaderRegistry::getInstance();
    auto result = registry.tryLoad(_params.format, *io_type, _params.filepath, config);

    if (!result.success) {
        return CommandResult::error(result.error_message);
    }

    // Convert LoadedDataVariant to DataTypeVariant
    auto dm_variant = toDataTypeVariant(std::move(result.data));
    if (!dm_variant) {
        return CommandResult::error(
                "Loaded data type cannot be stored in DataManager");
    }

    // Store in DataManager
    ctx.data_manager->setData(_params.data_key, std::move(*dm_variant), TimeKey("time"));

    _data_was_loaded = true;

    return CommandResult::ok({_params.data_key});
}

CommandResult LoadData::undo(CommandContext const & ctx) {
    if (!_data_was_loaded) {
        return CommandResult::error("LoadData was not executed successfully");
    }

    ctx.data_manager->deleteData(_params.data_key);
    _data_was_loaded = false;

    return CommandResult::ok({_params.data_key});
}

}// namespace commands
