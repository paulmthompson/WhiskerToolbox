#ifndef DATAVIEWER_WIDGET_SCENEBUILDINGHELPERS_HPP
#define DATAVIEWER_WIDGET_SCENEBUILDINGHELPERS_HPP

/**
 * @file SceneBuildingHelpers.hpp
 * @brief Helper functions to convert DataViewer series data into CorePlotting RenderableBatches.
 * 
 * These helpers bridge the gap between the existing OpenGLWidget data storage
 * (AnalogSeriesData, DigitalEventSeriesData, etc.) and the new CorePlotting
 * primitive types (RenderablePolyLineBatch, RenderableGlyphBatch, etc.).
 * 
 * Usage:
 * @code
 *   // Build a polyline batch from analog series
 *   auto batch = buildAnalogSeriesBatch(series, time_frame, time_range, model_params, view_params);
 *   polyline_renderer.uploadData(batch);
 * @endcode
 */

#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "AnalogVertexCache.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

// Forward declarations
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;
struct NewAnalogTimeSeriesDisplayOptions;
struct NewDigitalEventSeriesDisplayOptions;
struct NewDigitalIntervalSeriesDisplayOptions;

namespace DataViewerHelpers {

/**
 * @brief Rendering mode for analog series.
 */
enum class AnalogRenderMode {
    Line,   ///< Render as connected line strip (default)
    Markers ///< Render as individual point markers
};

/**
 * @brief Parameters for building an analog series batch.
 */
struct AnalogBatchParams {
    TimeFrameIndex start_time{0};
    TimeFrameIndex end_time{0};
    float gap_threshold{1.0f};///< Time index gap threshold for segment breaks
    bool detect_gaps{true};   ///< Whether to break lines at gaps
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float thickness{1.0f};
    AnalogRenderMode render_mode{AnalogRenderMode::Line}; ///< How to render the series
};

/**
 * @brief Build a RenderablePolyLineBatch from an AnalogTimeSeries.
 * 
 * Converts the analog data into GPU-ready vertex data. If gap detection is enabled,
 * the batch will contain multiple line segments broken at gaps.
 * 
 * @param series The analog time series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters
 * @param model_params Parameters for building the Model matrix
 * @param view_params Parameters for building the View matrix
 * @return A batch ready for upload to PolyLineRenderer
 */
CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatch(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        CorePlotting::AnalogSeriesMatrixParams const & model_params,
        CorePlotting::ViewProjectionParams const & view_params);

/**
 * @brief Build a RenderableGlyphBatch for an AnalogTimeSeries in marker mode.
 * 
 * Converts the analog data into individual point markers instead of a connected line.
 * Used when gap_handling is set to ShowMarkers.
 * 
 * @param series The analog time series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters
 * @param model_params Parameters for building the Model matrix
 * @param view_params Parameters for building the View matrix
 * @return A batch ready for upload to GlyphRenderer
 */
CorePlotting::RenderableGlyphBatch buildAnalogSeriesMarkerBatch(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        CorePlotting::AnalogSeriesMatrixParams const & model_params,
        CorePlotting::ViewProjectionParams const & view_params);

/**
 * @brief Parameters for building a digital event series batch.
 */
struct EventBatchParams {
    TimeFrameIndex start_time{0};
    TimeFrameIndex end_time{0};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float glyph_size{5.0f};
    CorePlotting::RenderableGlyphBatch::GlyphType glyph_type{
            CorePlotting::RenderableGlyphBatch::GlyphType::Tick};
};

/**
 * @brief Build a RenderableGlyphBatch from a DigitalEventSeries.
 * 
 * For events rendered as ticks (vertical lines), we use glyphs positioned at
 * the event times. The Model matrix handles vertical positioning.
 * 
 * @param series The digital event series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters
 * @param model_params Parameters for building the Model matrix
 * @param view_params Parameters for building the View matrix
 * @return A batch ready for upload to GlyphRenderer
 */
CorePlotting::RenderableGlyphBatch buildEventSeriesBatch(
        DigitalEventSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        EventBatchParams const & params,
        CorePlotting::EventSeriesMatrixParams const & model_params,
        CorePlotting::ViewProjectionParams const & view_params);

/**
 * @brief Parameters for building a digital interval series batch.
 */
struct IntervalBatchParams {
    TimeFrameIndex start_time{0};
    TimeFrameIndex end_time{0};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 0.5f};
};

/**
 * @brief Build a RenderableRectangleBatch from a DigitalIntervalSeries.
 * 
 * Converts intervals to rectangles with X coordinates from interval bounds
 * and Y coordinates normalized to [-1, 1] for the Model matrix to position.
 * 
 * @param series The digital interval series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters
 * @param model_params Parameters for building the Model matrix
 * @param view_params Parameters for building the View matrix
 * @return A batch ready for upload to RectangleRenderer
 */
