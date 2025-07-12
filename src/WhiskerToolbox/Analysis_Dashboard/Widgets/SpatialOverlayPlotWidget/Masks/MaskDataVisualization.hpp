#ifndef MASKDATAVISUALIZATION_HPP
#define MASKDATAVISUALIZATION_HPP

#include "SpatialIndex/RTree.hpp"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

class MaskData;
class QOpenGLShaderProgram;

/**
 * @brief Identifier for a specific mask within the dataset
 */
struct MaskIdentifier {
    int64_t timeframe;
    size_t mask_index;

    MaskIdentifier() : timeframe(0), mask_index(0) {}
    
    MaskIdentifier(int64_t t, size_t idx) : timeframe(t), mask_index(idx) {}
    
    bool operator==(const MaskIdentifier& other) const {
        return timeframe == other.timeframe && mask_index == other.mask_index;
    }
};

/**
 * @brief Visualization data for a single MaskData object
 */
struct MaskDataVisualization : protected QOpenGLFunctions_4_1_Core {
    // R-tree for spatial indexing of mask bounding boxes
    std::unique_ptr<RTree<MaskIdentifier>> spatial_index;
    
    // Binary image texture data and OpenGL objects
    std::vector<float> binary_image_data;
    GLuint binary_image_texture = 0;
    QOpenGLBuffer quad_vertex_buffer;
    QOpenGLVertexArrayObject quad_vertex_array_object;
    
    // Mask outline rendering data
    std::vector<float> outline_vertex_data;
    QOpenGLBuffer outline_vertex_buffer;
    QOpenGLVertexArrayObject outline_vertex_array_object;
    
    // Selection and hover data
    std::vector<RTreeEntry<MaskIdentifier>> selected_masks;
    std::vector<float> selection_outline_data;
    QOpenGLBuffer selection_outline_buffer;
    QOpenGLVertexArrayObject selection_outline_array_object;
    
    // Hover state
    std::vector<RTreeEntry<MaskIdentifier>> current_hover_entries;
    std::vector<float> hover_outline_data;
    QOpenGLBuffer hover_outline_buffer;
    QOpenGLVertexArrayObject hover_outline_array_object;
    
    // Hover bounding box rendering
    std::vector<float> hover_bbox_data;
    QOpenGLBuffer hover_bbox_buffer;
    QOpenGLVertexArrayObject hover_bbox_array_object;
    
    // Visualization properties
    QString key;
    QVector4D color;
    bool visible = true;
    float outline_thickness = 2.0f;
    
    // World bounds based on image size
    float world_min_x = 0.0f;
    float world_max_x = 1.0f;
    float world_min_y = 0.0f;
    float world_max_y = 1.0f;
    
    // Reference to original data (not owned)
    std::shared_ptr<MaskData> mask_data;

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
     * @brief Set outline thickness for mask rendering
     * @param thickness Thickness in pixels
     */
    void setOutlineThickness(float thickness);

    /**
     * @brief Get current outline thickness
     * @return Thickness in pixels
     */
    float getOutlineThickness() const { return outline_thickness; }

    /**
     * @brief Update selection outline buffer with current selection
     */
    void updateSelectionOutlineBuffer();

    /**
     * @brief Clear all selected masks
     */
    void clearSelection();

    /**
     * @brief Toggle selection of a mask
     * @param mask_id Identifier of the mask to toggle
     * @return True if mask was selected, false if deselected
     */
    bool toggleMaskSelection(MaskIdentifier const & mask_id);

    /**
     * @brief Set hover masks using R-tree entries directly
     * @param entries Vector of R-tree entries containing masks and their bounding boxes
     */
    void setHoverEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries);

    /**
     * @brief Clear hover state
     */
    void clearHover();

    /**
     * @brief Find all masks that contain the given point
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Vector of mask identifiers that contain the point
     */
    std::vector<MaskIdentifier> findMasksContainingPoint(float world_x, float world_y) const;

    /**
     * @brief Render the binary image texture
     * @param shader_program The shader program to use for rendering
     */
    void renderBinaryImage(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Render hover mask bounding boxes
     * @param shader_program The shader program to use for rendering
     */
    void renderHoverMaskBoundingBoxes(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Calculate bounding box for the entire MaskData
     * @return BoundingBox encompassing all masks
     */
    BoundingBox calculateBounds() const;

    /**
     * @brief Update hover bounding box buffer with current hover masks
     */
    void updateHoverBoundingBoxBuffer();

private:
    /**
     * @brief Create the binary image texture from all masks
     */
    void createBinaryImageTexture();

    /**
     * @brief Populate the R-tree with mask bounding boxes
     */
    void populateRTree();

    /**
     * @brief Generate outline vertex data for all masks
     */
    void generateOutlineVertexData();

    /**
     * @brief Generate bounding box line data from R-tree entries
     * @param entries Vector of R-tree entries containing masks and their bounding boxes
     * @return Vector of vertex data for the bounding box lines
     */
    std::vector<float> generateBoundingBoxDataFromEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries) const;

    /**
     * @brief Check if a mask contains a world coordinate point
     * 
     * This is an exact method that searches through all of the points in the mask
     * to see if the point is there. Compare to fast rTree indexing that only
     * considers the bounding box
     * 
     * @param mask_id Identifier of the mask to check
     * @param pixel_x Pixel X coordinate
     * @param pixel_y Pixel Y coordinate
     * @return True if the mask contains the point
     */
    bool maskContainsPoint(MaskIdentifier const & mask_id, uint32_t pixel_x, uint32_t pixel_y) const;

    /**
     * @brief Convert world coordinates to texture coordinates
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Pair of texture coordinates (u, v)
     */
    std::pair<float, float> worldToTexture(float world_x, float world_y) const;
};

#endif // MASKDATAVISUALIZATION_HPP