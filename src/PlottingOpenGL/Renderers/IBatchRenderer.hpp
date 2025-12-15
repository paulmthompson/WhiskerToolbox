#ifndef PLOTTINGOPENGL_RENDERERS_IBATCHRENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_IBATCHRENDERER_HPP

#include <glm/glm.hpp>

namespace PlottingOpenGL {

/**
 * @brief Abstract base interface for all batch renderers.
 * 
 * This interface defines the lifecycle and rendering contract for GPU-accelerated
 * batch renderers. Implementations handle specific primitive types (polylines,
 * glyphs, rectangles) and manage their own GPU resources (VBOs, VAOs, shaders).
 * 
 * Lifecycle:
 *   1. Construction (renderer created but not initialized)
 *   2. initialize() - Allocate GPU resources (must be called with valid GL context)
 *   3. uploadData() - Upload batch data to GPU (called when data changes)
 *   4. render() - Issue draw calls (called each frame)
 *   5. cleanup() - Release GPU resources (call before context destruction)
 * 
 * Thread Safety:
 *   All methods must be called from the thread with the active OpenGL context.
 * 
 * @see PolyLineRenderer, GlyphRenderer, RectangleRenderer
 */
class IBatchRenderer {
public:
    virtual ~IBatchRenderer() = default;

    /**
     * @brief Initialize GPU resources (VAOs, VBOs, shaders).
     * 
     * Must be called with a valid, current OpenGL context.
     * Can be called multiple times (will reinitialize if needed).
     * 
     * @return true if initialization succeeded
     */
    [[nodiscard]] virtual bool initialize() = 0;

    /**
     * @brief Release all GPU resources.
     * 
     * Should be called before the OpenGL context is destroyed.
     * Safe to call even if initialize() was never called.
     */
    virtual void cleanup() = 0;

    /**
     * @brief Check if the renderer has been successfully initialized.
     */
    [[nodiscard]] virtual bool isInitialized() const = 0;

    /**
     * @brief Render the current batch with the given View and Projection matrices.
     * 
     * The Model matrix is stored per-batch in the renderable data structure
     * and will be combined with the provided VP matrices.
     * 
     * @param view_matrix Global camera/view transformation
     * @param projection_matrix Projection to NDC
     */
    virtual void render(glm::mat4 const & view_matrix,
                        glm::mat4 const & projection_matrix) = 0;

    /**
     * @brief Check if there is any data to render.
     * 
     * @return true if the renderer has data uploaded and ready to draw
     */
    [[nodiscard]] virtual bool hasData() const = 0;

    /**
     * @brief Clear all uploaded data (but keep GPU resources allocated).
     */
    virtual void clearData() = 0;

protected:
    IBatchRenderer() = default;

    // Non-copyable, non-movable (GPU resources don't transfer well)
    IBatchRenderer(IBatchRenderer const &) = delete;
    IBatchRenderer & operator=(IBatchRenderer const &) = delete;
    IBatchRenderer(IBatchRenderer &&) = delete;
    IBatchRenderer & operator=(IBatchRenderer &&) = delete;
};

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_IBATCHRENDERER_HPP
