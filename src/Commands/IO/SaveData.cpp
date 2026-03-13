/**
 * @file SaveData.cpp
 * @brief Implementation of the SaveData command
 */

#include "SaveData.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "IO/core/LoaderRegistry.hpp"

#include <nlohmann/json.hpp>

namespace commands {

SaveData::SaveData(SaveDataParams p)
    : _params(std::move(p)) {}

std::string SaveData::commandName() const { return "SaveData"; }

std::string SaveData::toJson() const { return rfl::json::write(_params); }

CommandResult SaveData::execute(CommandContext const & ctx) {
    auto const dm_type = ctx.data_manager->getType(_params.data_key);
    if (dm_type == DM_DataType::Unknown) {
        return CommandResult::error(
                "Data key '" + _params.data_key + "' not found in DataManager");
    }

    auto const io_type = toIODataType(dm_type);

    // Get the raw data pointer from the variant
    auto variant = ctx.data_manager->getDataVariant(_params.data_key);
    if (!variant) {
        return CommandResult::error(
                "Data key '" + _params.data_key + "' could not be retrieved");
    }

    void const * data_ptr = std::visit(
            [](auto const & ptr) -> void const * {
                return static_cast<void const *>(ptr.get());
            },
            *variant);

    if (!data_ptr) {
        return CommandResult::error(
                "Data pointer for key '" + _params.data_key + "' is null");
    }

    // Build nlohmann::json config from format_options
    nlohmann::json config;
    if (_params.format_options) {
        auto const json_str = rfl::json::write(*_params.format_options);
        config = nlohmann::json::parse(json_str, nullptr, false);
        if (config.is_discarded()) {
            return CommandResult::error("Failed to parse format_options as JSON");
        }
    }

    // Dispatch through the registry
    auto & registry = LoaderRegistry::getInstance();
    auto result = registry.trySave(_params.format, io_type, _params.path, config, data_ptr);

    if (!result.success) {
        return CommandResult::error(result.error_message);
    }

    return CommandResult::ok();
}

}// namespace commands
