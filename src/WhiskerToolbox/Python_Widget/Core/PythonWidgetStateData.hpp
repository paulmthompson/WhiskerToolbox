#ifndef PYTHON_WIDGET_STATE_DATA_HPP
#define PYTHON_WIDGET_STATE_DATA_HPP

/**
 * @file PythonWidgetStateData.hpp
 * @brief Serializable state data structure for PythonWidget
 *
 * Plain data struct for reflect-cpp serialization. No Qt types.
 */

#include <rfl.hpp>

#include <string>

struct PythonWidgetStateData {
    std::string instance_id;
    std::string display_name = "Python Console";

    /// Last script path that was loaded/run
    std::string last_script_path;

    /// Whether to auto-scroll the output
    bool auto_scroll = true;

    /// Font size for the editor/output
    int font_size = 10;
};

#endif // PYTHON_WIDGET_STATE_DATA_HPP
