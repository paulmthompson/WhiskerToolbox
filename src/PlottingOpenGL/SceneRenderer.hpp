#ifndef PLOTTINGOPENGL_SCENERENDERER_HPP
#define PLOTTINGOPENGL_SCENERENDERER_HPP

#include "Renderers/GlyphRenderer.hpp"
#include "Renderers/PolyLineRenderer.hpp"
#include "Renderers/PreviewRenderer.hpp"
#include "Renderers/RectangleRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace PlottingOpenGL {

/**
 * @brief Coordinates rendering of a complete RenderableScene.
 * 
 * SceneRenderer is the main entry point for rendering CorePlotting scenes
 * using OpenGL. It manages the lifecycle of individual batch renderers and
 * orchestrates the rendering of all primitives in a scene.
 * 
 * Usage:
 * @code
 *   // In initializeGL():
 *   SceneRenderer renderer;
 *   renderer.initialize();
 *   
 *   // When scene data changes:
 *   renderer.uploadScene(scene);
 *   
 *   // In paintGL():
 *   renderer.render();
 *   
 *   // Before context destruction:
 *   renderer.cleanup();
 * @endcode
 * 
 * The SceneRenderer holds the View and Projection matrices from the scene
 * and passes them to each batch renderer along with the batch's Model matrix.
 * 
 * @see CorePlotting::RenderableScene
 */
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    // Non-copyable, non-movable (GPU resources don't transfer well)
    SceneRenderer(SceneRenderer const &) = delete;
    SceneRenderer & operator=(SceneRenderer const &) = delete;
    SceneRenderer(SceneRenderer &&) = delete;
    SceneRenderer & operator=(SceneRenderer &&) = delete;

    /**
     * @brief Initialize all batch renderers.
     * 
     * Must be called with a valid OpenGL context.
     * 
     * @return true if all renderers initialized successfully
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Release all GPU resources.
     * 
     * Should be called before the OpenGL context is destroyed.
     */
    void cleanup();

    /**
     * @brief Check if all renderers are initialized.
     */
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Upload a complete scene for rendering.
     * 
     * This distributes the scene's batches to the appropriate renderers
     * and stores the View/Projection matrices.
     * 
     * @param scene The scene to upload
     */
    void uploadScene(CorePlotting::RenderableScene const & scene);

    /**
     * @brief Clear all uploaded scene data.
     */
    void clearScene();

    /**
     * @brief Render the current scene.
     * 
     * Draws all uploaded batches using the stored View/Projection matrices.
     * Call this in your paintGL() or equivalent.
     */
    void render();

    /**
     * @brief Render with explicit View/Projection matrices.
     * 
     * Overrides the stored matrices for this render call only.
     * Useful for rendering the same scene from different viewpoints.
     * 
     * @param view_matrix Camera/view transformation
     * @param projection_matrix Projection to NDC
     */
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix);

    /**
     * @brief Set the rendering order for batch types.
     * 
     * By default, the order is: Rectangles -> PolyLines -> Glyphs
     * (back to front, so rectangles are behind lines and points).
     */
    enum class BatchType {
        Rectangle,
        PolyLine,
        Glyph
    };
    void setRenderOrder(std::vector<BatchType> const & order);

    // Access to individual renderers for fine-grained control
    [[nodiscard]] PolyLineRenderer & polyLineRenderer() { return m_poly_line_renderer; }
    [[nodiscard]] GlyphRenderer & glyphRenderer() { return m_glyph_renderer; }
    [[nodiscard]] RectangleRenderer & rectangleRenderer() { return m_rectangle_renderer; }
    [[nodiscard]] PreviewRenderer & previewRenderer() { return m_preview_renderer; }

    /**
     * @brief Render an interactive preview overlay
     * 
     * Call this after render() to draw interactive preview geometry
     * (drag rectangles, selection lines, etc.) on top of the scene.
     * 
     * @param preview The preview geometry to render
     * @param viewport_width Width of the viewport in pixels
     * @param viewport_height Height of the viewport in pixels
     */
    void renderPreview(CorePlotting::Interaction::GlyphPreview const & preview,
                       int viewport_width,
                       int viewport_height);

private:
    PolyLineRenderer m_poly_line_renderer;
    GlyphRenderer m_glyph_renderer;
    RectangleRenderer m_rectangle_renderer;
    PreviewRenderer m_preview_renderer;

    // Scene-level matrices
    glm::mat4 m_view_matrix{1.0f};
    glm::mat4 m_projection_matrix{1.0f};

    // Render order
    std::vector<BatchType> m_render_order{
            BatchType::Rectangle,
            BatchType::PolyLine,
            BatchType::Glyph};

    bool m_initialized{false};
};

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_SCENERENDERER_HPP
