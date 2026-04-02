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

    /**
     * @brief Upload per-line color overrides.
     *
     * Each entry corresponds to a line in LineBatchData::lines (0-based).
     * A color with alpha > 0 overrides the global color for that line.
     * Alpha == 0 means "use global color" (no override).
     *
     * Must be called after syncFromStore() — the number of entries must
     * equal the number of lines in the current LineBatchData.
     */
    void updateLineColorOverrides(std::vector<glm::vec4> const & per_line_colors);

    /// Whether GPU SSBOs are used (GL 4.3) vs CPU-side filtering (GL 4.1).
    [[nodiscard]] bool isUsingSSBOs() const { return m_use_ssbos; }

    /**
     * @brief Upload vertex data from the store's CPU mirror to the VBO/VAO.
     *
     * Call after BatchLineStore::upload() to sync the render buffers.
     * Separate from IBatchRenderer::uploadData because the data source is
     * the store, not a RenderablePrimitive.
     *
     * Uploads to four VBOs:
     *   1. m_vertex_vbo — xy positions from cpu.segments (2 floats per vertex)
     *   2. m_line_id_vbo — line IDs duplicated for both vertices of each segment
     *   3. m_selection_vbo — per-vertex selection flags (GL 4.1 fallback path)
     *   4. m_color_override_vbo — transparent overrides (defaults; call
     *      updateLineColorOverrides() afterwards if group coloring is needed)
     *
     * An empty CPU mirror (cpu.empty()) is handled gracefully — clears
     * m_total_vertices and returns without touching the GPU.
     *
     * @pre initialize() must have been called and returned true
     *      (enforcement: runtime_check — returns immediately if !m_initialized)
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — m_vao.bind() and all VBO allocate() calls issue raw GL immediately.
     * @pre BatchLineStore::upload() must have been called on m_store at least
     *      once before syncFromStore() (enforcement: runtime_check — silent)
     *      — if no upload() has been called, cpu.empty() fires, m_total_vertices
     *      is set to 0, and the function returns. Caller cannot distinguish
     *      "store empty" from "store never uploaded".
     * @pre m_store.cpuData().selection_mask.size() == m_store.numLines()
     *      (enforcement: none) [CRITICAL]
     *      — the selection flag loop accesses cpu.selection_mask[id - 1] with
     *      a bounds check against cpu.numLines() (== cpu.lines.size()), but NOT
     *      against cpu.selection_mask.size(). If selection_mask is shorter than
     *      lines, an out-of-bounds vector read occurs for line_ids whose 1-based
     *      id exceeds selection_mask.size().
     * @pre m_store.cpuData().line_ids.size() == m_store.numSegments()
     *      (enforcement: none) [IMPORTANT]
     *      — line_ids is iterated once per segment to build both the line_id VBO
     *      and the selection flag VBO. A shorter line_ids would produce VBOs
     *      misaligned with the vertex buffer, causing incorrect per-vertex
     *      attribute reads in the shader.
     * @pre cpu.segments.size() * sizeof(float) <= INT_MAX (narrowing cast to
     *      int for m_vertex_vbo.allocate()) (enforcement: none) [LOW]
     *
     * @post m_total_vertices == m_store.numSegments() * 2.
     * @post m_canvas_size is updated from cpu.canvas_width / canvas_height.
     * @post m_view_dirty == true (render cache invalidated).
     */
    void syncFromStore();

    /**
     * @brief Render to an offscreen FBO, then blit to the current framebuffer.
     *
     * @note **FBO caching is not yet implemented.** This function currently
     *       renders directly to the active framebuffer on every call, ignoring
     *       both the `force_redraw` parameter and the `m_view_dirty` flag.
     *       The documented FBO/blit pattern (render-to-texture + cache) is
     *       planned but not present in the current implementation.
     *       Only `m_view_dirty` is written at the end of this call (set to false).
     *
     * Returns without rendering if !isInitialized() or hasData() == false.
     *
     * @param view         View matrix
     * @param proj         Projection matrix
     * @param model        Model matrix (typically identity for batch lines)
     * @param force_redraw Currently ignored — present for future FBO interface
     *                     compatibility only.
     *
     * @pre initialize() must have been called and returned true
     *      (enforcement: runtime_check — returns immediately if !m_initialized)
     * @pre syncFromStore() must have been called at least once after the last
     *      BatchLineStore::upload() to populate vertex buffers and m_total_vertices
     *      (enforcement: runtime_check — silent via m_total_vertices == 0 guard;
     *      caller cannot distinguish "no data" from "sync not called")
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — renderLines() and renderHoverLine() issue GL draw calls with no
     *      context guard beyond the m_initialized check at entry.
     * @pre setViewportSize() must have been called with a positive x component
     *      (m_viewport_size.x > 0) (enforcement: none) [IMPORTANT]
     *      — the geometry shader computes half_width_ndc =
     *      (u_line_width / u_viewport_size.x) * 2.0; a zero x value causes
     *      shader division by zero, producing Inf/NaN in all vertex positions.
     * @pre m_line_width > 0 (enforcement: none) [LOW]
     *      — a negative line width produces geometrically flipped quad strips
     *      in the geometry shader without any error or clamp.
     *
     * @post m_view_dirty == false (regardless of whether anything was rendered).
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
    GLBuffer m_selection_vbo{GLBuffer::Type::Vertex};  // Per-vertex selection flag (GL 4.1 fallback)
    GLBuffer m_color_override_vbo{GLBuffer::Type::Vertex};  // Per-vertex color override (vec4)

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
    void uploadDefaultColorOverrides();
};

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_LINEBATCH_BATCHLINERENDERER_HPP
