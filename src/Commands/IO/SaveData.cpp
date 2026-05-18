/**
 * @file SaveData.cpp
 * @brief Implementation of the SaveData command
 */

#include "SaveData.hpp"

#include "Core/CommandContext.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"
#include "IO/core/LoaderRegistry.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>

namespace commands {

namespace {

/**
 * @brief Rename an existing single-file target to a backup path.
 * @param target_path Path that will be overwritten by the saver.
 * @param suffix Suffix appended to target_path for the backup.
 * @return Empty string on success, otherwise an explanatory error.
 * @post If target_path existed as a regular file, it is moved to target_path + suffix.
 */
[[nodiscard]] std::string backupExistingFile(
        std::filesystem::path const & target_path,
        std::string const & suffix) {
    if (suffix.empty() || !std::filesystem::exists(target_path) ||
        !std::filesystem::is_regular_file(target_path)) {
        return {};
    }

    std::error_code ec;
    auto const backup_path = std::filesystem::path(target_path.string() + suffix);
    std::filesystem::remove(backup_path, ec);
    ec.clear();
    std::filesystem::rename(target_path, backup_path, ec);
    if (ec) {
        return "Failed to back up existing file '" + target_path.string() +
               "' to '" + backup_path.string() + "': " + ec.message();
    }

    return {};
}

}// namespace

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

    auto const io_type = dm_type;

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
    if (!registry.isFormatSupported(_params.format, io_type)) {
        return CommandResult::error(
                "No registered saver supports format '" + _params.format +
                "' for data type " + std::to_string(static_cast<int>(io_type)));
    }

    if (_params.backup_existing.value_or(false)) {
        if (auto const backup_error = backupExistingFile(
                    _params.path, _params.backup_suffix.value_or(".bak"));
            !backup_error.empty()) {
            return CommandResult::error(backup_error);
        }
    }

    auto result = registry.trySave(_params.format, io_type, _params.path, config, data_ptr);

    if (!result.success) {
        return CommandResult::error(result.error_message);
    }

    return CommandResult::ok();
}

}// namespace commands
