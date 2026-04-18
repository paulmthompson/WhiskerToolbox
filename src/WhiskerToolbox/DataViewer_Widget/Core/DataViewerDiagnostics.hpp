/**
 * @file DataViewerDiagnostics.hpp
 * @brief Lightweight diagnostic data structs for the developer debug panel
 *
 * These structs are returned by OpenGLWidget::getDiagnostics() to expose
 * internal layout and rendering state without leaking implementation details
 * like SceneCacheState, TimeSeriesDataStore, or LayoutResponse.
 */

#ifndef DATAVIEWER_DIAGNOSTICS_HPP
#define DATAVIEWER_DIAGNOSTICS_HPP

#include <string>
#include <vector>

/**
 * @brief Per-lane diagnostic snapshot for one series
 *
 * Captures the layout, transform, and cache state of a single series
 * at the time getDiagnostics() is called.
 */
struct LaneDiagnostic {
    std::string series_key;
    std::string series_type;///< "Analog", "DigitalEvent", or "DigitalInterval"
    int lane_index = 0;
    bool is_visible = true;
    float center_y_ndc = 0.0f;     ///< Y offset in NDC from LayoutTransform
    float height_ndc = 0.0f;       ///< Vertical extent in NDC derived from gain
    float transform_offset = 0.0f; ///< LayoutTransform::offset
    float transform_gain = 1.0f;   ///< LayoutTransform::gain
    float user_scale_factor = 1.0f;///< Analog-only per-series scale
    float data_mean = 0.0f;        ///< Analog-only cached mean
    float data_std_dev = 0.0f;     ///< Analog-only cached std dev
    int vertex_count = 0;
    bool is_on_screen = true;///< Does lane NDC extent intersect the viewport?
};

/**
 * @brief Aggregate rendering statistics for the current scene
 */
struct RenderingStats {
    int polyline_batch_count = 0;
    int glyph_batch_count = 0;
    int rectangle_batch_count = 0;
    int total_vertex_count = 0;
    int total_entity_count = 0;
    int scene_rebuild_count = 0;
};

/**
 * @brief Complete diagnostics snapshot returned by OpenGLWidget::getDiagnostics()
 */
struct DataViewerDiagnostics {
    std::vector<LaneDiagnostic> lanes;
    RenderingStats rendering;
};

#endif// DATAVIEWER_DIAGNOSTICS_HPP
