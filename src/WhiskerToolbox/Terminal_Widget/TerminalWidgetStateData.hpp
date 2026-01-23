#ifndef TERMINAL_WIDGET_STATE_DATA_HPP
#define TERMINAL_WIDGET_STATE_DATA_HPP

/**
 * @file TerminalWidgetStateData.hpp
 * @brief Serializable state data structure for TerminalWidget
 * 
 * This file defines the state structure that TerminalWidgetState serializes to JSON
 * using reflect-cpp.
 * 
 * ## Purpose
 * 
 * TerminalWidget captures stdout/stderr output and displays it in a terminal-like
 * interface. The state tracks user preferences for display and buffer management.
 * 
 * @see TerminalWidgetState for the Qt wrapper class
 * @see TerminalWidget for the view implementation
 */

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable state data for TerminalWidget
 * 
 * All members are designed for reflect-cpp serialization.
 * No Qt types are used to ensure clean JSON output.
 */
struct TerminalWidgetStateData {
    // === Metadata ===
    std::string instance_id;                          ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Terminal";            ///< User-visible name
    
    // === Display Preferences ===
    bool auto_scroll = true;           ///< Auto-scroll to bottom when new output arrives
    bool show_timestamps = true;       ///< Show timestamps for each line
    bool word_wrap = true;             ///< Enable word wrapping
    
    // === Buffer Settings ===
    int max_lines = 10000;             ///< Maximum number of lines to keep in buffer
    
    // === Visual Settings ===
    int font_size = 10;                ///< Font size in points (range: 8 to 24)
    std::string font_family = "Consolas";  ///< Font family name
    
    // === Colors ===
    std::string background_color = "#000000";   ///< Background color (hex)
    std::string text_color = "#FFFFFF";         ///< Normal text color (hex)
    std::string error_color = "#FF6B6B";        ///< Error text color (hex)
    std::string system_color = "#FFAA00";       ///< System message color (hex)
};

#endif // TERMINAL_WIDGET_STATE_DATA_HPP
