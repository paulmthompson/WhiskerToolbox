#ifndef COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
#define COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP

#include "RenderablePrimitives.hpp"
#include "../CoordinateTransform/ViewState.hpp"
#include "../Layout/LayoutEngine.hpp"

namespace CorePlotting {

/**
 * @brief Helper class for constructing RenderableScene objects
 * 
 * Provides a fluent interface for building scenes incrementally:
 * 1. Set global matrices (View, Projection)
 * 2. Add batches with their Model matrices
 * 3. Build spatial index from all batches
 * 4. Finalize into RenderableScene
 * 
 * Example:
 * ```cpp
 * SceneBuilder builder;
 * builder.setViewState(view_state)
 *        .addPolyLineBatch(batch, model_matrix)
 *        .buildSpatialIndex(bounds)
 *        .build();
 * ```
 */
class SceneBuilder {
public:
    SceneBuilder() = default;
    
    /**
     * @brief Set View and Projection matrices from ViewState
     * @param state Current camera/viewport state
     * @return Reference to this builder for chaining
     */
    SceneBuilder& setViewState(ViewState const& state);
    
    /**
     * @brief Manually set View and Projection matrices
     * @param view View matrix
     * @param projection Projection matrix
     * @return Reference to this builder for chaining
     */
    SceneBuilder& setMatrices(glm::mat4 const& view, glm::mat4 const& projection);
    
    /**
     * @brief Add a polyline batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addPolyLineBatch(RenderablePolyLineBatch const& batch);
    
    /**
     * @brief Add a polyline batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addPolyLineBatch(RenderablePolyLineBatch&& batch);
    
    /**
     * @brief Add a glyph batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addGlyphBatch(RenderableGlyphBatch const& batch);
    
    /**
     * @brief Add a glyph batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addGlyphBatch(RenderableGlyphBatch&& batch);
    
    /**
     * @brief Add a rectangle batch to the scene
     * @param batch Batch to add (will be copied)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addRectangleBatch(RenderableRectangleBatch const& batch);
    
    /**
     * @brief Add a rectangle batch to the scene (move version)
     * @param batch Batch to add (will be moved)
     * @return Reference to this builder for chaining
     */
    SceneBuilder& addRectangleBatch(RenderableRectangleBatch&& batch);
    
    /**
     * @brief Build spatial index from all batches
     * 
     * Iterates through all batches and inserts their geometry into a QuadTree.
     * Uses the same world-space coordinates as the Model matrices.
     * 
     * @param bounds World-space bounds for the QuadTree
     * @return Reference to this builder for chaining
     */
    SceneBuilder& buildSpatialIndex(BoundingBox const& bounds);
    
    /**
     * @brief Finalize and return the constructed scene
     * 
     * Transfers ownership of all data to the returned RenderableScene.
     * The builder is reset to empty state after this call.
     * 
     * @return Complete RenderableScene ready for rendering
     */
    RenderableScene build();
    
    /**
     * @brief Reset builder to empty state
     * 
     * Clears all batches and matrices. Called automatically by build().
     */
    void reset();

private:
    RenderableScene _scene;
    bool _has_matrices{false};
};

} // namespace CorePlotting

#endif // COREPLOTTING_SCENEGRAPH_SCENEBUILDER_HPP
