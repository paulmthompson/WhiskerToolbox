#ifndef DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP

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
 */
struct NewAnalogTimeSeriesDisplayOptions {
    // Visual properties
    std::string hex_color{"#007bff"};
    float alpha{1.0f};
    bool is_visible{true};
    int line_thickness{1};

    // Scaling configuration
    AnalogScalingConfig scaling;

    // Legacy compatibility members for backward compatibility with OpenGLWidget
    float scale_factor{1.0f};                                        ///< Legacy scale factor (computed from std dev)
    float user_scale_factor{1.0f};                                   ///< Legacy user scale factor
    float y_offset{0.0f};                                            ///< Legacy Y offset for positioning
    AnalogGapHandling gap_handling{AnalogGapHandling::AlwaysConnect};///< Gap handling mode
    bool enable_gap_detection{false};                                ///< Enable automatic gap detection
    float gap_threshold{5.0f};                                       ///< Threshold for gap detection

    // Positioning allocated by PlottingManager
    float allocated_y_center{0.0f};///< Y-coordinate center allocated by plotting manager
    float allocated_height{1.0f};  ///< Height allocated by plotting manager

    // Data range information (for optimization)
    mutable float cached_std_dev{0.0f};
    mutable bool std_dev_cache_valid{false};
    mutable float cached_mean{0.0f};
    mutable bool mean_cache_valid{false};
};


#endif// DATAVIEWER_ANALOGTIMESERIESDISPLAYOPTIONS_HPP
