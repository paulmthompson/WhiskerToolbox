/**
 * @file SavePathResolver.cpp
 * @brief Implementation of provenance-aware save path resolution helpers.
 */

#include "SavePathResolver.hpp"

#include "DataManager/DataManager.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>

namespace commands {

namespace {

/**
 * @brief Check whether a file-origin record describes a multi-file load.
 * @param origin File-origin metadata from the lineage registry.
 * @return true when multi_file is set in the source config or the path is a directory.
 */
[[nodiscard]] bool isMultiFileOrigin(Neuralyzer::Entity::Lineage::FileOrigin const & origin) {
    if (origin.m_source_config_json.has_value()) {
        auto const config = nlohmann::json::parse(
                *origin.m_source_config_json, nullptr, false);
        if (!config.is_discarded() && config.value("multi_file", false)) {
            return true;
        }
    }

    std::error_code ec;
    return std::filesystem::is_directory(origin.m_path, ec);
}

/**
 * @brief Resolve a fallback parent directory against the DataManager output path.
 * @param output_path DataManager output directory.
 * @param fallback_parent_dir UI-provided directory name or path.
 * @return Absolute or output-relative parent directory path.
 */
[[nodiscard]] std::string resolveFallbackParentDir(
        std::string const & output_path,
        std::string const & fallback_parent_dir) {
    std::filesystem::path const parent_path(fallback_parent_dir);
    if (fallback_parent_dir == "." || !parent_path.is_absolute()) {
        if (fallback_parent_dir == ".") {
            return output_path;
        }
        return (std::filesystem::path(output_path) / fallback_parent_dir).string();
    }
    return fallback_parent_dir;
}

}// namespace

SavePathResolution resolveDefaultSavePath(
        DataManager const & data_manager,
        std::string const & data_key,
        std::string const & requested_format,
        std::string const & fallback_filename) {
    if (auto const * registry = data_manager.getLineageRegistry()) {
        if (auto origin = registry->getFileOrigin(data_key);
            origin && origin->m_format == requested_format && !origin->m_path.empty()) {
            auto const origin_path = std::filesystem::path(origin->m_path);
            return SavePathResolution{
                    .m_path = origin_path.string(),
                    .m_parent_dir = origin_path.parent_path().string(),
                    .m_filename = origin_path.filename().string(),
                    .m_using_file_origin = true};
        }
    }

    auto const fallback_path = std::filesystem::path(data_manager.getOutputPath()) / fallback_filename;
    return SavePathResolution{
            .m_path = fallback_path.string(),
            .m_parent_dir = fallback_path.parent_path().string(),
            .m_filename = fallback_path.filename().string(),
            .m_using_file_origin = false};
}

MultiFileSavePathResolution resolveDefaultMultiFileSaveDir(
        DataManager const & data_manager,
        std::string const & data_key,
        std::string const & requested_format,
        std::string const & fallback_parent_dir) {
    if (auto const * registry = data_manager.getLineageRegistry()) {
        if (auto origin = registry->getFileOrigin(data_key);
            origin && origin->m_format == requested_format && !origin->m_path.empty() &&
            isMultiFileOrigin(*origin)) {
            return MultiFileSavePathResolution{
                    .m_parent_dir = origin->m_path,
                    .m_using_file_origin = true};
        }
    }

    return MultiFileSavePathResolution{
            .m_parent_dir = resolveFallbackParentDir(
                    data_manager.getOutputPath(), fallback_parent_dir),
            .m_using_file_origin = false};
}

}// namespace commands
