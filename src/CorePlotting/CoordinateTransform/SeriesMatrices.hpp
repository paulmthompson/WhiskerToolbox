#ifndef COREPLOTTING_COORDINATETRANSFORM_SERIESMATRICES_HPP
#define COREPLOTTING_COORDINATETRANSFORM_SERIESMATRICES_HPP

#include "Layout/LayoutTransform.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace CorePlotting {

// Forward declarations for display options structs
struct SeriesStyle;
struct SeriesLayoutResult;
struct SeriesDataCache;

/**
 * @file SeriesMatrices.hpp
 * @brief MVP (Model-View-Projection) matrix construction for time-series plotting
 * 
 * This module provides generic matrix generation utilities for CorePlotting.
 * Widget-specific composition logic (combining data normalization, user adjustments,
 * and layout positioning) should be done in the widget code using NormalizationHelpers
 * and LayoutTransform::compose().
 * 
 * **Model Matrix**: Per-series positioning and scaling
 *   - Create from LayoutTransform using createModelMatrix()
 *   - Or use legacy AnalogSeriesMatrixParams for backwards compatibility
 * 
 * **View Matrix**: Shared global camera transformations
 *   - Global vertical panning via createViewMatrix()
 * 
 * **Projection Matrix**: Shared coordinate system mapping
 *   - Maps time indices to screen X coordinates
 *   - Maps data space to screen Y coordinates
 *   - Vertical positioning based on layout allocation
 *   - Series-specific scaling (intrinsic, user, global)
 *   - Data centering around mean value
 * 
 * **View Matrix**: Shared global camera transformations
 *   - Global vertical panning
 *   - Mode-dependent behavior (FullCanvas vs Stacked)
 * 
 * **Projection Matrix**: Shared coordinate system mapping
 *   - Maps time indices to screen X coordinates
 *   - Maps data space to screen Y coordinates
 *   - Enforces valid ranges to prevent NaN/Infinity
 * 
 * The separation of these three matrices allows independent control of:
 * - Per-series layout and scaling (Model)
 * - User camera state (View)  
 * - Data-to-screen mapping (Projection)
 */

/**
 * @brief Helper struct for analog series matrix parameters
 * 
 * Bundles the parameters needed for analog series matrix generation.
 * This struct is passed to matrix functions instead of individual parameters
 * to reduce coupling and improve testability.
 */
struct AnalogSeriesMatrixParams {
    // Layout parameters (from SeriesLayoutResult)
    float allocated_y_center{0.0f};
    float allocated_height{1.0f};

    // Scaling parameters (from SeriesStyle)
    float intrinsic_scale{1.0f};
    float user_scale_factor{1.0f};
    float global_zoom{1.0f};
    float user_vertical_offset{0.0f};

    // Data statistics (from SeriesDataCache)
    float data_mean{0.0f};
    float std_dev{1.0f};

    // Global parameters
    float global_vertical_scale{1.0f};
};

/**
 * @brief Helper struct for event series matrix parameters
 * 
 * Parameters for digital event series MVP matrix generation.
 */
struct EventSeriesMatrixParams {
    // Layout parameters
    float allocated_y_center{0.0f};
    float allocated_height{1.0f};

    // Event-specific parameters
    float event_height{0.0f}; ///< Desired height for events (0 = use allocated)
    float margin_factor{0.8f};///< Vertical margin (0-1)
    float global_vertical_scale{1.0f};

    // Viewport bounds (for FullCanvas mode)
    float viewport_y_min{-1.0f};
    float viewport_y_max{1.0f};

    // Mode flag
    enum class PlottingMode {
        FullCanvas,///< Events extend full viewport height
        Stacked    ///< Events positioned within allocated space
    };
    PlottingMode plotting_mode{PlottingMode::Stacked};
};

/**
 * @brief Helper struct for interval series matrix parameters
 */
struct IntervalSeriesMatrixParams {
    // Layout parameters
    float allocated_y_center{0.0f};
    float allocated_height{1.0f};

    // Interval-specific parameters
    float margin_factor{1.0f};
    float global_zoom{1.0f};
    float global_vertical_scale{1.0f};

    // Mode flag
    bool extend_full_canvas{true};
};

/**
 * @brief Helper struct for shared view/projection parameters
 */
struct ViewProjectionParams {
    // Viewport bounds
    float viewport_y_min{-1.0f};
    float viewport_y_max{1.0f};

    // Panning state
    float vertical_pan_offset{0.0f};

    // Global scaling
    float global_zoom{1.0f};
    float global_vertical_scale{1.0f};
};

// ============================================================================
// Analog Time Series MVP Matrices
// ============================================================================

