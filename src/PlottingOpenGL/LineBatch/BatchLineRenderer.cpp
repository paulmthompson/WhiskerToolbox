/**
 * @file BatchLineRenderer.cpp
 * @brief Batch line rendering from BatchLineStore — ported from LineDataVisualization
 */
#include "BatchLineRenderer.hpp"

#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderSourceType.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>

#include <iostream>

namespace PlottingOpenGL {

// ── Embedded fallback shaders (GL 4.1 — no SSBO) ──────────────────────

namespace BatchLineShaders {

constexpr char const * VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in uint a_line_id;

uniform mat4 u_mvp_matrix;

out vec2 v_position;
flat out uint v_line_id;

void main() {
    vec4 ndc_pos = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
    v_position = ndc_pos.xy;
    v_line_id = a_line_id;
    gl_Position = ndc_pos;
}
)";

constexpr char const * GEOMETRY_SHADER = R"(
#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec2 v_position[];
flat in uint v_line_id[];

out vec2 g_position;
flat out uint g_line_id;
flat out uint g_is_selected;

uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    uint line_id = v_line_id[0];

    vec2 p0 = v_position[0];
    vec2 p1 = v_position[1];

    vec2 line_dir = normalize(p1 - p0);
    vec2 perp = vec2(-line_dir.y, line_dir.x);

    float half_width_ndc = (u_line_width / u_viewport_size.x) * 2.0;

    vec2 v0 = p0 - perp * half_width_ndc;
    vec2 v1 = p0 + perp * half_width_ndc;
    vec2 v2 = p1 - perp * half_width_ndc;
    vec2 v3 = p1 + perp * half_width_ndc;

    uint is_selected = 0u;

    g_position = v0; g_line_id = line_id; g_is_selected = is_selected;
    gl_Position = vec4(v0, 0.0, 1.0); EmitVertex();

    g_position = v1; g_line_id = line_id; g_is_selected = is_selected;
    gl_Position = vec4(v1, 0.0, 1.0); EmitVertex();

    g_position = v2; g_line_id = line_id; g_is_selected = is_selected;
    gl_Position = vec4(v2, 0.0, 1.0); EmitVertex();

    g_position = v3; g_line_id = line_id; g_is_selected = is_selected;
    gl_Position = vec4(v3, 0.0, 1.0); EmitVertex();

    EndPrimitive();
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 410 core

flat in uint g_line_id;
flat in uint g_is_selected;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform vec4 u_selected_color;
uniform uint u_hover_line_id;

out vec4 FragColor;

void main() {
    vec4 final_color = u_color;

    if (g_is_selected != 0u) {
        final_color = u_selected_color;
    }

    if (u_hover_line_id > 0u && g_line_id == u_hover_line_id) {
        final_color = u_hover_color;
    }

    FragColor = final_color;
}
)";

} // namespace BatchLineShaders

// ── Construction / Destruction ─────────────────────────────────────────

BatchLineRenderer::BatchLineRenderer(
    BatchLineStore & store,
    std::string shader_base_path)
    : m_store(store),
      m_shader_base_path(std::move(shader_base_path))
{
}

BatchLineRenderer::~BatchLineRenderer()
{
    cleanup();
}

// ── IBatchRenderer ─────────────────────────────────────────────────────

bool BatchLineRenderer::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!GLFunctions::hasCurrentContext()) {
        return false;
    }

    // Detect GL version for SSBO support
    auto * ctx = QOpenGLContext::currentContext();
    if (ctx) {
        auto fmt = ctx->format();
        m_use_ssbos = (fmt.majorVersion() > 4) ||
                      (fmt.majorVersion() == 4 && fmt.minorVersion() >= 3);
    }

    // Load shaders
    if (!m_shader_base_path.empty()) {
        if (loadShadersFromManager()) {
            m_use_shader_manager = true;
        } else {
            std::cerr << "[BatchLineRenderer] ShaderManager load failed, "
                      << "falling back to embedded shaders\n";
            if (!compileEmbeddedShaders()) {
                return false;
            }
        }
    } else {
        if (!compileEmbeddedShaders()) {
            return false;
        }
    }

    // VAO + VBOs
    if (!m_vao.create() || !m_vertex_vbo.create() || !m_line_id_vbo.create()) {
        return false;
    }

    setupVertexAttributes();

    m_initialized = true;
    return true;
}

void BatchLineRenderer::cleanup()
{
    m_vertex_vbo.destroy();
    m_line_id_vbo.destroy();
    m_vao.destroy();
    if (!m_use_shader_manager) {
        m_embedded_shader.destroy();
    }
    m_initialized = false;
    m_total_vertices = 0;
}

bool BatchLineRenderer::isInitialized() const
{
    return m_initialized;
}

void BatchLineRenderer::render(glm::mat4 const & view_matrix,
                               glm::mat4 const & projection_matrix)
{
    if (!m_initialized || m_total_vertices == 0) {
        return;
    }

    glm::mat4 const mvp = projection_matrix * view_matrix;
    renderLines(mvp);

    if (m_hover_line_index.has_value()) {
        renderHoverLine(mvp);
    }
}

