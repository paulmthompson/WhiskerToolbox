#ifndef WORKSPACE_DATA_HPP
#define WORKSPACE_DATA_HPP

/**
 * @file WorkspaceData.hpp
 * @brief Serializable data structures for workspace save/restore
 *
 * WorkspaceData defines the "envelope" that ties together all the state
 * needed to reconstruct a WhiskerToolbox session:
 *  - What data was loaded (DataLoadEntry list)
 *  - Widget states (serialized via EditorRegistry)
 *  - Zone layout (serialized via ZoneConfig)
 *
 * DataLoadEntry is a reflect-cpp struct for unit testing of individual entries.
 * The workspace file itself is written as a clean JSON document using
 * nlohmann::json (so that embedded editor_states and zone_layout appear
 * as nested JSON objects rather than escaped strings).
 *
 * @see WorkspaceManager for the save/load orchestration
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 2
 */

#include <cstddef>
#include <string>
#include <vector>

namespace StateManagement {

/**
 * @brief Record of a single data-loading operation
 *
 * Captured every time the user loads data so that the workspace file
 * can replay the load sequence on restore.
 */
struct DataLoadEntry {
    /// Discriminator for the loading mechanism.
    /// Values: "json_config", "video", "images"
    std::string loader_type;

    /// Absolute path to the source (JSON config file, video file, or image directory).
    std::string source_path;
};

/**
 * @brief Complete workspace state
 *
 * This struct is used internally by WorkspaceManager for capture/restore.
 * It is NOT directly serialized via reflect-cpp; the workspace file is
 * written with nlohmann::json so that nested JSON remains readable.
 */
struct WorkspaceData {
    std::string version = "1.0";
    std::string created_at;  ///< ISO 8601 timestamp
    std::string modified_at; ///< ISO 8601 timestamp

    /// Ordered list of data loads performed during the session
    std::vector<DataLoadEntry> data_loads;

    /// Opaque JSON string from EditorRegistry::toJson()
    std::string editor_states_json;

    /// Opaque JSON string from ZoneConfig::saveToJson()
    std::string zone_layout_json;
};

} // namespace StateManagement

#endif // WORKSPACE_DATA_HPP
