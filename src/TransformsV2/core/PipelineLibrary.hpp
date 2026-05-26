#ifndef WHISKERTOOLBOX_V2_PIPELINE_LIBRARY_HPP
#define WHISKERTOOLBOX_V2_PIPELINE_LIBRARY_HPP

/**
 * @file PipelineLibrary.hpp
 * @brief Qt-free helpers for listing and persisting TransformsV2 PipelineDescriptor files
 *
 * User-saved pipelines live under {@code config_dir/pipelines/transforms_v2/} by convention.
 * This matches StateManager's application config root (see pipeline_library_roadmap.md).
 */

#include "core/PipelineLoader.hpp"

#include <rfl/Result.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Catalog entry for a saved pipeline JSON file
 */
struct PipelineLibraryEntry {
    /// Filename stem (no directory, no .json extension)
    std::string id;
    /// Display name from metadata.name when present, otherwise id
    std::string display_name;
    /// Absolute or relative path to the .json file
    std::filesystem::path path;
};

/**
 * @brief Default subdirectory for user TransformsV2 pipelines under a config root
 * @param config_dir Application config directory (e.g. StateManager::configDir())
 * @return {@code config_dir / "pipelines" / "transforms_v2"}
 */
[[nodiscard]] std::filesystem::path defaultUserPipelineDirectory(
        std::filesystem::path const & config_dir);

/**
 * @brief Create the user pipeline directory if it does not exist
 * @param config_dir Application config directory
 * @return Success, or error message if creation failed
 */
[[nodiscard]] rfl::Result<std::filesystem::path> ensureUserPipelineDirectory(
        std::filesystem::path const & config_dir);

/**
 * @brief Convert a human-readable name into a safe filename stem
 * @param name Proposed pipeline name (may contain spaces or punctuation)
 * @return Non-empty filesystem-safe stem; falls back to {@code "pipeline"} if empty after sanitization
 */
[[nodiscard]] std::string sanitizePipelineFilename(std::string_view name);

/**
 * @brief List {@code .json} pipeline files in a directory
 * @param directory Directory to scan (need not exist; returns empty vector)
 * @return Entries sorted by display_name then id
 */
[[nodiscard]] std::vector<PipelineLibraryEntry> listPipelineFiles(
        std::filesystem::path const & directory);

/**
 * @brief Read and parse a PipelineDescriptor from disk
 * @param filepath Path to a JSON file
 * @return Parsed descriptor or error
 */
[[nodiscard]] rfl::Result<PipelineDescriptor> loadPipelineDescriptorFromFile(
        std::filesystem::path const & filepath);

/**
 * @brief Write a PipelineDescriptor to disk as JSON
 * @param filepath Destination path (parent directories are not created)
 * @param descriptor Descriptor to serialize
 * @return Success or error
 */
[[nodiscard]] rfl::Result<rfl::Nothing> savePipelineDescriptorToFile(
        std::filesystem::path const & filepath,
        PipelineDescriptor const & descriptor);

/**
 * @brief Remove a pipeline file from the library
 * @param filepath Path to delete
 * @return Success or error
 */
[[nodiscard]] rfl::Result<rfl::Nothing> deletePipelineFile(std::filesystem::path const & filepath);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_PIPELINE_LIBRARY_HPP
