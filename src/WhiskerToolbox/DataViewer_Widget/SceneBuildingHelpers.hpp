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

}// namespace DataViewerHelpers

#endif// DATAVIEWER_WIDGET_SCENEBUILDINGHELPERS_HPP
