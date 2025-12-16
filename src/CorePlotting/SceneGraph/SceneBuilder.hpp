#ifndef COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
#define COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP

#include "CoordinateTransform/ViewState.hpp"
#include "Layout/LayoutEngine.hpp"
#include "RenderablePrimitives.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;

namespace CorePlotting {

/**
 * @brief Pending event series data for spatial index construction
 */
struct PendingEventSeries {
    std::string series_key;
    DigitalEventSeries const* series;
    SeriesLayout const* layout;
    TimeFrame const* time_frame;
};

/**
 * @brief Pending interval series data for spatial index construction
 */
struct PendingIntervalSeries {
    std::string series_key;
    DigitalIntervalSeries const* series;
    SeriesLayout const* layout;
    TimeFrame const* time_frame;
};

/**
 * @brief Helper class for constructing RenderableScene objects
 * 
 * Provides a fluent interface for building scenes incrementally:
 * 1. Set bounds (required for spatial indexing)
 * 2. Add data series (events, intervals) - creates batches AND collects for spatial index
 * 3. Optionally set View/Projection matrices
 * 4. Call build() - automatically creates spatial index from all discrete elements
 * 
 * Example:
 * ```cpp
 * RenderableScene scene = SceneBuilder()
 *     .setBounds(bounds)
 *     .setViewState(view_state)
 *     .addEventSeries("trial1", trial1, *layout.findLayout("trial1"), time_frame)
 *     .addEventSeries("trial2", trial2, *layout.findLayout("trial2"), time_frame)
 *     .build();  // Automatically builds spatial index
 * ```
 * 
 * The builder ensures the spatial index is synchronized with the rendered geometry
 * by building both from the same source data.
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
    SceneBuilder& setBounds(BoundingBox const& bounds);

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

    // ========================================================================
    // High-level data series methods (preferred API)
    // ========================================================================

    /**
     * @brief Add an event series to the scene
     * 
     * Creates a glyph batch for rendering AND registers the series for spatial
     * indexing. The spatial index is built automatically in build().
     * 
     * @param series_key Unique identifier for this series (for hit test results)
     * @param series The event series data
     * @param layout Layout position for this series
     * @param time_frame Time frame for converting indices to absolute time
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addEventSeries(
        std::string const& series_key,
        DigitalEventSeries const& series,
        SeriesLayout const& layout,
        TimeFrame const& time_frame);

    /**
     * @brief Add an interval series to the scene
     * 
     * Creates a rectangle batch for rendering AND registers the series for
     * spatial indexing. The spatial index is built automatically in build().
     * 
     * @param series_key Unique identifier for this series
     * @param series The interval series data
     * @param layout Layout position for this series
     * @param time_frame Time frame for converting indices to absolute time
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addIntervalSeries(
        std::string const& series_key,
        DigitalIntervalSeries const& series,
        SeriesLayout const& layout,
        TimeFrame const& time_frame);

    // ========================================================================
    // Low-level batch methods (for custom geometry)
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
     * @deprecated Prefer using addEventSeries/addIntervalSeries which handle
     * spatial indexing automatically. This method is for backwards compatibility.
     * 
     * @param bounds World-space bounds for the QuadTree
     * @return Reference to this builder for chaining
     */
    SceneBuilder & buildSpatialIndex(BoundingBox const & bounds);

    /**
     * @brief Finalize and return the constructed scene
     * 
     * If discrete elements (events, intervals) were added via addEventSeries
     * or addIntervalSeries, automatically builds the spatial index.
     * 
     * @return Complete RenderableScene ready for rendering
     * @throws std::runtime_error if bounds not set but discrete series were added
     */
    RenderableScene build();

    /**
     * @brief Reset builder to empty state
     * 
     * Clears all batches, pending series, and matrices. Called automatically by build().
     */
    void reset();

    /**
     * @brief Get mapping from batch index to series key
     * 
     * Useful for SceneHitTester::queryIntervals which needs to map
     * batch indices back to series identifiers.
     */
    [[nodiscard]] std::map<size_t, std::string> const& getRectangleBatchKeyMap() const {
        return _rectangle_batch_key_map;
    }

private:
    RenderableScene _scene;
    bool _has_matrices{false};
    
    // Bounds for spatial indexing
    std::optional<BoundingBox> _bounds;
    
    // Pending series for spatial index construction
    std::vector<PendingEventSeries> _pending_events;
    std::vector<PendingIntervalSeries> _pending_intervals;
    
    // Mapping from batch index to series key
    std::map<size_t, std::string> _rectangle_batch_key_map;
    
    // Internal helper to build spatial index from pending series
    void buildSpatialIndexFromPendingSeries();
};

}// namespace CorePlotting

#endif// COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
