#ifndef LINEDATAVISUALIZATION_HPP
#define LINEDATAVISUALIZATION_HPP

#include "CoreGeometry/boundingbox.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "LineIdentifier.hpp"

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

    // Picking framebuffer for hover detection
    QOpenGLFramebufferObject * picking_framebuffer;
    QOpenGLBuffer picking_vertex_buffer;
    QOpenGLVertexArrayObject picking_vertex_array_object;

    // Cached rendering for performance optimization
    QOpenGLFramebufferObject * static_lines_cache;     // Cache of all 300k lines (rendered once)
    QOpenGLFramebufferObject * dynamic_hover_buffer;   // Buffer for hover line (rendered each frame)
    bool static_cache_valid = false;                   // Whether cache needs regeneration
    float cached_line_width = -1.0f;                   // Line width used for cache (invalidate if changed)

    // Cached rendering framebuffer - render all lines once, reuse every frame
    QOpenGLFramebufferObject * cached_lines_framebuffer;
    bool cached_lines_valid = false;  // Whether the cached render is up to date

    // Shader programs
    QOpenGLShaderProgram * line_shader_program;
    QOpenGLShaderProgram * picking_shader_program;

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

    void buildVertexData(std::shared_ptr<LineData> const & line_data);

    /**
     * @brief Update picking framebuffer with current line data
     */
    void updatePickingFramebuffer();

    /**
     * @brief Render all static lines to cache (called once or when cache is invalidated)
     */
    void renderStaticLinesToCache(float line_width);

    /**
     * @brief Render hover line to dynamic buffer
     */
    void renderHoverLineToDynamicBuffer(float line_width);

    /**
     * @brief Composite cached static lines + dynamic hover line to screen
     * @param view_transform 3x3 matrix for zoom/pan transformation
     */
    void compositeCachedLines(QOpenGLShaderProgram * composite_shader, 
                             const QMatrix3x3& view_transform = QMatrix3x3());

    /**
     * @brief Invalidate static cache (call when line data changes)
     */
    void invalidateStaticCache();

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
     * @brief Get line identifier at screen position
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @return Line identifier if found, empty optional otherwise
     */
    std::optional<LineIdentifier> getLineAtScreenPosition(int screen_x, int screen_y);

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
};

#endif// LINEDATAVISUALIZATION_HPP