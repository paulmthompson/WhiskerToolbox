#ifndef LINEDATAVISUALIZATION_HPP
#define LINEDATAVISUALIZATION_HPP

#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "LineIdentifier.hpp"
#include "../Selection/SelectionModes.hpp"

#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>
#include <QGenericMatrix>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QOpenGLShaderProgram;


/**
 * @brief Visualization data for a single LineData object
 */
struct LineDataVisualization : protected QOpenGLFunctions_4_1_Core {
    // Line data storage
    std::shared_ptr<LineData> m_line_data;
    std::vector<float> vertex_data;              // All line segments as pairs of vertices
    std::vector<uint32_t> line_id_data;          // Line ID for each vertex
    std::vector<uint32_t> line_offsets;          // Start index of each line in vertex_data (legacy)
    std::vector<uint32_t> line_lengths;          // Number of vertices in each line (legacy)
    std::vector<LineIdentifier> line_identifiers;// Mapping from line index to identifier
    
    // Vertex range tracking for efficient hover rendering
    struct LineVertexRange {
        uint32_t start_vertex;  // Starting vertex index
        uint32_t vertex_count;  // Number of vertices for this line
    };
    std::vector<LineVertexRange> line_vertex_ranges; // Vertex ranges for each line

    // OpenGL resources
    QOpenGLBuffer vertex_buffer;
    QOpenGLBuffer line_id_buffer;
    QOpenGLVertexArrayObject vertex_array_object;

    // Framebuffers
    QOpenGLFramebufferObject * scene_framebuffer;  // For caching the rendered scene
    QOpenGLFramebufferObject * picking_framebuffer;// For hover detection

    // Picking-specific GL resources
    QOpenGLBuffer picking_vertex_buffer;
    QOpenGLVertexArrayObject picking_vertex_array_object;

    // Fullscreen quad for blitting
    QOpenGLVertexArrayObject fullscreen_quad_vao;
    QOpenGLBuffer fullscreen_quad_vbo;

    // Shader programs
    QOpenGLShaderProgram * line_shader_program;
    QOpenGLShaderProgram * picking_shader_program;
    QOpenGLShaderProgram * blit_shader_program;

    // Visualization properties
    QString key;
    QVector4D color;
    bool visible = true;
    QVector2D canvas_size;  // Canvas size for coordinate normalization

    // Hover state for this LineData
    LineIdentifier current_hover_line;
    bool has_hover_line = false;
    uint32_t cached_hover_line_index = 0;     // Cached index to avoid linear search
    GLint cached_hover_uniform_location = -1; // Cached uniform location to avoid repeated queries

    // Selection state for this LineData (for future implementation)
    std::unordered_set<LineIdentifier> selected_lines;
    std::vector<float> selection_vertex_data;
    QOpenGLBuffer selection_vertex_buffer;
    QOpenGLVertexArrayObject selection_vertex_array_object;

    // Selection mode state
    SelectionMode _current_selection_mode;

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
    void renderLines(QOpenGLShaderProgram * shader_program, float line_width);

    /**
     * @brief Direct rendering fallback (original method)
     */
    void renderLinesDirect(QOpenGLShaderProgram * shader_program, float line_width);

    /**
     * @brief Render lines using internal shader program (convenience method)
     */
    void renderLines(float line_width);

    /**
     * @brief Render lines to picking framebuffer for hover detection
     */
    void renderLinesToPickingBuffer(float line_width);

    /**
     * @brief Render all lines to the scene framebuffer for caching.
     */
    void renderLinesToSceneBuffer(QOpenGLShaderProgram * shader_program, float line_width);

    /**
     * @brief Blit the cached scene framebuffer to the screen.
     */
    void blitSceneBuffer();

    /**
     * @brief Render just the currently hovered line.
     */
    void renderHoverLine(QOpenGLShaderProgram * shader_program, float line_width);

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

    /**
     * @brief Set the current selection mode for this visualization
     * @param mode The current selection mode
     */
    void setSelectionMode(SelectionMode mode);
};

#endif// LINEDATAVISUALIZATION_HPP