bool BatchLineRenderer::hasData() const
{
    return m_total_vertices > 0;
}

void BatchLineRenderer::clearData()
{
    m_total_vertices = 0;
    m_view_dirty = true;
}

// ── Appearance / Interaction ───────────────────────────────────────────

void BatchLineRenderer::setHoverLine(std::optional<std::uint32_t> line_index)
{
    m_hover_line_index = line_index;
}

// ── Sync from store ────────────────────────────────────────────────────

void BatchLineRenderer::syncFromStore()
{
    if (!m_initialized) {
        return;
    }

    auto const & cpu = m_store.cpuData();
    if (cpu.empty()) {
        m_total_vertices = 0;
        m_view_dirty = true;
        return;
    }

    // Upload vertex positions (flat x,y pairs — 2 vertices per segment)
    // The segments array is {x1,y1,x2,y2, x1,y1,x2,y2, ...}
    // For GL_LINES, we need consecutive (x,y) pairs.
    // LineBatchData::segments is already in that format (each 4 floats = 2 vertices).
    (void) m_vao.bind();

    (void) m_vertex_vbo.bind();
    m_vertex_vbo.allocate(
        cpu.segments.data(),
        static_cast<int>(cpu.segments.size() * sizeof(float)));
    m_vertex_vbo.release();

    // Upload per-vertex line IDs (each segment has 2 vertices with same line_id)
    // We need to duplicate each line_id for both vertices of the segment.
    std::vector<std::uint32_t> per_vertex_ids;
    per_vertex_ids.reserve(cpu.line_ids.size() * 2);
    for (auto const id : cpu.line_ids) {
        per_vertex_ids.push_back(id);
        per_vertex_ids.push_back(id);
    }

    (void) m_line_id_vbo.bind();
    m_line_id_vbo.allocate(
        per_vertex_ids.data(),
        static_cast<int>(per_vertex_ids.size() * sizeof(std::uint32_t)));
    m_line_id_vbo.release();

    m_vao.release();

    m_total_vertices = static_cast<int>(cpu.numSegments() * 2);
    m_canvas_size = {cpu.canvas_width, cpu.canvas_height};
    m_view_dirty = true;
}

// ── Cached rendering ───────────────────────────────────────────────────

void BatchLineRenderer::renderWithCache(
    glm::mat4 const & view,
    glm::mat4 const & proj,
    glm::mat4 const & model,
    bool /*force_redraw*/)
{
    // For the initial implementation, we simply render directly.
    // FBO caching (render-to-texture + blit) can be added later
    // to match the LineDataVisualization's framebuffer caching pattern.
    if (!m_initialized || m_total_vertices == 0) {
        return;
    }

    glm::mat4 const mvp = proj * view * model;
    renderLines(mvp);

    if (m_hover_line_index.has_value()) {
        renderHoverLine(mvp);
    }

    m_view_dirty = false;
}

// ── Private: shader loading ────────────────────────────────────────────

bool BatchLineRenderer::loadShadersFromManager()
{
    ShaderManager & sm = ShaderManager::instance();

    char const * program_name = m_use_ssbos ? SHADER_43_NAME : SHADER_41_NAME;

    if (sm.getProgram(program_name)) {
        return true; // Already loaded for this context
    }

    if (m_use_ssbos) {
        // GL 4.3: use shaders with SSBO support
        return sm.loadProgram(
            program_name,
            m_shader_base_path + "line_with_geometry.vert",
            m_shader_base_path + "line_with_geometry_43.frag",
            m_shader_base_path + "line_with_geometry_43.geom",
            ShaderSourceType::Resource);
    } else {
        // GL 4.1: shaders without SSBO
        return sm.loadProgram(
            program_name,
            m_shader_base_path + "line_with_geometry.vert",
            m_shader_base_path + "line_with_geometry.frag",
            m_shader_base_path + "line_with_geometry.geom",
            ShaderSourceType::Resource);
    }
}

bool BatchLineRenderer::compileEmbeddedShaders()
{
    return m_embedded_shader.createFromSource(
        BatchLineShaders::VERTEX_SHADER,
        BatchLineShaders::GEOMETRY_SHADER,
        BatchLineShaders::FRAGMENT_SHADER);
}

void BatchLineRenderer::setupVertexAttributes()
{
    auto * f = GLFunctions::get();
    if (!f) {
        return;
    }

    (void) m_vao.bind();

    // Attribute 0: position vec2 (x, y)
    (void) m_vertex_vbo.bind();
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    f->glEnableVertexAttribArray(0);
    m_vertex_vbo.release();

    // Attribute 1: line_id uint
    (void) m_line_id_vbo.bind();
    auto * ef = GLFunctions::getExtra();
    if (ef) {
        ef->glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(std::uint32_t), nullptr);
    }
    f->glEnableVertexAttribArray(1);
    m_line_id_vbo.release();

    m_vao.release();
}

// ── Private: rendering ─────────────────────────────────────────────────

void BatchLineRenderer::renderLines(glm::mat4 const & mvp)
{
    auto * f = GLFunctions::get();
    if (!f) {
        return;
    }

    // Get shader program
    QOpenGLShaderProgram * shader = nullptr;
    if (m_use_shader_manager) {
        char const * name = m_use_ssbos ? SHADER_43_NAME : SHADER_41_NAME;
        auto * sp = ShaderManager::instance().getProgram(name);
        if (!sp) {
            return;
        }
        sp->use();
        shader = sp->getNativeProgram();
    } else {
        if (!m_embedded_shader.bind()) {
            return;
        }
        shader = m_embedded_shader.native();
    }

    if (!shader) {
        return;
    }

    // Set uniforms
    shader->setUniformValue("u_mvp_matrix", QMatrix4x4(glm::value_ptr(mvp)).transposed());
    shader->setUniformValue("u_color",
        m_global_color.r, m_global_color.g, m_global_color.b, m_global_color.a);
    shader->setUniformValue("u_hover_color",
        m_hover_color.r, m_hover_color.g, m_hover_color.b, m_hover_color.a);
    shader->setUniformValue("u_selected_color",
        m_selected_color.r, m_selected_color.g, m_selected_color.b, m_selected_color.a);
    shader->setUniformValue("u_line_width", m_line_width);
    shader->setUniformValue("u_viewport_size", m_viewport_size.x, m_viewport_size.y);
    shader->setUniformValue("u_canvas_size", m_canvas_size.x, m_canvas_size.y);
    shader->setUniformValue("u_hover_line_id", static_cast<GLuint>(0)); // No hover in main pass

    // Bind SSBOs for the geometry shader (GL 4.3 only)
    if (m_use_ssbos) {
        m_store.bindForRender();
    }

    // Draw all segments
    (void) m_vao.bind();
    f->glDrawArrays(GL_LINES, 0, m_total_vertices);
    m_vao.release();

    if (m_use_shader_manager) {
        // ShaderManager programs stay bound until next use()
    } else {
        m_embedded_shader.release();
    }
}

void BatchLineRenderer::renderHoverLine(glm::mat4 const & mvp)
{
    if (!m_hover_line_index.has_value()) {
        return;
    }

    auto const & cpu = m_store.cpuData();
    auto const idx = m_hover_line_index.value();
    if (idx >= cpu.numLines()) {
        return;
    }

    auto const & info = cpu.lines[idx];
    if (info.segment_count == 0) {
        return;
    }

    auto * f = GLFunctions::get();
    if (!f) {
        return;
    }

    // Get shader program
    QOpenGLShaderProgram * shader = nullptr;
    if (m_use_shader_manager) {
        char const * name = m_use_ssbos ? SHADER_43_NAME : SHADER_41_NAME;
        auto * sp = ShaderManager::instance().getProgram(name);
        if (!sp) {
            return;
        }
        sp->use();
        shader = sp->getNativeProgram();
    } else {
        if (!m_embedded_shader.bind()) {
            return;
        }
        shader = m_embedded_shader.native();
    }

    if (!shader) {
        return;
    }

    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set uniforms (same as main pass but with hover active)
    shader->setUniformValue("u_mvp_matrix", QMatrix4x4(glm::value_ptr(mvp)).transposed());
    shader->setUniformValue("u_color",
        m_global_color.r, m_global_color.g, m_global_color.b, m_global_color.a);
    shader->setUniformValue("u_hover_color",
        m_hover_color.r, m_hover_color.g, m_hover_color.b, m_hover_color.a);
    shader->setUniformValue("u_selected_color",
        m_selected_color.r, m_selected_color.g, m_selected_color.b, m_selected_color.a);
    shader->setUniformValue("u_line_width", m_line_width);
    shader->setUniformValue("u_viewport_size", m_viewport_size.x, m_viewport_size.y);
    shader->setUniformValue("u_canvas_size", m_canvas_size.x, m_canvas_size.y);

    // Set hover line ID (1-based to match shader convention)
    shader->setUniformValue("u_hover_line_id", static_cast<GLuint>(idx + 1));

    if (m_use_ssbos) {
        m_store.bindForRender();
    }

    // Draw only the hovered line's segments
    // Each segment has 2 vertices, so vertex offset = first_segment * 2
    auto const start_vertex = static_cast<int>(info.first_segment * 2);
    auto const vertex_count = static_cast<int>(info.segment_count * 2);

    (void) m_vao.bind();
    f->glDrawArrays(GL_LINES, start_vertex, vertex_count);
    m_vao.release();

    // Reset hover
    shader->setUniformValue("u_hover_line_id", static_cast<GLuint>(0));

    f->glDisable(GL_BLEND);

    if (!m_use_shader_manager) {
        m_embedded_shader.release();
    }
}

} // namespace PlottingOpenGL
