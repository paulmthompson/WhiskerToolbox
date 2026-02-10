#ifndef COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
#define COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP

#include "../Interaction/DataCoordinates.hpp"
#include "../Interaction/GlyphPreview.hpp"
#include "../Layout/LayoutTransform.hpp"

#include "Entity/EntityTypes.hpp"
#include "SpatialIndex/QuadTree.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <optional>
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

    // Mapping from EntityId to series_key for hit test result enrichment
    // Populated by SceneBuilder::addGlyphs/addRectangles during scene construction
    std::unordered_map<EntityId, std::string> entity_to_series_key;

    // Selection state (queryable from the scene)
    // EntityIds of currently selected elements
    std::unordered_set<EntityId> selected_entities;

    // Active preview for interactive glyph creation/modification
    // Set by the widget during interactions, rendered on top of main scene
    std::optional<Interaction::GlyphPreview> active_preview;

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

    // ========================================================================
    // Coordinate Conversion (Canvas → World → Data)
    // ========================================================================

    /**
     * @brief Convert canvas pixel coordinates to world coordinates
     * 
     * Uses the scene's stored view and projection matrices to unproject
     * from canvas space to world space.
     * 
     * @param canvas_x X coordinate in pixels (0 = left edge)
     * @param canvas_y Y coordinate in pixels (0 = top edge, Y increases downward)
     * @param viewport_width Viewport width in pixels
     * @param viewport_height Viewport height in pixels
     * @return World-space coordinates (X = time, Y = normalized plot position)
     */
    [[nodiscard]] glm::vec2 canvasToWorld(
        float canvas_x, float canvas_y,
        int viewport_width, int viewport_height) const;

    /**
     * @brief Convert a GlyphPreview to DataCoordinates for committing to DataManager
     * 
     * This is the main conversion method for interactive operations. It takes
     * the preview geometry (in canvas coordinates) and converts it to data
     * coordinates suitable for updating a data series.
     * 
     * @param preview The preview geometry in canvas coordinates
     * @param viewport_width Current viewport width in pixels
     * @param viewport_height Current viewport height in pixels
     * @param y_transform The Y-axis LayoutTransform for the target series.
     *                    This is obtained from the LayoutResponse for the series.
     * @param series_key Key of the target series in DataManager
     * @param entity_id Optional EntityId if modifying an existing element
     * @return DataCoordinates ready for committing to DataManager
     * 
     * **Usage Example**:
     * @code
     *   // After interaction completes:
     *   auto preview = controller.getPreview();
     *   auto y_transform = layout_response.getSeriesTransform(series_key);
     *   auto data_coords = scene.previewToDataCoords(
     *       preview, width(), height(), y_transform, series_key, entity_id);
     *   commitToDataManager(data_coords);
     * @endcode
     */
    [[nodiscard]] Interaction::DataCoordinates previewToDataCoords(
        Interaction::GlyphPreview const& preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform,
        std::string const& series_key,
        std::optional<EntityId> entity_id = std::nullopt) const;

    /**
     * @brief Convert rectangle preview to interval coordinates
     * 
     * Specialized conversion for interval creation/modification.
     * Only uses the X coordinates (time), ignoring Y (height spans full range).
     * 
     * @param rect_preview A GlyphPreview of Type::Rectangle
     * @param viewport_width Viewport width in pixels
     * @param viewport_height Viewport height in pixels
     * @return IntervalCoords with start and end time indices
     */
    [[nodiscard]] Interaction::DataCoordinates::IntervalCoords previewToIntervalCoords(
        Interaction::GlyphPreview const& rect_preview,
        int viewport_width, int viewport_height) const;

    /**
     * @brief Convert line preview to line coordinates
     * 
     * Converts both endpoints from canvas to world coordinates.
     * Y values are optionally transformed using the provided layout transform.
     * 
     * @param line_preview A GlyphPreview of Type::Line
     * @param viewport_width Viewport width in pixels
     * @param viewport_height Viewport height in pixels
     * @param y_transform Y-axis transform (use identity for raw world coords)
     * @return LineCoords with start and end points in data space
     */
    [[nodiscard]] Interaction::DataCoordinates::LineCoords previewToLineCoords(
        Interaction::GlyphPreview const& line_preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform) const;

    /**
     * @brief Convert point preview to point coordinates
     * 
     * @param point_preview A GlyphPreview of Type::Point
     * @param viewport_width Viewport width in pixels
     * @param viewport_height Viewport height in pixels
     * @param y_transform Y-axis transform
     * @return PointCoords in data space
     */
    [[nodiscard]] Interaction::DataCoordinates::PointCoords previewToPointCoords(
        Interaction::GlyphPreview const& point_preview,
        int viewport_width, int viewport_height,
        LayoutTransform const& y_transform) const;
};

}// namespace CorePlotting

#endif// COREPLOTTING_SCENEGRAPH_RENDERABLEPRIMITIVES_HPP
