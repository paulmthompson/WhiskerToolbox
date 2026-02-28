#ifndef DATA_INSPECTOR_STATE_DATA_HPP
#define DATA_INSPECTOR_STATE_DATA_HPP

/**
 * @file DataInspectorStateData.hpp
 * @brief Serializable state data structure for DataInspector_Widget
 *
 * Separated from DataInspectorState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see DataInspectorState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <string>
#include <vector>

/**
 * @brief Serializable data structure for DataInspectorState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataInspectorStateData {
    std::string inspected_data_key;  ///< Currently inspected data key
    bool is_pinned = false;          ///< Whether to ignore SelectionContext updates
    std::string display_name = "Data Inspector";  ///< User-visible name
    std::string instance_id;         ///< Unique instance ID (preserved across serialization)
    std::vector<std::string> collapsed_sections;  ///< Section IDs that are collapsed in UI

    // Type-specific UI state stored as JSON string for flexibility
    // This allows type-specific widgets to store their preferences without
    // needing per-type state classes
    std::string ui_state_json = "{}";
};

#endif // DATA_INSPECTOR_STATE_DATA_HPP
