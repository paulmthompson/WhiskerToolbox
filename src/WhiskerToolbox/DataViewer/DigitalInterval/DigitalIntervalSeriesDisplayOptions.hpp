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
    
    // ========== Legacy Accessors (for backward compatibility) ==========
    
    // Visual properties - forward to style
    [[nodiscard]] std::string const& hex_color() const { return style.hex_color; }
    [[nodiscard]] float alpha() const { return style.alpha; }
    [[nodiscard]] bool is_visible() const { return style.is_visible; }
    
    // Mutable setters for style
    void set_hex_color(std::string color) { style.hex_color = std::move(color); }
    void set_alpha(float a) { style.alpha = a; }
    void set_visible(bool visible) { style.is_visible = visible; }
    
    // Layout properties - forward to layout
    [[nodiscard]] float allocated_y_center() const { return layout.allocated_y_center; }
    [[nodiscard]] float allocated_height() const { return layout.allocated_height; }
    
    void set_allocated_y_center(float y) { layout.allocated_y_center = y; }
    void set_allocated_height(float h) { layout.allocated_height = h; }
};

#endif// DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP
