/**
 * @file SavePathResolver.cpp
 * @brief Implementation of provenance-aware save path resolution helpers.
 */

#include "SavePathResolver.hpp"

#include "DataManager/DataManager.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"

#include <filesystem>

namespace commands {

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

}// namespace commands