CorePlotting::RenderableRectangleBatch buildIntervalSeriesBatch(
        DigitalIntervalSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        IntervalBatchParams const & params,
        CorePlotting::IntervalSeriesMatrixParams const & model_params,
        CorePlotting::ViewProjectionParams const & view_params);

/**
 * @brief Build highlight rectangle for a selected interval.
 * 
 * Creates a separate batch for the selection highlight border.
 * 
 * @param start_time Start time of the interval (master time frame)
 * @param end_time End time of the interval (master time frame)
 * @param highlight_color Color for the highlight (typically brighter)
 * @param model_matrix Model matrix from the parent interval series
 * @return A batch containing just the highlight rectangle
 */
CorePlotting::RenderableRectangleBatch buildIntervalHighlightBatch(
        int64_t start_time,
        int64_t end_time,
        glm::vec4 const & highlight_color,
        glm::mat4 const & model_matrix);

/**
 * @brief Build highlight border polylines for a selected interval.
 * 
 * Creates a polyline batch containing the four edges of the selection rectangle.
 * This is drawn on top of the filled rectangle for visual emphasis.
 * 
 * @param start_time Start time of the interval (master time frame)
 * @param end_time End time of the interval (master time frame)
 * @param highlight_color Color for the border (typically brighter than fill)
 * @param border_thickness Line width for the border
 * @param model_matrix Model matrix from the parent interval series
 * @return A batch containing the four border lines
 */
CorePlotting::RenderablePolyLineBatch buildIntervalHighlightBorderBatch(
        int64_t start_time,
        int64_t end_time,
        glm::vec4 const & highlight_color,
        float border_thickness,
        glm::mat4 const & model_matrix);

// ============================================================================
// Simplified API using LayoutTransform (Phase 4.13)
// ============================================================================
// These functions eliminate the intermediate param structs by taking 
// a pre-composed LayoutTransform or Model matrix directly.

/**
 * @brief Simplified analog batch building with pre-composed model matrix
 * 
 * Uses a pre-composed Model matrix instead of AnalogSeriesMatrixParams.
 * The caller computes the model matrix using CorePlotting::composeAnalogYTransform().
 * 
 * @param series The analog time series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters (time range, gap config, style)
 * @param model_matrix Pre-computed Model matrix
 * @return A batch ready for upload to PolyLineRenderer
 */
CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatchSimplified(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix);

/**
 * @brief Simplified analog marker batch building with pre-composed model matrix
 */
CorePlotting::RenderableGlyphBatch buildAnalogSeriesMarkerBatchSimplified(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix);

/**
 * @brief Simplified event batch building with pre-composed model matrix
 */
CorePlotting::RenderableGlyphBatch buildEventSeriesBatchSimplified(
        DigitalEventSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        EventBatchParams const & params,
        glm::mat4 const & model_matrix);

/**
 * @brief Simplified interval batch building with pre-composed model matrix
 */
CorePlotting::RenderableRectangleBatch buildIntervalSeriesBatchSimplified(
        DigitalIntervalSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        IntervalBatchParams const & params,
        glm::mat4 const & model_matrix);

// ============================================================================
// Cached Vertex API for efficient scrolling (Phase: Streaming Optimization)
// ============================================================================
// These functions use AnalogVertexCache to minimize vertex regeneration
// when scrolling time series data.

/**
 * @brief Build analog batch using vertex cache for efficient scrolling
 * 
 * This function implements the ring buffer optimization strategy:
 * 1. Check if cache covers the requested range
 * 2. If not, generate only the missing edge data
 * 3. Update the cache with new vertices
 * 4. Return batch built from cached vertices
 * 
 * For typical scrolling (scroll by 10-100 points out of 100K visible),
 * this is ~26-130x faster than regenerating all vertices.
 * 
 * @param series The analog time series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param params Batch building parameters (time range, gap config, style)
 * @param model_matrix Pre-computed Model matrix
 * @param cache Vertex cache (will be updated with new data)
 * @return A batch ready for upload to PolyLineRenderer
 */
CorePlotting::RenderablePolyLineBatch buildAnalogSeriesBatchCached(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        AnalogBatchParams const & params,
        glm::mat4 const & model_matrix,
        DataViewer::AnalogVertexCache & cache);

/**
 * @brief Generate vertices for a specific time range (helper for cache population)
 * 
 * This is the core vertex generation logic extracted for use by the cache.
 * 
 * @param series The analog time series data
 * @param master_time_frame The master time frame for coordinate conversion
 * @param start_time Start of range to generate
 * @param end_time End of range to generate
 * @return Vector of CachedAnalogVertex ready for cache insertion
 */
std::vector<DataViewer::CachedAnalogVertex> generateVerticesForRange(
        AnalogTimeSeries const & series,
        std::shared_ptr<TimeFrame> const & master_time_frame,
        TimeFrameIndex start_time,
        TimeFrameIndex end_time);

}// namespace DataViewerHelpers

#endif// DATAVIEWER_WIDGET_SCENEBUILDINGHELPERS_HPP
