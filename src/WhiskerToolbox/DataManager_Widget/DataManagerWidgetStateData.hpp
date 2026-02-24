#ifndef DATA_MANAGER_WIDGET_STATE_DATA_HPP
#define DATA_MANAGER_WIDGET_STATE_DATA_HPP

/**
 * @file DataManagerWidgetStateData.hpp
 * @brief Serializable state data structure for DataManager_Widget
 *
 * Separated from DataManagerWidgetState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see DataManagerWidgetState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable data structure for DataManagerWidgetState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataManagerWidgetStateData {
    std::string selected_data_key;  ///< Currently selected data key in feature table
    std::string instance_id;        ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Manager";  ///< User-visible name
};

#endif // DATA_MANAGER_WIDGET_STATE_DATA_HPP
