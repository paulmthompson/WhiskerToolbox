/**
 * @file SavePathResolver.hpp
 * @brief Helpers for resolving provenance-aware default save paths.
 */

#ifndef SAVE_PATH_RESOLVER_HPP
#define SAVE_PATH_RESOLVER_HPP

#include <string>

class DataManager;

namespace commands {

/**
 * @brief Resolved save target for a data key.
 */
struct SavePathResolution {
    std::string m_path;              ///< Full target file path.
    std::string m_parent_dir;        ///< Directory containing the target file.
    std::string m_filename;          ///< Target file name.
    bool m_using_file_origin = false;///< True when the source file origin was used.
};

/**
 * @brief Resolve the default single-file save path for a data key.
 *
 * The resolver prefers a matching file-origin record, then falls back to the
 * DataManager output directory plus fallback_filename.
 *
 * @param data_manager DataManager that owns data and lineage state.
 * @param data_key Key of the data object being saved.
 * @param requested_format Saver format requested by the UI/command.
 * @param fallback_filename Filename to use when no matching file origin exists.
 * @return Resolved path components and whether provenance was used.
 * @post data_manager is not modified.
 */
[[nodiscard]] SavePathResolution resolveDefaultSavePath(
        DataManager const & data_manager,
        std::string const & data_key,
        std::string const & requested_format,
        std::string const & fallback_filename);

}// namespace commands

#endif// SAVE_PATH_RESOLVER_HPP
