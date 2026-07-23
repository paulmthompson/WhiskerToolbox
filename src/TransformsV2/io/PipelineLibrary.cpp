/**
 * @file PipelineLibrary.cpp
 * @brief Implementation of TransformsV2 pipeline library file helpers
 */

#include "PipelineLibrary.hpp"

#include "io/PipelineLoader.hpp"

#include <rfl/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

/**
 * @brief Resolve display name from descriptor metadata or filename stem
 */
[[nodiscard]] std::string entryDisplayName(
        PipelineDescriptor const & descriptor,
        std::string const & id) {
    if (descriptor.metadata.has_value() && descriptor.metadata->name.has_value() &&
        !descriptor.metadata->name->empty()) {
        return *descriptor.metadata->name;
    }
    return id;
}

/**
 * @brief Read entire file contents as a string
 */
[[nodiscard]] rfl::Result<std::string> readFileToString(std::filesystem::path const & filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return rfl::Error("Failed to open file: " + filepath.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return rfl::Result<std::string>(buffer.str());
}

}// namespace

std::filesystem::path defaultUserPipelineDirectory(std::filesystem::path const & config_dir) {
    return config_dir / "pipelines" / "transforms_v2";
}

rfl::Result<std::filesystem::path> ensureUserPipelineDirectory(
        std::filesystem::path const & config_dir) {
    auto const dir = defaultUserPipelineDirectory(config_dir);
    std::error_code error;
    if (!std::filesystem::create_directories(dir, error) && error) {
        return rfl::Error("Failed to create pipeline directory '" + dir.string() +
                          "': " + error.message());
    }
    return rfl::Result<std::filesystem::path>(dir);
}

std::string sanitizePipelineFilename(std::string_view name) {
    std::string result;
    result.reserve(name.size());

    bool last_was_separator = false;
    for (char const ch: name) {
        auto const lower = static_cast<unsigned char>(ch);
        if (std::isalnum(lower) != 0) {
            result.push_back(static_cast<char>(std::tolower(lower)));
            last_was_separator = false;
        } else if (ch == '-' || ch == '_' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!result.empty() && !last_was_separator) {
                result.push_back('_');
                last_was_separator = true;
            }
        }
    }

    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }

    if (result.empty()) {
        return "pipeline";
    }
    return result;
}

std::vector<PipelineLibraryEntry> listPipelineFiles(std::filesystem::path const & directory) {
    std::vector<PipelineLibraryEntry> entries;

    std::error_code error;
    if (!std::filesystem::exists(directory, error) || !std::filesystem::is_directory(directory, error)) {
        return entries;
    }

    for (auto const & dir_entry: std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        if (!dir_entry.is_regular_file()) {
            continue;
        }

        auto const & path = dir_entry.path();
        if (path.extension() != ".json") {
            continue;
        }

        auto const id = path.stem().string();
        auto display_name = id;

        auto const file_contents = readFileToString(path);
        if (file_contents) {
            auto const descriptor_result =
                    rfl::json::read<PipelineDescriptor>(file_contents.value());
            if (descriptor_result) {
                display_name = entryDisplayName(descriptor_result.value(), id);
            }
        }

        entries.push_back(PipelineLibraryEntry{
                .id = id,
                .display_name = std::move(display_name),
                .path = path});
    }

    std::ranges::sort(entries, [](PipelineLibraryEntry const & a, PipelineLibraryEntry const & b) {
        if (a.display_name != b.display_name) {
            return a.display_name < b.display_name;
        }
        return a.id < b.id;
    });

    return entries;
}

rfl::Result<PipelineDescriptor> loadPipelineDescriptorFromFile(
        std::filesystem::path const & filepath) {
    auto const result = loadPipelineDescriptorFromJsonFile(filepath.string());
    if (!result) {
        return rfl::Error(result.error()->what());
    }
    return rfl::Result<PipelineDescriptor>(result.value());
}

rfl::Result<rfl::Nothing> savePipelineDescriptorToFile(
        std::filesystem::path const & filepath,
        PipelineDescriptor const & descriptor) {
    auto const result = savePipelineToFile(filepath.string(), descriptor);
    if (!result) {
        return rfl::Error(result.error()->what());
    }
    return rfl::Result<rfl::Nothing>(rfl::Nothing{});
}

rfl::Result<rfl::Nothing> deletePipelineFile(std::filesystem::path const & filepath) {
    std::error_code error;
    if (!std::filesystem::exists(filepath, error)) {
        return rfl::Error("Pipeline file does not exist: " + filepath.string());
    }

    if (!std::filesystem::remove(filepath, error)) {
        return rfl::Error("Failed to delete pipeline file '" + filepath.string() + "': " +
                          error.message());
    }

    return rfl::Result<rfl::Nothing>(rfl::Nothing{});
}

}// namespace Neuralyzer::Transforms::V2::Examples
