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
#include <vector>

struct PythonWidgetStateData {
    std::string instance_id;
    std::string display_name = "Python Console";

    /// Last script path that was loaded/run
    std::string last_script_path;

    /// Whether to auto-scroll the output
    bool auto_scroll = true;

    /// Font size for the editor/output
    int font_size = 10;

    /// Command history for the console REPL
    std::vector<std::string> command_history;

    /// Recent script file paths
    std::vector<std::string> recent_scripts;

    /// Active virtual environment path (Phase 6)
    std::string venv_path;

    /// Last working directory for scripts
    std::string last_working_directory;

    /// Whether to show line numbers in the editor
    bool show_line_numbers = true;

    /// Unsaved editor buffer content
    std::string editor_content;

    /// Script arguments passed via sys.argv (Phase 5)
    std::string script_arguments;

    /// Auto-import prelude code executed on interpreter init (Phase 5)
    std::string auto_import_prelude = "import numpy as np\nfrom whiskertoolbox_python import *";

    /// Whether the auto-import prelude is enabled (Phase 5)
    bool prelude_enabled = true;
};

#endif // PYTHON_WIDGET_STATE_DATA_HPP
