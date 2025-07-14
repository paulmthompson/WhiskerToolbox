#ifndef MASKDATAVISUALIZATION_HPP
#define MASKDATAVISUALIZATION_HPP

#include "SpatialIndex/RTree.hpp"
#include "CoreGeometry/polygon.hpp"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <set>
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

    bool operator<(const MaskIdentifier& other) const {
        return (timeframe < other.timeframe) || 
               (timeframe == other.timeframe && mask_index < other.mask_index);
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
    

    
    // Selection and hover data
    std::set<MaskIdentifier> selected_masks;
    std::vector<float> selection_binary_image_data;
    GLuint selection_binary_image_texture = 0;
    
    // Hover state
    std::vector<RTreeEntry<MaskIdentifier>> current_hover_entries;
    
    // Hover union polygon rendering
    Polygon hover_union_polygon;
    std::vector<float> hover_polygon_data;
    QOpenGLBuffer hover_polygon_buffer;
    QOpenGLVertexArrayObject hover_polygon_array_object;
    
    // Visualization properties
    QString key;
    QVector4D color;
    bool visible = true;
    
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
     * @brief Clear all selected masks
     */
    void clearSelection();

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
     * @brief Refine R-tree entries to only those that contain the given point using precise pixel checking
     * @param entries Vector of R-tree entries to refine
     * @param world_x World X coordinate
     * @param world_y World Y coordinate
     * @return Vector of mask identifiers from entries that contain the point
     */
    std::vector<MaskIdentifier> refineMasksContainingPoint(std::vector<RTreeEntry<MaskIdentifier>> const & entries, float world_x, float world_y) const;

    /**
     * @brief Render the binary image texture
     * @param shader_program The shader program to use for rendering
     */
    void renderBinaryImage(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Render hover mask union polygon
     * @param shader_program The shader program to use for rendering
     */
    void renderHoverMaskUnionPolygon(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Render selected masks as a binary image with different color/opacity
     * @param shader_program The shader program to use for rendering
     */
    void renderSelectedMasks(QOpenGLShaderProgram * shader_program);

    /**
     * @brief Calculate bounding box for the entire MaskData
     * @return BoundingBox encompassing all masks
     */
    BoundingBox calculateBounds() const;

    /**
     * @brief Update hover union polygon from current hover entries
     */
    void updateHoverUnionPolygon();

    /**
     * @brief Select multiple masks at once
     * @param mask_ids Vector of mask identifiers to select
     */
    void selectMasks(std::vector<MaskIdentifier> const & mask_ids);

    /**
     * @brief Toggle selection state of a single mask
     * @param mask_id Identifier of the mask to toggle
     * @return True if mask was selected, false if deselected
     */
    bool toggleMaskSelection(MaskIdentifier const & mask_id);

    /**
     * @brief Remove a specific mask from selection if it's currently selected
     * @param mask_id Identifier of the mask to remove from selection
     * @return True if mask was removed from selection, false if it wasn't selected
     */
    bool removeMaskFromSelection(MaskIdentifier const & mask_id);

    /**
     * @brief Remove multiple masks from selection (intersection removal)
     * @param mask_ids Vector of mask identifiers to potentially remove
     * @return Number of masks actually removed from the current selection
     */
    size_t removeIntersectingMasks(std::vector<MaskIdentifier> const & mask_ids);

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
     * @brief Update the selection binary image texture based on currently selected masks
     */
    void updateSelectionBinaryImageTexture();

    /**
     * @brief Compute the union polygon from R-tree entries
     * @param entries Vector of R-tree entries containing masks and their bounding boxes
     * @return Union polygon encompassing all bounding boxes
     */
    Polygon computeUnionPolygonFromEntries(std::vector<RTreeEntry<MaskIdentifier>> const & entries) const;

    /**
     * @brief Generate polygon vertex data for OpenGL rendering
     * @param polygon The polygon to convert to vertex data
     * @return Vector of vertex data for line strip rendering
     */
    std::vector<float> generatePolygonVertexData(Polygon const & polygon) const;

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

private:

    /**
     * @brief Check if two bounding boxes can be unioned using simple rectangular union
     * @param bbox1 First bounding box
     * @param bbox2 Second bounding box
     * @return True if they can be unioned as a simple rectangle
     */
    bool canUseSimpleRectangularUnion(BoundingBox const & bbox1, BoundingBox const & bbox2) const;

    /**
     * @brief Get the simple rectangular union of two bounding boxes
     * @param bbox1 First bounding box
     * @param bbox2 Second bounding box
     * @return Union bounding box
     */
    BoundingBox getSimpleRectangularUnion(BoundingBox const & bbox1, BoundingBox const & bbox2) const;

    /**
     * @brief Get the overall bounding box that encompasses all input bounding boxes
     * @param boxes Vector of bounding boxes
     * @return Overall bounding box
     */
    BoundingBox getOverallBoundingBox(std::vector<BoundingBox> const & boxes) const;

    /**
     * @brief Check if all bounding boxes form a dense cluster that can be represented as a single rectangle
     * @param boxes Vector of bounding boxes to check
     * @return True if they can be efficiently represented as a single rectangle
     */
    bool areAllBoxesRectangularlyUnifiable(std::vector<BoundingBox> const & boxes) const;

    
};

static Polygon computeUnionPolygonUsingContainment(std::vector<RTreeEntry<MaskIdentifier>> const & entries);
   

#endif // MASKDATAVISUALIZATION_HPP