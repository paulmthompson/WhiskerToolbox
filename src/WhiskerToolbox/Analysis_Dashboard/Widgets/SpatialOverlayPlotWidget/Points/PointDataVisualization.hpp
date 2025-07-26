#ifndef POINTDATAVISUALIZATION_HPP
#define POINTDATAVISUALIZATION_HPP

#include "SpatialIndex/QuadTree.hpp"
#include "../Selection/SelectionHandlers.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>
#include <variant>

class PointData;
class QOpenGLShaderProgram;
class PolygonSelectionHandler;
class LineSelectionHandler;
class NoneSelectionHandler;


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
    QOpenGLBuffer _highlight_vertex_buffer;
    QOpenGLVertexArrayObject _highlight_vertex_array_object;

    PointDataVisualization(QString const & data_key, std::shared_ptr<PointData> const & point_data);
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
     * @brief Remove a specific point from selection if it's currently selected
     * @param point_ptr Pointer to the point to remove from selection
     * @return True if point was removed from selection, false if it wasn't selected
     */
    bool removePointFromSelection(QuadTreePoint<int64_t> const * point_ptr);

    /**
     * @brief Render all points, selections, and highlights for this visualization
     * @param mvp_matrix The model-view-projection matrix
     * @param point_size The size of the points to render
     */
    void render(QMatrix4x4 const & mvp_matrix, float point_size);

    /**
     * @brief Calculate bounding box for a PointData object
     * @param point_data The PointData to calculate bounds for
     * @return BoundingBox for the PointData
     */
    BoundingBox calculateBoundsForPointData(PointData const * point_data) const;

    //========== Selection Handlers ==========

    /**
     * @brief Apply selection to this PointDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(SelectionVariant & selection_handler);

    /**
     * @brief Apply selection to this PointDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(PolygonSelectionHandler const & selection_handler);

private:
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
    void renderHoverPoint(QOpenGLShaderProgram * shader_program, float point_size);
};


#endif // POINTDATAVISUALIZATION_HPP