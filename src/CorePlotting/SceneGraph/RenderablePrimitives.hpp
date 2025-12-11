#ifndef COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
#define COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP

#include "Entity/EntityTypes.hpp"
#include <cstdint>
#include <glm/glm.hpp>
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
};

/**
 * @brief The complete description of a scene to be rendered.
 * 
 * This struct contains all the primitives and global state required to draw a frame.
 * It is produced by the CorePlotting layout engine and consumed by the Renderer.
 */
struct RenderableScene {
    std::vector<RenderablePolyLineBatch> poly_line_batches;
    std::vector<RenderableRectangleBatch> rectangle_batches;
    std::vector<RenderableGlyphBatch> glyph_batches;

    // Global State
    glm::mat4 view_matrix{1.0f};
    glm::mat4 projection_matrix{1.0f};
};

}// namespace CorePlotting

#endif// COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
