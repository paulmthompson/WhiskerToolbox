#ifndef APP_PREFERENCES_DATA_HPP
#define APP_PREFERENCES_DATA_HPP

/**
 * @file AppPreferencesData.hpp
 * @brief Serializable data structure for application-level preferences
 *
 * Application preferences are global to the program, machine-specific,
 * and automatically persisted. They survive across sessions and are
 * never tied to a specific dataset or workspace.
 *
 * Examples: Python environment search paths, default directories, UI theme.
 *
 * @see AppPreferences for the Qt wrapper class
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 1 for design rationale
 */

#include <rfl.hpp>

#include "KeymapSystem/Keymap.hpp"

#include <string>
#include <vector>

namespace StateManagement {

/**
 * @brief Serializable application preferences data
 *
 * All members are designed for reflect-cpp serialization.
 * No Qt types — this struct can be tested without Qt.
 */
struct AppPreferencesData {
    std::string version = "1.0";///< Schema version for migration

    // === Python Environment ===
    std::vector<std::string> python_env_search_paths;///< Directories to search for Python environments
    std::string preferred_python_env;                ///< Last-used or preferred Python environment path

    // === File Dialogs ===
    std::string default_import_directory;///< Default directory for import dialogs
    std::string default_export_directory;///< Default directory for export dialogs

    // === Data Loading ===
    std::string default_time_frame_key;///< Default TimeFrame key for new data loads

    // === Keyboard Shortcuts ===
    std::vector<KeymapSystem::KeymapOverrideEntry> keybinding_overrides;///< User keybinding customizations
};

}// namespace StateManagement

#endif// APP_PREFERENCES_DATA_HPP
