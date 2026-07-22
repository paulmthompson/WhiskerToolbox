/**
 * @file PathFieldKind.hpp
 * @brief Path-picker mode for string parameter fields in schema-driven UI
 */
#ifndef NEURALYZER_PATH_FIELD_KIND_HPP
#define NEURALYZER_PATH_FIELD_KIND_HPP

/**
 * @brief How AutoParamWidget renders a string field as a filesystem path
 */
enum class PathFieldKind {
    None,           ///< Plain text input
    FilePath,       ///< File open dialog + line edit
    DirectoryPath   ///< Directory picker + line edit
};

#endif// NEURALYZER_PATH_FIELD_KIND_HPP
