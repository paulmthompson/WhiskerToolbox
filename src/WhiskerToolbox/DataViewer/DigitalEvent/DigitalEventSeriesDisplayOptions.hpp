#ifndef DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP

#include "CorePlotting/DataTypes/SeriesStyle.hpp"
#include "CorePlotting/DataTypes/SeriesLayoutResult.hpp"

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
 * 
 * ARCHITECTURE NOTE (Phase 0 Cleanup):
 * This struct has been refactored to use CorePlotting's separated concerns:
 * - `style`: Pure visual configuration (color, alpha, thickness) - user-settable
 * - `layout`: Positioning output from LayoutEngine - read-only computed values
 * 
 * This separation clarifies ownership and prevents conflation of concerns.
 * See CorePlotting/DESIGN.md and ROADMAP.md Phase 0 for details.
 */
struct NewDigitalEventSeriesDisplayOptions {
    // ========== Separated Concerns (Phase 0 Refactoring) ==========
    
    /// Pure rendering style (user-configurable)
    CorePlotting::SeriesStyle style{CorePlotting::SeriesStyle{"#ff9500", 0.8f}};
    
    /// Layout output (computed by PlottingManager/LayoutEngine)
    CorePlotting::SeriesLayoutResult layout{0.0f, 2.0f}; // Default height 2.0
    
    // ========== Event-Specific Configuration ==========

    // Plotting mode configuration
    EventPlottingMode plotting_mode{EventPlottingMode::FullCanvas};///< How events should be plotted

    // Legacy compatibility members for widget configuration
    EventDisplayMode display_mode{EventDisplayMode::Stacked};///< Legacy display mode for widget compatibility
    float vertical_spacing{0.1f};                            ///< Vertical spacing between stacked events (legacy)
    float event_height{0.05f};                               ///< Height of individual events (legacy)

    // Global scaling applied by PlottingManager
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor

    // Rendering options
    float margin_factor{0.95f};///< Margin factor for event height (0.95 = 95% of allocated space)
    
};

#endif// DATAVIEWER_DIGITALEVENTSERIESDISPLAYOPTIONS_HPP
