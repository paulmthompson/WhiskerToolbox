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
    
};


#endif// DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP
