#ifndef PATH_RESOLVER_HPP
#define PATH_RESOLVER_HPP

/**
 * @file PathResolver.hpp
 * @brief Utility for converting between absolute and workspace-relative paths
 *
 * When saving a workspace, data file paths are converted to relative paths
 * (relative to the workspace file location) so workspaces are portable.
 * When loading, relative paths are resolved back to absolute.
 *
 * @see WorkspaceManager for usage
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 2b
 */

#include <filesystem>
#include <string>

namespace StateManagement {

struct PathResolver {
    /**
     * @brief Convert an absolute path to a relative path based on workspace directory
     * @param absolute_path The absolute path to convert
     * @param workspace_dir The directory containing the workspace file
     * @return Relative path string, or the original path if conversion fails
     */
    [[nodiscard]] static std::string toRelative(std::string const & absolute_path,
                                                 std::string const & workspace_dir)
    {
        try {
            if (absolute_path.empty() || workspace_dir.empty()) {
                return absolute_path;
            }

            std::filesystem::path abs(absolute_path);
            std::filesystem::path base(workspace_dir);

            // Only convert if both paths are absolute
            if (!abs.is_absolute() || !base.is_absolute()) {
                return absolute_path;
            }

            auto rel = std::filesystem::relative(abs, base);
            return rel.string();
        } catch (...) {
            return absolute_path;
        }
    }

    /**
     * @brief Convert a relative path to an absolute path based on workspace directory
     * @param relative_path The relative path to resolve
     * @param workspace_dir The directory containing the workspace file
     * @return Absolute path string
     */
    [[nodiscard]] static std::string toAbsolute(std::string const & relative_path,
                                                 std::string const & workspace_dir)
    {
        try {
            if (relative_path.empty() || workspace_dir.empty()) {
                return relative_path;
            }

            std::filesystem::path rel(relative_path);

            // Already absolute — return as-is
            if (rel.is_absolute()) {
                return relative_path;
            }

            auto abs = std::filesystem::path(workspace_dir) / rel;
            return std::filesystem::weakly_canonical(abs).string();
        } catch (...) {
            return relative_path;
        }
    }

    /**
     * @brief Check whether a file or directory exists
     * @param path Path to check (absolute or relative)
     * @return true if it exists on disk
     */
    [[nodiscard]] static bool fileExists(std::string const & path)
    {
        try {
            return std::filesystem::exists(path);
        } catch (...) {
            return false;
        }
    }
};

} // namespace StateManagement

#endif // PATH_RESOLVER_HPP
