#ifndef LINEDATAVISUALIZATION_HPP
#define LINEDATAVISUALIZATION_HPP

#include "Widgets/SpatialOverlayPlotWidget/RenderingContext.hpp"
#include "Widgets/SpatialOverlayPlotWidget/Selection/SelectionHandlers.hpp"
#include "Widgets/SpatialOverlayPlotWidget/Selection/SelectionModes.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "LineIdentifier.hpp"

#include <QGenericMatrix>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QOpenGLShaderProgram;
class PolygonSelectionHandler;
class LineSelectionHandler;
class NoneSelectionHandler;


/**
 * @brief Visualization data for a single LineData object
 */
struct LineDataVisualization : protected QOpenGLFunctions_4_3_Core {
    // Line data storage
    std::shared_ptr<LineData> m_line_data_ptr;

    std::vector<float> m_vertex_data;              // All line segments as pairs of vertices
    std::vector<uint32_t> m_line_id_data;          // Line ID for each vertex
    std::vector<LineIdentifier> m_line_identifiers;// Mapping from line index to identifier

    // Vertex range tracking for efficient hover rendering
    struct LineVertexRange {
        uint32_t start_vertex;// Starting vertex index
        uint32_t vertex_count;// Number of vertices for this line
    };
    std::vector<LineVertexRange> m_line_vertex_ranges;// Vertex ranges for each line

    // OpenGL resources
    QOpenGLBuffer m_vertex_buffer;
    QOpenGLBuffer m_line_id_buffer;
    QOpenGLVertexArrayObject m_vertex_array_object;

    // Framebuffers
    QOpenGLFramebufferObject * m_scene_framebuffer;// For caching the rendered scene

    // Compute shader resources for line intersection
    QOpenGLBuffer m_line_segments_buffer;       // Buffer containing all line segments
    QOpenGLBuffer m_intersection_results_buffer;// Buffer for storing intersection results
    QOpenGLBuffer m_intersection_count_buffer;  // Buffer for storing result count
    std::vector<float> m_segments_data;         // CPU copy of line segments for compute shader

    // Fullscreen quad for blitting
    QOpenGLVertexArrayObject m_fullscreen_quad_vao;
    QOpenGLBuffer m_fullscreen_quad_vbo;

    // Shader programs
    QOpenGLShaderProgram * m_line_shader_program;
    QOpenGLShaderProgram * m_blit_shader_program;
    QOpenGLShaderProgram * m_line_intersection_compute_shader;

    // Visualization properties
    QString m_key;
    QVector4D m_color;
    bool m_visible = true;
    QVector2D m_canvas_size;// Canvas size for coordinate normalization

    // Hover state for this LineData
    LineIdentifier m_current_hover_line;
    bool m_has_hover_line = false;
    uint32_t m_cached_hover_line_index = 0;    // Cached index to avoid linear search
    GLint m_cached_hover_uniform_location = -1;// Cached uniform location to avoid repeated queries

    // GPU-based selection using mask buffer
    std::unordered_set<LineIdentifier> m_selected_lines;
    QOpenGLBuffer m_selection_mask_buffer;                        // Buffer containing selection mask for each line
    std::vector<uint32_t> m_selection_mask;                       // CPU copy of selection mask
    std::unordered_map<LineIdentifier, size_t> m_line_id_to_index;// Fast lookup from LineIdentifier to index

    // Visibility system for individual lines
    QOpenGLBuffer m_visibility_mask_buffer;           // Buffer containing visibility mask for each line
    std::vector<uint32_t> m_visibility_mask;          // CPU copy of visibility mask (1 = visible, 0 = hidden)
    std::unordered_set<LineIdentifier> m_hidden_lines;// Set of hidden line identifiers

    // Time range filtering
    int m_time_range_start = 0;
    int m_time_range_end = 999999;
    bool m_time_range_enabled = false;

    // Statistics tracking
    size_t m_total_line_count = 0;
    size_t m_hidden_line_count = 0;

    bool m_viewIsDirty = true;
    bool m_dataIsDirty = true;
    QMatrix4x4 m_cachedMvpMatrix;// Cached MVP matrix to detect view changes

    LineDataVisualization(QString const & data_key, std::shared_ptr<LineData> const & line_data);
    ~LineDataVisualization();

    /**
     * @brief Initialize OpenGL resources for this visualization
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources for this visualization
     */
    void cleanupOpenGLResources();

    /**
     * @brief Build vertex data from LineData object. This is called when the line data is changed.
     */
    void buildVertexData();

