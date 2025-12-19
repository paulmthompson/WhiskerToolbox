#ifndef PLOTTINGOPENGL_RENDERERS_PREVIEWRENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_PREVIEWRENDERER_HPP

#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "PlottingOpenGL/GLContext.hpp"

#include <glm/glm.hpp>

namespace PlottingOpenGL {

/**
 * @brief Renders interactive preview geometry in screen space
 * 
 * PreviewRenderer draws GlyphPreview primitives directly in canvas/pixel
 * coordinates using an orthographic projection. This is used for interactive
 * glyph creation and modification feedback (drag rectangles, selection lines, etc.).
 * 
 * Unlike the batch renderers (RectangleRenderer, GlyphRenderer, etc.) which work
 * in world coordinates with Model-View-Projection transforms, PreviewRenderer
 * uses a simple screen-space ortho projection where coordinates map directly
 * to pixels.
 * 
 * **Coordinate System**:
 * - Origin at top-left corner of viewport
 * - X increases rightward (0 to viewport_width)
 * - Y increases downward (0 to viewport_height)
 * 
 * **Rendering Features**:
 * - Filled rectangles with optional stroke
 * - Lines with configurable width
 * - "Ghost" rendering of original geometry during modification
 * - Alpha blending for semi-transparent previews
 * 
 * Usage:
 * @code
 *   // In initializeGL():
 *   PreviewRenderer preview_renderer;
 *   preview_renderer.initialize();
 *   
 *   // In paintGL(), after scene rendering:
 *   if (controller.isActive()) {
 *       auto preview = controller.getPreview();
 *       preview_renderer.render(preview, viewport_width, viewport_height);
 *   }
 *   
 *   // Before context destruction:
 *   preview_renderer.cleanup();
 * @endcode
 * 
 * @see CorePlotting::Interaction::GlyphPreview
 * @see CorePlotting::Interaction::IGlyphInteractionController
 */
class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();

    // Non-copyable, non-movable
    PreviewRenderer(PreviewRenderer const &) = delete;
    PreviewRenderer & operator=(PreviewRenderer const &) = delete;
    PreviewRenderer(PreviewRenderer &&) = delete;
    PreviewRenderer & operator=(PreviewRenderer &&) = delete;

    /**
     * @brief Initialize GPU resources (shaders, VAO, VBOs)
     * 
     * Must be called with a valid OpenGL context.
     * 
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Release all GPU resources
     * 
     * Safe to call even if initialize() was never called.
     */
    void cleanup();

    /**
     * @brief Check if renderer is initialized
     */
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    /**
     * @brief Render a preview in screen coordinates
     * 
     * Draws the preview geometry using an orthographic projection where
     * coordinates map directly to pixels.
     * 
     * @param preview The preview geometry to render
     * @param viewport_width Width of the viewport in pixels
     * @param viewport_height Height of the viewport in pixels
     */
    void render(CorePlotting::Interaction::GlyphPreview const & preview,
                int viewport_width,
                int viewport_height);

private:
    void renderRectangle(CorePlotting::Interaction::GlyphPreview const & preview,
                         glm::mat4 const & ortho);
    void renderLine(CorePlotting::Interaction::GlyphPreview const & preview,
                    glm::mat4 const & ortho);
    void renderPoint(CorePlotting::Interaction::GlyphPreview const & preview,
                     glm::mat4 const & ortho);
    void renderPolygon(CorePlotting::Interaction::GlyphPreview const & preview,
                       glm::mat4 const & ortho);

    void renderSingleRectangle(glm::vec4 const & bounds, glm::vec4 const & fill_color,
                                glm::vec4 const & stroke_color, float stroke_width,
                                bool show_fill, bool show_stroke,
                                glm::mat4 const & ortho);
    void renderSingleLine(glm::vec2 const & start, glm::vec2 const & end,
                          glm::vec4 const & color, float width,
                          glm::mat4 const & ortho);
    void renderSinglePoint(glm::vec2 const & pos, glm::vec4 const & color,
                           float radius, glm::mat4 const & ortho);

    GLShaderProgram m_fill_shader;
    GLShaderProgram m_line_shader;

    GLVertexArray m_vao;
    GLBuffer m_vertex_buffer{GLBuffer::Type::Vertex};

    bool m_initialized{false};
};

/**
 * @brief Embedded shader source code for the preview renderer
 */
namespace PreviewShaders {

/**
 * @brief Simple vertex shader for preview geometry
 * 
 * Takes 2D positions and transforms via an orthographic matrix.
 */
constexpr char const * VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec2 a_position;

uniform mat4 u_ortho_matrix;

void main() {
    gl_Position = u_ortho_matrix * vec4(a_position, 0.0, 1.0);
}
)";

/**
 * @brief Simple fragment shader with uniform color
 */
constexpr char const * FRAGMENT_SHADER = R"(
#version 410 core

uniform vec4 u_color;

out vec4 FragColor;

void main() {
    FragColor = u_color;
}
)";

} // namespace PreviewShaders

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_RENDERERS_PREVIEWRENDERER_HPP
