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
    std::vector<float> vertex_data;              // All line vertices flattened
    std::vector<uint32_t> line_offsets;          // Start index of each line in vertex_data
    std::vector<uint32_t> line_lengths;          // Number of vertices in each line
    std::vector<LineIdentifier> line_identifiers;// Mapping from line index to identifier

    // OpenGL resources
    QOpenGLBuffer vertex_buffer;
    QOpenGLVertexArrayObject vertex_array_object;

    // Picking framebuffer for hover detection
    QOpenGLFramebufferObject * picking_framebuffer;
    QOpenGLBuffer picking_vertex_buffer;
    QOpenGLVertexArrayObject picking_vertex_array_object;

    // Shader programs
    QOpenGLShaderProgram * line_shader_program;
    QOpenGLShaderProgram * picking_shader_program;

    // Visualization properties
    QString key;
    QVector4D color;
    bool visible = true;

    // Hover state for this LineData
    LineIdentifier current_hover_line;
    bool has_hover_line = false;

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
     * @brief Render lines for this LineData
     */
    void renderLines(QOpenGLShaderProgram * shader_program, float line_width);

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