    /**
     * @brief Update picking framebuffer with current line data
     */
    void updatePickingFramebuffer();

    /**
     * @brief Update OpenGL buffers with current vertex data
     */
    void updateOpenGLBuffers();


    /**
     * @brief Render lines for this LineData
     */
    void render(QMatrix4x4 const & mvp_matrix, float line_width);

    /**
     * @brief Get line identifier at screen position
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param widget_width The width of the viewport/widget
     * @param widget_height The height of the viewport/widget
     * @return Line identifier if found, empty optional otherwise
     */
    std::optional<LineIdentifier> getLineAtScreenPosition(
            int screen_x, int screen_y, int widget_width, int widget_height);

    /**
     * @brief Get all line identifiers intersecting a line segment on screen
     * @param start_x Screen X coordinate of line start
     * @param start_y Screen Y coordinate of line start
     * @param end_x Screen X coordinate of line end
     * @param end_y Screen Y coordinate of line end
     * @param widget_width The width of the viewport/widget
     * @param widget_height The height of the viewport/widget
     * @param mvp_matrix The model-view-projection matrix for coordinate transformation
     * @param line_width The line width for intersection tolerance
     * @return Vector of all intersecting line identifiers
     */
    std::vector<LineIdentifier> getAllLinesIntersectingLine(
            int start_x, int start_y, int end_x, int end_y,
            int widget_width, int widget_height,
            QMatrix4x4 const & mvp_matrix, float line_width);

    /**
     * @brief Set hover line
     * @param line_id The line identifier to hover, or empty for no hover
     */
    void setHoverLine(std::optional<LineIdentifier> line_id);

    /**
     * @brief Get current hover line
     */
    std::optional<LineIdentifier> getHoverLine() const;

    /**
     * @brief Calculate bounding box for a LineData object
     * @param line_data The LineData to calculate bounds for
     * @return BoundingBox for the LineData
     */
    BoundingBox calculateBoundsForLineData(LineData const * line_data) const;


    void clearSelection();

    //========== Selection Handlers ==========

    void applySelection(SelectionVariant & selection_handler, RenderingContext const & context);

    /**
     * @brief Apply selection to this LineDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(PolygonSelectionHandler const & selection_handler);

    /**
     * @brief Apply selection to this LineDataVisualization
     * @param selection_handler The LineSelectionHandler to apply
     * @param context The rendering context
     */
    void applySelection(LineSelectionHandler const & selection_handler, RenderingContext const & context);

    /**
     * @brief Get tooltip text for the current hover state
     * @return QString with tooltip information, or empty if no hover
     */
    QString getTooltipText() const;

    /**
     * @brief Handle hover events for this visualization
     * @param screen_pos The mouse position in screen coordinates
     * @param widget_size The size of the widget in pixels
     * @param mvp_matrix The model-view-projection matrix for coordinate transformation
     * @return True if the hover state changed, false otherwise
     */
    bool handleHover(QPoint const & screen_pos, QSize const & widget_size, QMatrix4x4 const & mvp_matrix);

    //========== Visibility Management ==========

    /**
     * @brief Hide selected lines from view
     * @return Number of lines that were hidden
     */
    size_t hideSelectedLines();

    /**
     * @brief Show all hidden lines in this visualization
     * @return Number of lines that were shown
     */
    size_t showAllLines();

    /**
     * @brief Get visibility statistics
     * @return Pair of (total_lines, hidden_lines)
     */
    std::pair<size_t, size_t> getVisibilityStats() const;

    //========== Time Range Filtering ==========

    /**
     * @brief Set time range filter for line visibility
     * @param start_frame Start frame (inclusive)
     * @param end_frame End frame (inclusive)
     */
    void setTimeRange(int start_frame, int end_frame);

    /**
     * @brief Enable or disable time range filtering
     * @param enabled Whether time range filtering should be active
     */
    void setTimeRangeEnabled(bool enabled);

    /**
     * @brief Get current time range settings
     * @return Tuple of (start_frame, end_frame, enabled)
     */
    std::tuple<int, int, bool> getTimeRange() const;

private:
    void _renderLinesToSceneBuffer(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width);
    void _blitSceneBuffer();
    void _renderHoverLine(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width);
    void _renderSelection(QMatrix4x4 const & mvp_matrix, float line_width);
    void _initializeComputeShaderResources();
    void _cleanupComputeShaderResources();
    void _updateLineSegmentsBuffer();
    void _updateSelectionMask();
    void _updateVisibilityMask();
};

#endif// LINEDATAVISUALIZATION_HPP