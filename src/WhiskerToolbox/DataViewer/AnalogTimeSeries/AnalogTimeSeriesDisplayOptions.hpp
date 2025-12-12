#ifndef DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP

#include "CorePlotting/DataTypes/SeriesStyle.hpp"
#include "CorePlotting/DataTypes/SeriesLayoutResult.hpp"
#include "CorePlotting/DataTypes/SeriesDataCache.hpp"

#include <string>

enum class AnalogGapHandling {
    AlwaysConnect,// Always connect points (current behavior)
    DetectGaps,   // Break lines when gaps exceed threshold
    ShowMarkers   // Show individual markers instead of lines
};

/**
 * @brief Scaling configuration for analog time series data
 *
 * Defines the three categories of scaling: intrinsic (data-based),
 * user-specified, and global scaling factors.
 */
struct AnalogScalingConfig {
    // Intrinsic scaling based on data characteristics
    float intrinsic_scale{1.0f}; ///< Normalization based on data properties (e.g., 3*std_dev)
    float intrinsic_offset{0.0f};///< Data-based vertical offset

    // User-specified scaling controls
    float user_scale_factor{1.0f};   ///< User-controlled amplitude scaling
    float user_vertical_offset{0.0f};///< User-controlled vertical positioning

    // Global scaling applied to all series
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor
};


/**
 * @brief Display options for new analog time series visualization system
 *
 * Comprehensive configuration for analog series display including scaling,
 * positioning, and visual properties.
 * 
 * ARCHITECTURE NOTE (Phase 0 Cleanup):
 * This struct has been refactored to use CorePlotting's separated concerns:
 * - `style`: Pure visual configuration (color, alpha, thickness) - user-settable
 * - `layout`: Positioning output from LayoutEngine - read-only computed values
 * - `data_cache`: Expensive statistical calculations - mutable cache
 * 
 * This separation clarifies ownership and prevents conflation of concerns.
 * See CorePlotting/DESIGN.md and ROADMAP.md Phase 0 for details.
 */
struct NewAnalogTimeSeriesDisplayOptions {
    // ========== Separated Concerns (Phase 0 Refactoring) ==========
    
    /// Pure rendering style (user-configurable)
    CorePlotting::SeriesStyle style;
    
    /// Layout output (computed by PlottingManager/LayoutEngine)
    CorePlotting::SeriesLayoutResult layout;
    
    /// Cached statistical data (mutable, invalidated on data change)
    CorePlotting::SeriesDataCache data_cache;
    
    // ========== Analog-Specific Configuration ==========

    // Scaling configuration
    AnalogScalingConfig scaling;

    // Legacy compatibility members for backward compatibility with OpenGLWidget
    float scale_factor{1.0f};                                        ///< Legacy scale factor (computed from std dev)
    float user_scale_factor{1.0f};                                   ///< Legacy user scale factor
    float y_offset{0.0f};                                            ///< Legacy Y offset for positioning
    AnalogGapHandling gap_handling{AnalogGapHandling::AlwaysConnect};///< Gap handling mode
    bool enable_gap_detection{false};                                ///< Enable automatic gap detection
    float gap_threshold{5.0f};                                       ///< Threshold for gap detection
    
    // ========== Legacy Accessors (for backward compatibility) ==========
    
    // Visual properties - forward to style
    [[nodiscard]] std::string const& hex_color() const { return style.hex_color; }
    [[nodiscard]] float alpha() const { return style.alpha; }
    [[nodiscard]] bool is_visible() const { return style.is_visible; }
    [[nodiscard]] int line_thickness() const { return style.line_thickness; }
    
    // Mutable setters for style
    void set_hex_color(std::string color) { style.hex_color = std::move(color); }
    void set_alpha(float a) { style.alpha = a; }
    void set_visible(bool visible) { style.is_visible = visible; }
    void set_line_thickness(int thickness) { style.line_thickness = thickness; }
    
    // Layout properties - forward to layout
    [[nodiscard]] float allocated_y_center() const { return layout.allocated_y_center; }
    [[nodiscard]] float allocated_height() const { return layout.allocated_height; }
    
    void set_allocated_y_center(float y) { layout.allocated_y_center = y; }
    void set_allocated_height(float h) { layout.allocated_height = h; }
    
    // Cache properties - forward to data_cache
    [[nodiscard]] float cached_std_dev() const { return data_cache.cached_std_dev; }
    [[nodiscard]] bool std_dev_cache_valid() const { return data_cache.std_dev_cache_valid; }
    [[nodiscard]] float cached_mean() const { return data_cache.cached_mean; }
    [[nodiscard]] bool mean_cache_valid() const { return data_cache.mean_cache_valid; }
    
    void set_cached_std_dev(float val) const { data_cache.cached_std_dev = val; }
    void set_std_dev_cache_valid(bool valid) const { data_cache.std_dev_cache_valid = valid; }
    void set_cached_mean(float val) const { data_cache.cached_mean = val; }
    void set_mean_cache_valid(bool valid) const { data_cache.mean_cache_valid = valid; }
    
    void invalidate_cache() { data_cache.invalidate(); }
};


#endif// DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP
