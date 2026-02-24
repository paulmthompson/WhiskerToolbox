#ifndef DATA_TRANSFORM_WIDGET_STATE_DATA_HPP
#define DATA_TRANSFORM_WIDGET_STATE_DATA_HPP

/**
 * @file DataTransformWidgetStateData.hpp
 * @brief Serializable state data structure for DataTransform_Widget
 *
 * Separated from DataTransformWidgetState.hpp so it can be included without
 * pulling in Qt dependencies (e.g. for fuzz testing).
 *
 * @see DataTransformWidgetState for the Qt wrapper class
 */

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable data structure for DataTransformWidgetState
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct DataTransformWidgetStateData {
    std::string selected_input_data_key;  ///< Input data key for transform (from SelectionContext)
    std::string selected_operation;        ///< Currently selected transform operation name
    std::string last_output_name;          ///< Last used output name (for convenience)
    std::string instance_id;               ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Data Transform";  ///< User-visible name
};

#endif // DATA_TRANSFORM_WIDGET_STATE_DATA_HPP