/**
 * @brief Create Model matrix for analog series positioning and scaling
 *
 * Implements three-tier scaling system:
 * 1. Intrinsic scaling (3*std_dev → ±1.0)
 * 2. User-specified scaling
 * 3. Global zoom
 * 
 * Centers data around mean value for proper visual centering.
 *
 * @param params Combined parameters for matrix generation
 * @return Model transformation matrix
 */
glm::mat4 getAnalogModelMatrix(AnalogSeriesMatrixParams const & params);

/**
 * @brief Create View matrix for analog series global transformations
 *
 * Applies view-level transformations to all analog series.
 * Handles global vertical panning.
 *
 * @param params View/projection parameters
 * @return View transformation matrix
 */
glm::mat4 getAnalogViewMatrix(ViewProjectionParams const & params);

/**
 * @brief Create Projection matrix for analog series coordinate mapping
 *
 * Maps data coordinates to normalized device coordinates [-1, 1].
 * Includes robust validation to prevent OpenGL state corruption.
 *
 * @param start_time_index Start of visible time range
 * @param end_time_index End of visible time range  
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @return Projection transformation matrix
 */
glm::mat4 getAnalogProjectionMatrix(TimeFrameIndex start_time_index,
                                    TimeFrameIndex end_time_index,
                                    float y_min,
                                    float y_max);

// ============================================================================
// Digital Event Series MVP Matrices
// ============================================================================

/**
 * @brief Create Model matrix for digital event series
 *
 * Handles both plotting modes:
 * - FullCanvas: Events extend from top to bottom of entire viewport
 * - Stacked: Events are positioned within allocated space
 *
 * @param params Event-specific parameters
 * @return Model transformation matrix
 */
glm::mat4 getEventModelMatrix(EventSeriesMatrixParams const & params);

/**
 * @brief Create View matrix for digital event series
 *
 * Behavior depends on plotting mode:
 * - FullCanvas: No panning (events stay viewport-pinned)
 * - Stacked: Applies panning (events move with content)
 *
 * @param params Event-specific parameters
 * @param view_params View/projection parameters  
 * @return View transformation matrix
 */
glm::mat4 getEventViewMatrix(EventSeriesMatrixParams const & params,
                             ViewProjectionParams const & view_params);

/**
 * @brief Create Projection matrix for digital event series
 *
 * Maps time indices and data coordinates to NDC.
 * Behavior is consistent across both plotting modes.
 *
 * @param start_time_index Start of visible time range
 * @param end_time_index End of visible time range
 * @param y_min Minimum Y coordinate in data space  
 * @param y_max Maximum Y coordinate in data space
 * @return Projection transformation matrix
 */
glm::mat4 getEventProjectionMatrix(TimeFrameIndex start_time_index,
                                   TimeFrameIndex end_time_index,
                                   float y_min,
                                   float y_max);

// ============================================================================
// Digital Interval Series MVP Matrices
// ============================================================================

/**
 * @brief Create Model matrix for digital interval series
 *
 * Intervals are rendered as rectangles extending vertically.
 * Supports full-canvas mode for background highlighting.
 *
 * @param params Interval-specific parameters
 * @return Model transformation matrix
 */
glm::mat4 getIntervalModelMatrix(IntervalSeriesMatrixParams const & params);

/**
 * @brief Create View matrix for digital interval series
 *
 * Intervals remain viewport-pinned (do not move with panning).
 *
 * @param params View/projection parameters
 * @return View transformation matrix (typically identity)
 */
glm::mat4 getIntervalViewMatrix(ViewProjectionParams const & params);

/**
 * @brief Create Projection matrix for digital interval series
 *
 * Maps time indices to horizontal extent, viewport bounds to vertical.
 *
 * @param start_time_index Start of visible time range
 * @param end_time_index End of visible time range
 * @param viewport_y_min Bottom of viewport in world coordinates
 * @param viewport_y_max Top of viewport in world coordinates  
 * @return Projection transformation matrix
 */
glm::mat4 getIntervalProjectionMatrix(TimeFrameIndex start_time_index,
                                      TimeFrameIndex end_time_index,
                                      float viewport_y_min,
                                      float viewport_y_max);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Validate and sanitize orthographic projection parameters
 *
 * Ensures parameters produce valid matrices without NaN/Infinity.
 * Applies minimum range constraints and clamps extreme values.
 *
 * @param left Left boundary (will be modified if invalid)
 * @param right Right boundary (will be modified if invalid)
 * @param bottom Bottom boundary (will be modified if invalid)
 * @param top Top boundary (will be modified if invalid)
 * @param context_name Name for debug messages (e.g., "Analog", "Event")
 * @return true if parameters were valid, false if corrections were applied
 */
