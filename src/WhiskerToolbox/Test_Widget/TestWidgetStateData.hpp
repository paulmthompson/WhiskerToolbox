#ifndef TEST_WIDGET_STATE_DATA_HPP
#define TEST_WIDGET_STATE_DATA_HPP

/**
 * @file TestWidgetStateData.hpp
 * @brief Serializable state data structure for TestWidget
 * 
 * This file defines the state structure that TestWidgetState serializes to JSON
 * using reflect-cpp. It demonstrates the View/Properties split pattern with
 * simple but meaningful features.
 * 
 * ## Purpose
 * 
 * TestWidget serves as a proof-of-concept for the View/Properties split pattern:
 * - TestWidgetView displays the visualization
 * - TestWidgetProperties provides the controls
 * - Both share the same TestWidgetState instance
 * 
 * ## Features Demonstrated
 * 
 * - Boolean toggles (checkboxes in properties)
 * - Color selection (color picker in properties)
 * - Numeric values (slider/spinbox in properties)
 * - Text values (line edit in properties)
 * 
 * @see TestWidgetState for the Qt wrapper class
 * @see TestWidgetView for the visualization component
 * @see TestWidgetProperties for the controls component
 */

#include <rfl.hpp>

#include <string>

/**
 * @brief Serializable state data for TestWidget
 * 
 * All members are designed for reflect-cpp serialization.
 * No Qt types are used to ensure clean JSON output.
 */
struct TestWidgetStateData {
    // === Metadata ===
    std::string instance_id;                      ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Test Widget";     ///< User-visible name
    
    // === Feature Toggles ===
    bool show_grid = true;           ///< Show grid overlay in the view
    bool show_crosshair = false;     ///< Show crosshair at center
    bool enable_animation = false;   ///< Enable animated element
    
    // === Color ===
    std::string highlight_color = "#FF5500";  ///< Highlight/accent color (hex format)
    
    // === Numeric Values ===
    double zoom_level = 1.0;   ///< Zoom level (range: 0.1 to 5.0)
    int grid_spacing = 50;     ///< Grid spacing in pixels (range: 10 to 200)
    
    // === Text ===
    std::string label_text = "Test Label";  ///< Label text displayed in view
};

#endif // TEST_WIDGET_STATE_DATA_HPP
