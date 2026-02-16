#ifndef SESSION_DATA_HPP
#define SESSION_DATA_HPP

/**
 * @file SessionData.hpp
 * @brief Serializable data structure for session memory
 *
 * Session memory tracks transient-but-useful state that persists across
 * application launches: last used file paths per dialog, recent workspace
 * list, window geometry. This is machine-specific and auto-saved silently.
 *
 * @see SessionStore for the Qt wrapper class
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 1 for design rationale
 */

#include <rfl.hpp>

#include <map>
#include <string>
#include <vector>

namespace StateManagement {

/**
 * @brief Serializable session memory data
 *
 * All members are designed for reflect-cpp serialization.
 * No Qt types — this struct can be tested without Qt.
 */
struct SessionData {
    std::string version = "1.0";///< Schema version for migration

    // === Per-Dialog Path Memory ===
    /// Maps dialog_id → last-used directory path.
    /// Each file dialog in the app gets a unique ID (e.g., "import_csv", "load_video").
    std::map<std::string, std::string> last_used_paths;

    // === Recent Workspaces ===
    /// Most recently used workspace file paths, most recent first.
    /// Capped at max_recent_workspaces entries.
    std::vector<std::string> recent_workspaces;

    // === Window Geometry ===
    int window_x = 100;
    int window_y = 100;
    int window_width = 1400;
    int window_height = 900;
    bool window_maximized = false;

    // === Constants (not serialized — used by SessionStore) ===
    /// Maximum number of recent workspaces to keep
    static constexpr int max_recent_workspaces = 10;
};

}// namespace StateManagement

#endif// SESSION_DATA_HPP
