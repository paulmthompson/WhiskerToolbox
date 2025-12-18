#ifndef COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
#define COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP

#include "CoordinateTransform/ViewState.hpp"
#include "Layout/LayoutEngine.hpp"
#include "Mappers/MapperConcepts.hpp"
#include "Mappers/MappedElement.hpp"
#include "RenderablePrimitives.hpp"

#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CorePlotting {

/**
 * @brief Glyph styling options for discrete elements
 */
struct GlyphStyle {
    RenderableGlyphBatch::GlyphType glyph_type{RenderableGlyphBatch::GlyphType::Circle};
    float size{5.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief Rectangle styling options for interval elements
 */
struct RectangleStyle {
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief PolyLine styling options
 */
struct PolyLineStyle {
    float thickness{1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 model_matrix{1.0f};
};

/**
 * @brief Helper class for constructing RenderableScene objects
 * 
 * Provides a fluent interface for building scenes incrementally:
 * 1. Set bounds (required for spatial indexing of discrete elements)
 * 2. Add mapped elements using range-based API:
 *    - addGlyphs() for discrete points/events
 *    - addRectangles() for intervals
 *    - addPolyLine()/addPolyLines() for continuous data
 * 3. Optionally set View/Projection matrices
 * 4. Call build() to finalize the scene
 * 
 * Example usage with TimeSeriesMapper:
 * ```cpp
 * using namespace CorePlotting;
 * 
 * RenderableScene scene = SceneBuilder()
 *     .setBounds(bounds)
 *     .setViewState(view_state)
 *     .addGlyphs("trial1", TimeSeriesMapper::mapEvents(events1, layout1, tf))
 *     .addGlyphs("trial2", TimeSeriesMapper::mapEvents(events2, layout2, tf))
 *     .addRectangles("intervals", TimeSeriesMapper::mapIntervals(intervals, layout, tf))
 *     .build();
 * ```
 * 
 * The builder creates both rendering geometry AND spatial index from the same
 * position data, ensuring they remain synchronized.
 */
class SceneBuilder {
public:
    SceneBuilder() = default;

    /**
     * @brief Set world-space bounds for the scene (required for spatial indexing)
     * 
     * Must be called before adding any series. The bounds define the QuadTree
     * coverage area for hit testing.
     * 
     * @param bounds World-space bounding box
     * @return Reference to this builder for chaining
     */
    SceneBuilder & setBounds(BoundingBox const & bounds);

    /**
     * @brief Set View and Projection matrices from ViewState
     * @param state Current camera/viewport state
     * @return Reference to this builder for chaining
     */
    SceneBuilder & setViewState(ViewState const & state);

    /**
     * @brief Manually set View and Projection matrices
     * @param view View matrix
     * @param projection Projection matrix
     * @return Reference to this builder for chaining
     */
    SceneBuilder & setMatrices(glm::mat4 const & view, glm::mat4 const & projection);

    /**
     * @brief Set the selection state for the scene
     * 
     * Selected EntityIds will be stored in the scene and used to populate
     * selection_flags in rectangle batches during build().
     * 
     * @param selected Set of selected EntityIds
     * @return Reference to this builder for chaining
     */
    SceneBuilder & setSelectedEntities(std::unordered_set<EntityId> const & selected);

    /**
     * @brief Set the selection state for the scene (move version)
     * @param selected Set of selected EntityIds (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & setSelectedEntities(std::unordered_set<EntityId> && selected);

    // ========================================================================
    // Range-based API for mapped elements (preferred)
    // ========================================================================

    /**
     * @brief Add glyphs from a range of mapped elements
     * 
     * Creates a glyph batch for rendering AND inserts into spatial index.
     * Single traversal: positions go to both GPU buffer and QuadTree.
     * 
     * @tparam R Range type satisfying MappedElementRange concept
     * @param series_key Unique identifier for this series (for hit test results)
     * @param elements Range of MappedElement (x, y, entity_id)
     * @param style Optional styling (glyph type, size, color, model matrix)
     * @return Reference to this builder for chaining
     */
    template<MappedElementRange R>
    SceneBuilder & addGlyphs(
            std::string const & series_key,
            R && elements,
            GlyphStyle const & style = {});

    /**
     * @brief Add rectangles from a range of mapped rect elements
     * 
     * Creates a rectangle batch for rendering AND inserts centers into spatial index.
     * 
     * @tparam R Range type satisfying MappedRectRange concept
     * @param series_key Unique identifier for this series
     * @param elements Range of MappedRectElement (x, y, width, height, entity_id)
     * @param style Optional styling (color, model matrix)
     * @return Reference to this builder for chaining
     */
    template<MappedRectRange R>
    SceneBuilder & addRectangles(
            std::string const & series_key,
            R && elements,
            RectangleStyle const & style = {});

    /**
     * @brief Add a single polyline from a range of vertices
     * 
     * Creates a polyline batch for a single continuous line.
     * Does NOT add to spatial index (polylines typically use compute shader hit testing).
     * 
     * @tparam R Range type satisfying MappedVertexRange concept
     * @param series_key Unique identifier for this series
     * @param vertices Range of MappedVertex (x, y)
     * @param entity_id EntityId for the entire line
     * @param style Optional styling (thickness, color, model matrix)
     * @return Reference to this builder for chaining
     */
    template<MappedVertexRange R>
    SceneBuilder & addPolyLine(
            std::string const & series_key,
            R && vertices,
            EntityId entity_id,
            PolyLineStyle const & style = {});

    /**
     * @brief Add multiple polylines from a range of line views
     * 
     * Creates a polyline batch with multiple lines. Each line view provides
     * its own entity_id and vertex range.
     * 
     * @tparam R Range type satisfying MappedLineRange concept
     * @param series_key Unique identifier for this series
     * @param lines Range of MappedLineView (entity_id + vertices)
     * @param style Optional styling (thickness, color, model matrix)
     * @return Reference to this builder for chaining
     */
    template<MappedLineRange R>
    SceneBuilder & addPolyLines(
            std::string const & series_key,
            R && lines,
            PolyLineStyle const & style = {});

    // ========================================================================
    // Low-level batch methods (for pre-built geometry)
    // ========================================================================

    /**
     * @brief Add a polyline batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addPolyLineBatch(RenderablePolyLineBatch const & batch);

    /**
     * @brief Add a polyline batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addPolyLineBatch(RenderablePolyLineBatch && batch);

    /**
     * @brief Add a glyph batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addGlyphBatch(RenderableGlyphBatch const & batch);

    /**
     * @brief Add a glyph batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addGlyphBatch(RenderableGlyphBatch && batch);

    /**
     * @brief Add a rectangle batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addRectangleBatch(RenderableRectangleBatch const & batch);

    /**
     * @brief Add a rectangle batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder & addRectangleBatch(RenderableRectangleBatch && batch);

    /**
     * @brief Manually build spatial index from all batches
     * 
     * @note For new code, prefer addGlyphs/addRectangles which populate
     * the spatial index automatically. This method is for custom geometry.
     * 
     * @param bounds World-space bounds for the QuadTree
     * @return Reference to this builder for chaining
     */
    SceneBuilder & buildSpatialIndex(BoundingBox const & bounds);

    /**
     * @brief Finalize and return the constructed scene
     * 
     * Creates spatial index if bounds were set and discrete elements were added.
     * 
     * @return Complete RenderableScene ready for rendering
     * @throws std::runtime_error if discrete elements added without bounds
     */
    RenderableScene build();

    /**
     * @brief Reset builder to empty state
     * 
     * Clears all batches, spatial index data, and matrices.
     * Called automatically by build().
     */
    void reset();

    /**
     * @brief Get mapping from batch index to series key
     * 
     * Useful for SceneHitTester::queryIntervals which needs to map
     * batch indices back to series identifiers.
     */
    [[nodiscard]] std::map<size_t, std::string> const & getRectangleBatchKeyMap() const {
        return _rectangle_batch_key_map;
    }

    /**
     * @brief Get mapping from glyph batch index to series key
     */
    [[nodiscard]] std::map<size_t, std::string> const & getGlyphBatchKeyMap() const {
        return _glyph_batch_key_map;
    }

private:
    RenderableScene _scene;
    bool _has_matrices{false};
    bool _has_discrete_elements{false};

    // Selection state to apply during build
    std::unordered_set<EntityId> _selected_entities;

    // Bounds for spatial indexing
    std::optional<BoundingBox> _bounds;

    // Pending spatial index insertions (populated by addGlyphs/addRectangles)
    struct PendingSpatialInsert {
        float x;
        float y;
        EntityId entity_id;
    };
    std::vector<PendingSpatialInsert> _pending_spatial_inserts;

    // Mapping from batch index to series key
    std::map<size_t, std::string> _rectangle_batch_key_map;
    std::map<size_t, std::string> _glyph_batch_key_map;

    // Internal helper to build spatial index from pending inserts
    void buildSpatialIndexFromPending();
};

// ============================================================================
// Template implementations
// ============================================================================

template<MappedElementRange R>
SceneBuilder & SceneBuilder::addGlyphs(
        std::string const & series_key,
        R && elements,
        GlyphStyle const & style) {
    
    RenderableGlyphBatch batch;
    batch.glyph_type = style.glyph_type;
    batch.size = style.size;
    batch.model_matrix = style.model_matrix;

    // Single traversal: populate both batch and spatial index pending list
    for (auto const & elem : elements) {
        batch.positions.emplace_back(elem.x, elem.y);
        batch.entity_ids.push_back(elem.entity_id);
        batch.colors.push_back(style.color);  // Per-glyph color
        
        // Queue for spatial index
        _pending_spatial_inserts.push_back({elem.x, elem.y, elem.entity_id});
    }

    // Track batch key mapping
    size_t const batch_index = _scene.glyph_batches.size();
    _glyph_batch_key_map[batch_index] = series_key;

    _scene.glyph_batches.push_back(std::move(batch));
    _has_discrete_elements = true;

    return *this;
}

template<MappedRectRange R>
SceneBuilder & SceneBuilder::addRectangles(
        std::string const & series_key,
        R && elements,
        RectangleStyle const & style) {
    
    RenderableRectangleBatch batch;
    batch.model_matrix = style.model_matrix;
    
    // Single traversal: populate batch and spatial index
    for (auto const & elem : elements) {
        batch.bounds.push_back(glm::vec4(elem.x, elem.y, elem.width, elem.height));
        batch.entity_ids.push_back(elem.entity_id);
        
        // Use rectangle center for spatial index
        float const center_x = elem.x + elem.width / 2.0f;
        float const center_y = elem.y + elem.height / 2.0f;
        _pending_spatial_inserts.push_back({center_x, center_y, elem.entity_id});
    }

    // Track batch key mapping
    size_t const batch_index = _scene.rectangle_batches.size();
    _rectangle_batch_key_map[batch_index] = series_key;

    _scene.rectangle_batches.push_back(std::move(batch));
    _has_discrete_elements = true;

    return *this;
}

template<MappedVertexRange R>
SceneBuilder & SceneBuilder::addPolyLine(
        [[maybe_unused]] std::string const & series_key,
        R && vertices,
        EntityId entity_id,
        PolyLineStyle const & style) {
    
    RenderablePolyLineBatch batch;
    batch.thickness = style.thickness;
    batch.global_color = style.color;
    batch.global_entity_id = entity_id;
    batch.model_matrix = style.model_matrix;

    int32_t const start_index = 0;
    int32_t vertex_count = 0;

    for (auto const & v : vertices) {
        batch.vertices.push_back(v.x);
        batch.vertices.push_back(v.y);
        ++vertex_count;
    }

    batch.line_start_indices.push_back(start_index);
    batch.line_vertex_counts.push_back(vertex_count);
    batch.entity_ids.push_back(entity_id);

    _scene.poly_line_batches.push_back(std::move(batch));
    
    // Note: We intentionally don't add polylines to spatial index
    // Dense line data should use compute shader hit testing
    
    return *this;
}

template<MappedLineRange R>
SceneBuilder & SceneBuilder::addPolyLines(
        [[maybe_unused]] std::string const & series_key,
        R && lines,
        PolyLineStyle const & style) {
    
    RenderablePolyLineBatch batch;
    batch.thickness = style.thickness;
    batch.global_color = style.color;
    batch.model_matrix = style.model_matrix;

    int32_t current_vertex_index = 0;

    for (auto const & line : lines) {
        batch.line_start_indices.push_back(current_vertex_index);
        batch.entity_ids.push_back(line.entity_id);

        int32_t line_vertex_count = 0;
        for (auto const & v : line.vertices()) {
            batch.vertices.push_back(v.x);
            batch.vertices.push_back(v.y);
            ++line_vertex_count;
        }
        
        batch.line_vertex_counts.push_back(line_vertex_count);
        current_vertex_index += line_vertex_count;
    }

    _scene.poly_line_batches.push_back(std::move(batch));
    
    return *this;
}

}// namespace CorePlotting

#endif// COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
