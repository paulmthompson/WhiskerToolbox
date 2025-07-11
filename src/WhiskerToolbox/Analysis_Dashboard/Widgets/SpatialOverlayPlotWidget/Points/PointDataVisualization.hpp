#ifndef POINTDATAVISUALIZATION_HPP
#define POINTDATAVISUALIZATION_HPP

#include "SpatialIndex/QuadTree.hpp"


#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

class QOpenGLShaderProgram;

/**
 * @brief Visualization data for a single PointData object
 */
struct PointDataVisualization : protected QOpenGLFunctions_4_1_Core  {
    std::unique_ptr<QuadTree<int64_t>> spatial_index;
    std::vector<float> vertex_data;
    QOpenGLBuffer vertex_buffer;
    QOpenGLVertexArrayObject vertex_array_object;
    QString key;
    QVector4D color;
    bool visible = true;

    // Selection state for this PointData
    std::unordered_set<QuadTreePoint<int64_t> const *> selected_points;
    std::vector<float> selection_vertex_data;
    QOpenGLBuffer selection_vertex_buffer;
    QOpenGLVertexArrayObject selection_vertex_array_object;

    // Hover state for this PointData
    QuadTreePoint<int64_t> const * current_hover_point = nullptr;

    PointDataVisualization(QString const & data_key);
    ~PointDataVisualization();

    /**
     * @brief Initialize OpenGL resources for this visualization
     */
    void initializeOpenGLResources();

    /**
     * @brief Clean up OpenGL resources for this visualization
     */
    void cleanupOpenGLResources();

    /**
     * @brief Update selection vertex buffer with current selection
     */
    void updateSelectionVertexBuffer();

    /**
     * @brief Clear all selected points
     */
    void clearSelection();

    /**
     * @brief Toggle selection of a point
     * @param point_ptr Pointer to the point to toggle
     * @return True if point was selected, false if deselected
     */
    bool togglePointSelection(QuadTreePoint<int64_t> const * point_ptr);

    /**
     * @brief Render points for this PointData
     */
    void renderPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render selected points for this PointData
     */
    void renderSelectedPoints(QOpenGLShaderProgram * shader_program, float point_size);

    /**
     * @brief Render hover point for this PointData
     */
    void renderHoverPoint(QOpenGLShaderProgram * shader_program, 
                          float point_size,
                          QOpenGLBuffer & highlight_buffer, 
                          QOpenGLVertexArrayObject & highlight_vao);
};


#endif // POINTDATAVISUALIZATION_HPP