bool validateOrthoParams(float & left, float & right,
                         float & bottom, float & top,
                         char const * context_name = "Matrix");

/**
 * @brief Validate that a matrix contains only finite values
 *
 * Checks all matrix elements for NaN or Infinity.
 * Returns identity matrix if validation fails.
 *
 * @param matrix Matrix to validate
 * @param context_name Name for debug messages
 * @return Validated matrix (original or identity if invalid)
 */
glm::mat4 validateMatrix(glm::mat4 const & matrix,
                         char const * context_name = "Matrix");

// ============================================================================
// Inverse Transform Utilities
// ============================================================================

/**
 * @brief Convert world Y coordinate to analog data value
 * 
 * Inverts the Model matrix transformation applied in getAnalogModelMatrix().
 * Use this to convert mouse cursor position (after screen→world transform)
 * to the corresponding data value for the series.
 * 
 * The forward transform is:
 *   y_world = (y_data - data_mean) * scale + allocated_y_center + user_offset
 * 
 * This function computes the inverse:
 *   y_data = (y_world - allocated_y_center - user_offset) / scale + data_mean
 * 
 * @param world_y Y coordinate in world space (post-View transform)
 * @param params Same parameters used to create the Model matrix
 * @return Corresponding data value in the analog series
 * 
 * @note The world_y should have View transforms already applied (i.e., pan offset
 *       should be accounted for before calling this function).
 */
[[nodiscard]] float worldYToAnalogValue(float world_y, AnalogSeriesMatrixParams const & params);

/**
 * @brief Convert analog data value to world Y coordinate
 * 
 * Forward transform from data space to world space.
 * This is the analytical equivalent of applying the Model matrix.
 * 
 * @param data_value Value in the analog series
 * @param params Same parameters used to create the Model matrix
 * @return Corresponding Y coordinate in world space
 */
[[nodiscard]] float analogValueToWorldY(float data_value, AnalogSeriesMatrixParams const & params);

// ============================================================================
// Simplified LayoutTransform-based API
// ============================================================================
// These functions work with composed LayoutTransforms instead of param structs.
// The caller is responsible for computing the final transform by composing:
//   1. Data normalization (from NormalizationHelpers)
//   2. Layout positioning (from LayoutEngine)
//   3. Any user adjustments

/**
 * @brief Create Model matrix from a LayoutTransform
 * 
 * This is the preferred API for creating Model matrices.
 * The LayoutTransform encapsulates all the Y-axis positioning and scaling.
 * 
 * @param y_transform Combined transform for Y axis (data normalization + layout + user adjustments)
 * @return Model matrix that applies the transform
 */
[[nodiscard]] inline glm::mat4 createModelMatrix(LayoutTransform const & y_transform) {
    return y_transform.toModelMatrixY();
}

/**
 * @brief Create Model matrix from separate X and Y transforms
 * 
 * For spatial data where both axes need transformation.
 * 
 * @param x_transform Transform for X axis
 * @param y_transform Transform for Y axis
 * @return Model matrix applying both transforms
 */
[[nodiscard]] inline glm::mat4 createModelMatrix(LayoutTransform const & x_transform,
                                                 LayoutTransform const & y_transform) {
    glm::mat4 m(1.0f);
    m[0][0] = x_transform.gain;  // X scale
    m[1][1] = y_transform.gain;  // Y scale
    m[3][0] = x_transform.offset;// X translation
    m[3][1] = y_transform.offset;// Y translation
    return m;
}

/**
 * @brief Create View matrix for vertical panning
 * 
 * @param vertical_pan Y offset for panning
 * @return View matrix with translation
 */
[[nodiscard]] inline glm::mat4 createViewMatrix(float vertical_pan = 0.0f) {
    glm::mat4 v(1.0f);
    v[3][1] = vertical_pan;
    return v;
}

/**
 * @brief Create standard time-series Projection matrix
 * 
 * Maps time range to X and viewport range to Y.
 * Includes validation to prevent invalid matrices.
 * 
 * @param start_time Start of visible time range
 * @param end_time End of visible time range
 * @param y_min Bottom of viewport
 * @param y_max Top of viewport
 * @return Orthographic projection matrix
 */
[[nodiscard]] glm::mat4 createProjectionMatrix(TimeFrameIndex start_time,
                                               TimeFrameIndex end_time,
                                               float y_min,
                                               float y_max);

}// namespace CorePlotting

#endif// COREPLOTTING_COORDINATETRANSFORM_SERIESMATRICES_HPP
