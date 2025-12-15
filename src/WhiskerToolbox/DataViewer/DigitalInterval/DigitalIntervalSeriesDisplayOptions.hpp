#ifndef DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP

#include "CorePlotting/DataTypes/SeriesStyle.hpp"
#include "CorePlotting/DataTypes/SeriesLayoutResult.hpp"

#include <string>

/**
 * @brief Display options for new digital interval series visualization system
 *
 * Configuration for digital interval series display including visual properties,
 * positioning, and rendering options. Intervals are rendered as rectangles
 * extending from top to bottom of canvas with lighter alpha.
 * 
 * ARCHITECTURE NOTE (Phase 0 Cleanup):
 * This struct has been refactored to use CorePlotting's separated concerns:
 * - `style`: Pure visual configuration (color, alpha, thickness) - user-settable
 * - `layout`: Positioning output from LayoutEngine - read-only computed values
 * 
 * This separation clarifies ownership and prevents conflation of concerns.
 * See CorePlotting/DESIGN.md and ROADMAP.md Phase 0 for details.
 */
struct NewDigitalIntervalSeriesDisplayOptions {
    // ========== Separated Concerns (Phase 0 Refactoring) ==========
    
    /// Pure rendering style (user-configurable)
    CorePlotting::SeriesStyle style{CorePlotting::SeriesStyle{"#ff6b6b", 0.3f}};
    
    /// Layout output (computed by PlottingManager/LayoutEngine)
    CorePlotting::SeriesLayoutResult layout{0.0f, 2.0f}; // Default height 2.0 (full canvas)
    
    // ========== Interval-Specific Configuration ==========

    // Global scaling applied by PlottingManager
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor

    // Rendering options
    bool extend_full_canvas{true};///< Whether intervals extend from top to bottom of canvas
    float margin_factor{0.95f};   ///< Margin factor for interval height (0.95 = 95% of allocated space)
    float interval_height{1.0f};  ///< Height of the interval (1.0 = full canvas by default)
    
};

#endif// DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP
