#ifndef MASKDATAVISUALIZATION_HPP
#define MASKDATAVISUALIZATION_HPP

#include "Widgets/SpatialOverlayPlotWidget/Selection/SelectionHandlers.hpp"
#include "CoreGeometry/polygon.hpp"
#include "MaskIdentifier.hpp"
#include "SpatialIndex/RTree.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVertexArrayObject>
#include <QString>
#include <QVector2D>
#include <QVector4D>

#include <cstdint>
#include <memory>
#include <set>
#include <unordered_set>
#include <variant>
#include <vector>

class MaskData;
class QOpenGLShaderProgram;
class PolygonSelectionHandler;
class NoneSelectionHandler;
class LineSelectionHandler;
class PointSelectionHandler;


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
     * @brief Render all masks, selections, and hover highlights for this visualization
     * @param mvp_matrix The model-view-projection matrix
     */
    void render(QMatrix4x4 const & mvp_matrix);


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


    //========== Selection Handlers ==========

    /**
     * @brief Apply selection to this MaskDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(SelectionVariant & selection_handler);

    /**
     * @brief Apply selection to this MaskDataVisualization
     * @param selection_handler The PolygonSelectionHandler to apply
     */
    void applySelection(PolygonSelectionHandler const & selection_handler);

    /**
     * @brief Apply selection to this MaskDataVisualization
     * @param selection_handler The PointSelectionHandler to apply
     */
    void applySelection(PointSelectionHandler const & selection_handler);

    /**
     * @brief Get tooltip text for the current hover state
     * @return QString with tooltip information, or empty if no hover
     */
    QString getTooltipText() const;

    /**
     * @brief Handle hover events for this visualization
     * @param world_pos The mouse position in world coordinates
     * @return True if the hover state changed, false otherwise
     */
    bool handleHover(QVector2D const & world_pos);

private:
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
    std::vector<MaskIdentifier> refineMasksContainingPoint(
            std::vector<RTreeEntry<MaskIdentifier>> const & entries,
            float world_x,
            float world_y) const;

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

    /**
     * @brief Flip Y coordinate to account for different coordinate systems
     * @param y Original Y coordinate
     * @return Flipped Y coordinate
     */
    float flipY(float y) const {
        return world_max_y - y;
    }
};

static Polygon computeUnionPolygonUsingContainment(std::vector<RTreeEntry<MaskIdentifier>> const & entries);


#endif// MASKDATAVISUALIZATION_HPP