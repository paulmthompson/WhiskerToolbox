#ifndef MASKDATAVISUALIZATION_HPP
#define MASKDATAVISUALIZATION_HPP

#include "SpatialIndex/RTree.hpp"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>

class MaskData;
class QOpenGLShaderProgram;

/**
 * @brief Visualization data for a single MaskData object
 */
struct MaskDataVisualization : protected QOpenGLFunctions_4_1_Core  {
    std::unique_ptr<RTree<int64_t>> spatial_index;
    std::vector<float> vertex_data;
    QOpenGLBuffer vertex_buffer;
    QOpenGLVertexArrayObject vertex_array_object;
    QString key;
    QVector4D color;
    bool visible = true;

    // Selection state for this MaskData
    std::unordered_set<RTreeEntry<int64_t> const *> selected_masks;
    std::vector<float> selection_vertex_data;
    QOpenGLBuffer selection_vertex_buffer;
    QOpenGLVertexArrayObject selection_vertex_array_object;

    // Hover state for this MaskData
    RTreeEntry<int64_t> const * current_hover_mask = nullptr;

    MaskDataVisualization(QString const & data_key, std::shared_ptr<MaskData> const & mask_data);
    ~MaskDataVisualization();

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
     * @brief Clear all selected masks
     */
    void clearSelection();

    /**
     * @brief Toggle selection of a mask
     * @param mask_ptr Pointer to the mask to toggle
     * @return True if mask was selected, false if deselected
     */
    bool toggleMaskSelection(RTreeEntry<int64_t> const * mask_ptr);

    /**
     * @brief Render masks for this MaskData
     * 
     * Masks should appear as a homogenous solid shape with an outline of the same
     * color but darker.
     * 
     * @param shader_program The shader program to use for rendering
     */
    void renderMasks(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Render selected masks for this MaskData
     * 
     * Selected masks should appear as a homogenous solid shape with an outline of the same
     * color but darker. Selected masks should be a different color than the unselected masks.
     * 
     * @param shader_program The shader program to use for rendering
     */
    void renderSelectedMasks(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Render hover mask for this MaskData
     * 
     * Hover masks should appear as a homogenous solid shape with an outline of the same
     * color but darker.
     * 
     * @param shader_program The shader program to use for rendering
     * @param highlight_buffer The buffer to use for highlighting
     * @param highlight_vao The vertex array object to use for highlighting
     */
    void renderHoverMask(QOpenGLShaderProgram * shader_program, 
                          QOpenGLBuffer & highlight_buffer, 
                          QOpenGLVertexArrayObject & highlight_vao);

    /**
     * @brief Calculate bounding box for a MaskData object
     * @param mask_data The MaskData to calculate bounds for
     * @return BoundingBox for the MaskData
     */
    BoundingBox calculateBoundsForMaskData(MaskData const * mask_data) const;
};


#endif // MASKDATAVISUALIZATION_HPP