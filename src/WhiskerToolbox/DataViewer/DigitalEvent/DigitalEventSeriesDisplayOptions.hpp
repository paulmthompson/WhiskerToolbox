#ifndef DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP

#include <string>

/**
 * @brief Plotting mode for digital event series
 */
enum class EventPlottingMode {
    FullCanvas,///< Events extend from top to bottom of entire plot (like digital intervals)
    Stacked    ///< Events are allocated a portion of canvas and extend within that space (like analog series)
};

/**
 * @brief Event display modes for digital event series
 *
 * Controls how digital events are visually presented on the canvas:
 * - Stacked: Events are positioned in separate horizontal lanes with configurable spacing
 * - FullCanvas: Events stretch from top to bottom of the entire canvas
 */
enum class EventDisplayMode {
    Stacked,  ///< Stack events with configurable spacing (default)
    FullCanvas///< Events stretch from top to bottom of canvas
};


/**
 * @brief Display options for new digital event series visualization system
 *
 * Configuration for digital event series display including visual properties,
 * positioning, plotting mode, and rendering options. Events are rendered as
 * vertical lines extending either across full canvas or within allocated space.
 */
struct NewDigitalEventSeriesDisplayOptions {
    // Visual properties
    std::string hex_color{"#ff9500"};///< Color of the events (default: orange)
    float alpha{0.8f};               ///< Alpha transparency for events (default: 80%)
    bool is_visible{true};           ///< Whether events are visible
    int line_thickness{2};           ///< Thickness of event lines

    // Plotting mode configuration
    EventPlottingMode plotting_mode{EventPlottingMode::FullCanvas};///< How events should be plotted

    // Legacy compatibility members for widget configuration
    EventDisplayMode display_mode{EventDisplayMode::Stacked};///< Legacy display mode for widget compatibility
    float vertical_spacing{0.1f};                            ///< Vertical spacing between stacked events (legacy)
    float event_height{0.05f};                               ///< Height of individual events (legacy)

    // Global scaling applied by PlottingManager
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor

    // Positioning allocated by PlottingManager (used in Stacked mode)
    float allocated_y_center{0.0f};///< Y-coordinate center allocated by plotting manager
    float allocated_height{2.0f};  ///< Height allocated by plotting manager

    // Rendering options
    float margin_factor{0.95f};///< Margin factor for event height (0.95 = 95% of allocated space)
};

#endif// DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP
