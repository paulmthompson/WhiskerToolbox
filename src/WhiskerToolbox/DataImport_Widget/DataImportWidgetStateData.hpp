#ifndef DATA_IMPORT_WIDGET_STATE_DATA_HPP
#define DATA_IMPORT_WIDGET_STATE_DATA_HPP

/**
 * @file DataImportWidgetStateData.hpp
 * @brief Serializable state data structure for DataImport_Widget
 *
 * Separated from DataImportWidgetState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see DataImportWidgetState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <map>
#include <string>

/**
 * @brief Serializable data structure for DataImportWidgetState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataImportWidgetStateData {
    std::string selected_import_type;                        ///< Currently selected data type (e.g., "LineData")
    std::string last_used_directory;                         ///< Persistent directory preference for file dialogs
    std::map<std::string, std::string> format_preferences;   ///< Per-type format preferences (e.g., "LineData" -> "CSV")
    std::string instance_id;                                 ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Import";                ///< User-visible name
};

#endif // DATA_IMPORT_WIDGET_STATE_DATA_HPP
