#ifndef LINEDATAVISUALIZATION_HPP
#define LINEDATAVISUALIZATION_HPP

#include "../Selection/SelectionModes.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "LineIdentifier.hpp"
#include "../Selection/SelectionHandlers.hpp"
#include "../RenderingContext.hpp"

#include <QGenericMatrix>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>
#include <QMatrix4x4>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>
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
    std::shared_ptr<LineData> m_line_data;
    std::vector<float> vertex_data;              // All line segments as pairs of vertices
    std::vector<uint32_t> line_id_data;          // Line ID for each vertex
    std::vector<uint32_t> line_offsets;          // Start index of each line in vertex_data (legacy)
    std::vector<uint32_t> line_lengths;          // Number of vertices in each line (legacy)
    std::vector<LineIdentifier> line_identifiers;// Mapping from line index to identifier

    // Vertex range tracking for efficient hover rendering
    struct LineVertexRange {
        uint32_t start_vertex;// Starting vertex index
        uint32_t vertex_count;// Number of vertices for this line
    };
    std::vector<LineVertexRange> line_vertex_ranges;// Vertex ranges for each line

    // OpenGL resources
    QOpenGLBuffer vertex_buffer;
    QOpenGLBuffer line_id_buffer;
    QOpenGLVertexArrayObject vertex_array_object;

    // Framebuffers
    QOpenGLFramebufferObject * scene_framebuffer;  // For caching the rendered scene

    // Compute shader resources for line intersection
    QOpenGLBuffer line_segments_buffer;    // Buffer containing all line segments
    QOpenGLBuffer intersection_results_buffer; // Buffer for storing intersection results
    QOpenGLBuffer intersection_count_buffer;   // Buffer for storing result count
    std::vector<float> segments_data;      // CPU copy of line segments for compute shader

    // Fullscreen quad for blitting
    QOpenGLVertexArrayObject fullscreen_quad_vao;
    QOpenGLBuffer fullscreen_quad_vbo;

    // Shader programs
    QOpenGLShaderProgram * line_shader_program;
    QOpenGLShaderProgram * blit_shader_program;
    QOpenGLShaderProgram * line_intersection_compute_shader;

    // Visualization properties
    QString key;
    QVector4D color;
    bool visible = true;
    QVector2D canvas_size;// Canvas size for coordinate normalization

    // Hover state for this LineData
    LineIdentifier current_hover_line;
    bool has_hover_line = false;
    uint32_t cached_hover_line_index = 0;    // Cached index to avoid linear search
    GLint cached_hover_uniform_location = -1;// Cached uniform location to avoid repeated queries

    // Selection state for this LineData (for future implementation)
    std::unordered_set<LineIdentifier> selected_lines;
    std::vector<float> selection_vertex_data;
    QOpenGLBuffer selection_vertex_buffer;
    QOpenGLVertexArrayObject selection_vertex_array_object;

    bool m_viewIsDirty = true;
    bool m_dataIsDirty = true;

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
     * @param line_data The LineData to build vertex data from
     */
    void buildVertexData(LineData const * line_data);

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
            const QMatrix4x4& mvp_matrix, float line_width);

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

    void applySelection(SelectionVariant & selection_handler, RenderingContext const& context);

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
    void applySelection(LineSelectionHandler const & selection_handler, RenderingContext const& context);

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
    bool handleHover(const QPoint & screen_pos, const QSize & widget_size, const QMatrix4x4& mvp_matrix);

private:
    void renderLinesToSceneBuffer(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width);
    void blitSceneBuffer();
    void renderHoverLine(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width);
    void renderSelection(QMatrix4x4 const & mvp_matrix, float line_width);
    void initializeComputeShaderResources();
    void cleanupComputeShaderResources();
    void updateLineSegmentsBuffer();
};

#endif// LINEDATAVISUALIZATION_HPP