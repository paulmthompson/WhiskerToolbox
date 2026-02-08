/**
 * @file BatchLineRenderer.hpp
 * @brief Renders large batches of lines from BatchLineStore via geometry shaders
 *
 * Reads segment data from BatchLineStore's shared VBOs/SSBOs.  On GL 4.3,
 * uses SSBOs for visibility/selection masks in the geometry shader.  On GL 4.1
 * (macOS), falls back to CPU-side filtering.
 *
 * Supports:
 *  - Visibility filtering (hidden lines not drawn)
 *  - Selection highlighting (selected lines drawn with different color)
 *  - Hover highlighting (single line drawn on top with distinct color)
 *  - Group coloring (per-line palette index via a per-vertex attribute)
 *
 * Managed independently by the widget, not as a SceneRenderer slot.
 * See batch_line_selection_roadmap.md §Resolved Questions for rationale.
 */
#ifndef PLOTTINGOPENGL_LINEBATCH_BATCHLINERENDERER_HPP
#define PLOTTINGOPENGL_LINEBATCH_BATCHLINERENDERER_HPP

#include "BatchLineStore.hpp"

#include "PlottingOpenGL/GLContext.hpp"
#include "PlottingOpenGL/Renderers/IBatchRenderer.hpp"

#include <glm/glm.hpp>

#include <optional>
#include <string>

namespace PlottingOpenGL {

class BatchLineRenderer : public IBatchRenderer {
public:
    /**
     * @brief Construct a BatchLineRenderer backed by the given store.
     *
     * @param store  The BatchLineStore that owns the segment data.
     *               Must outlive this renderer.
     * @param shader_base_path  Base path to the shaders directory (e.g.,
     *                          ":/shaders/").  If empty, uses embedded fallback.
     */
    explicit BatchLineRenderer(
        BatchLineStore & store,
        std::string shader_base_path = "");
    ~BatchLineRenderer() override;

    // ── IBatchRenderer interface ───────────────────────────────────────

    [[nodiscard]] bool initialize() override;
    void cleanup() override;
    [[nodiscard]] bool isInitialized() const override;
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix) override;
    [[nodiscard]] bool hasData() const override;
    void clearData() override;

    // ── Appearance ─────────────────────────────────────────────────────

    void setLineWidth(float width) { m_line_width = width; }
    void setGlobalColor(glm::vec4 const & color) { m_global_color = color; }
    void setSelectedColor(glm::vec4 const & color) { m_selected_color = color; }
    void setHoverColor(glm::vec4 const & color) { m_hover_color = color; }
    void setCanvasSize(glm::vec2 const & size) { m_canvas_size = size; }
    void setViewportSize(glm::vec2 const & size) { m_viewport_size = size; }

    // ── Interaction state ──────────────────────────────────────────────

    /**
     * @brief Set the line index to render with hover highlighting.
     *
     * @param line_index  0-based index into LineBatchData::lines,
     *                    or std::nullopt to clear hover.
     */
    void setHoverLine(std::optional<std::uint32_t> line_index);

    /// Whether GPU SSBOs are used (GL 4.3) vs CPU-side filtering (GL 4.1).
    [[nodiscard]] bool isUsingSSBOs() const { return m_use_ssbos; }

    /**
     * @brief Upload vertex data from the store's CPU mirror to the VBO/VAO.
     *
     * Call after BatchLineStore::upload() to sync the render buffers.
     * Separate from IBatchRenderer::uploadData because the data source is
     * the store, not a RenderablePrimitive.
     */
    void syncFromStore();

    /**
     * @brief Render to an offscreen FBO, then blit to the current framebuffer.
     *
     * This follows the pattern from LineDataVisualization: render to a cached
     * FBO, then blit.  The FBO is re-rendered only when the view is dirty.
     *
     * @param view     View matrix
     * @param proj     Projection matrix
     * @param model    Model matrix (typically identity for batch lines)
     * @param force_redraw  If true, always re-render to FBO even if cached
     */
    void renderWithCache(glm::mat4 const & view,
                         glm::mat4 const & proj,
                         glm::mat4 const & model = glm::mat4{1.0f},
                         bool force_redraw = false);

    /// Mark the cached FBO as needing re-render.
    void markViewDirty() { m_view_dirty = true; }

private:
    BatchLineStore & m_store;
    std::string m_shader_base_path;

    // ── GL Resources ───────────────────────────────────────────────────
    GLVertexArray m_vao;
    GLBuffer m_vertex_vbo{GLBuffer::Type::Vertex};
    GLBuffer m_line_id_vbo{GLBuffer::Type::Vertex};

    // ── Shader state ───────────────────────────────────────────────────
    bool m_use_ssbos{false};          // True if GL 4.3 SSBOs available
    bool m_use_shader_manager{false}; // True if using ShaderManager

    // Only used when not using ShaderManager
    GLShaderProgram m_embedded_shader;

    // ── Appearance state ───────────────────────────────────────────────
    glm::vec4 m_global_color{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec4 m_selected_color{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 m_hover_color{1.0f, 1.0f, 0.0f, 1.0f};
    glm::vec2 m_canvas_size{640.0f, 480.0f};
    glm::vec2 m_viewport_size{1024.0f, 1024.0f};
    float m_line_width{1.0f};

    // ── Hover state ────────────────────────────────────────────────────
    std::optional<std::uint32_t> m_hover_line_index;

    // ── Render state ───────────────────────────────────────────────────
    bool m_initialized{false};
    bool m_view_dirty{true};
    int m_total_vertices{0};

    // ── Shader program names ───────────────────────────────────────────
    static constexpr char const * SHADER_43_NAME = "batch_line_with_geometry_43";
    static constexpr char const * SHADER_41_NAME = "batch_line_with_geometry";

    // ── Private methods ────────────────────────────────────────────────
    bool loadShadersFromManager();
    bool compileEmbeddedShaders();
    void setupVertexAttributes();
    void renderLines(glm::mat4 const & mvp);
    void renderHoverLine(glm::mat4 const & mvp);
};

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_LINEBATCH_BATCHLINERENDERER_HPP
