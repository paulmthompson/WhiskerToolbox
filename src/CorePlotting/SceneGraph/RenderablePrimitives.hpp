#ifndef COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
#define COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP

#include "../Interaction/GlyphPreview.hpp"

#include "Entity/EntityTypes.hpp"
#include "SpatialIndex/QuadTree.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace CorePlotting {

/**
 * @brief A batch of lines (e.g., LineData, Epochs, or segmented AnalogSeries)
 * 
 * Designed for efficient rendering via glMultiDrawArrays or Instancing.
 * The Transformer flattens source data into these vectors.
 */
struct RenderablePolyLineBatch {
    // Geometry: Flat array of vertices {x, y, x, y...}
    // Ready for direct VBO upload (or SSBO for Compute Shaders).
    // Coordinates are in World Space.
    std::vector<float> vertices;

    // Topology: Defines individual lines within the vertex buffer
    std::vector<int32_t> line_start_indices;
    std::vector<int32_t> line_vertex_counts;

    // Per-Line Attributes (Optional)
    // If size == line_count: Each line has a unique ID (e.g., Epochs).
    // If empty: All lines share 'global_entity_id' (e.g., Segmented AnalogSeries).
    std::vector<EntityId> entity_ids;
    EntityId global_entity_id{0};

    // Per-Line Colors (Optional)
    // If size == line_count: Each line has a unique color.
    // If empty: All lines use 'global_color'.
    std::vector<glm::vec4> colors;

    // Global Attributes
    float thickness{1.0f};
    glm::vec4 global_color{1.0f, 1.0f, 1.0f, 1.0f};

    // Model matrix for this batch (positions in world space)
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief A batch of glyphs (e.g., Events in a Raster Plot, Points)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableGlyphBatch {
    // Instance Data: {x, y} positions in World Space
    std::vector<glm::vec2> positions;

    // Per-Glyph Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;

    // Global Attributes
    enum class GlyphType {
        Circle,
        Square,
        Tick,
        Cross
    };
    GlyphType glyph_type{GlyphType::Circle};
    float size{5.0f};

    // Model matrix for this batch
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief A batch of rectangles (e.g., DigitalIntervalSeries)
 * 
 * Designed for Instanced Rendering.
 */
struct RenderableRectangleBatch {
    // Instance Data: {x, y, width, height} per rectangle in World Space
    std::vector<glm::vec4> bounds;

    // Per-Rectangle Attributes
    std::vector<glm::vec4> colors;
    std::vector<EntityId> entity_ids;

    // Per-Rectangle Selection State (parallel to entity_ids)
    // 0 = normal, 1 = selected. Used by renderer for visual highlighting.
    std::vector<uint8_t> selection_flags;

    // Model matrix for this batch
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief The complete description of a scene to be rendered.
 * 
 * This struct contains all the primitives and global state required to draw a frame.
 * It is produced by the CorePlotting layout engine and consumed by the Renderer.
 * 
 * ARCHITECTURE:
 * - Each batch has its own Model matrix (per-series positioning)
 * - View and Projection matrices are shared (global camera state)
 * - QuadTree lives here for synchronization (same layout as batches)
 * 
 * See DESIGN.md for the complete architecture discussion.
 */
struct RenderableScene {
    // Primitive batches (each has its own Model matrix)
    std::vector<RenderablePolyLineBatch> poly_line_batches;
    std::vector<RenderableRectangleBatch> rectangle_batches;
    std::vector<RenderableGlyphBatch> glyph_batches;

    // Shared transformation matrices (apply to all batches)
    glm::mat4 view_matrix{1.0f};      // Camera pan/zoom
    glm::mat4 projection_matrix{1.0f};// World → NDC mapping

    // Spatial index for hit testing
    // Built alongside geometry to ensure synchronization
    // Uses same world-space coordinates as Model matrices
    std::unique_ptr<QuadTree<EntityId>> spatial_index;

    // Selection state (queryable from the scene)
    // EntityIds of currently selected elements
    std::unordered_set<EntityId> selected_entities;

    /**
     * @brief Check if an entity is selected
     * @param id EntityId to check
     * @return true if the entity is in the selection set
     */
    [[nodiscard]] bool isSelected(EntityId id) const {
        return selected_entities.contains(id);
    }

    /**
     * @brief Get the selection set
     * @return Const reference to the selected entity set
     */
    [[nodiscard]] std::unordered_set<EntityId> const & getSelectedEntities() const {
        return selected_entities;
    }
};

}// namespace CorePlotting

#endif// COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
