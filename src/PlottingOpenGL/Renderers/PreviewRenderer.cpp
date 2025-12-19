#include "PreviewRenderer.hpp"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cmath>

namespace PlottingOpenGL {

PreviewRenderer::PreviewRenderer() = default;

PreviewRenderer::~PreviewRenderer() {
    cleanup();
}

bool PreviewRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

    // Compile shaders
    if (!m_fill_shader.createFromSource(PreviewShaders::VERTEX_SHADER,
                                         PreviewShaders::FRAGMENT_SHADER)) {
        return false;
    }

    // Use same shaders for lines (just different draw mode)
    if (!m_line_shader.createFromSource(PreviewShaders::VERTEX_SHADER,
                                         PreviewShaders::FRAGMENT_SHADER)) {
        m_fill_shader.destroy();
        return false;
    }

    // Create VAO
    if (!m_vao.create()) {
        m_fill_shader.destroy();
        m_line_shader.destroy();
        return false;
    }

    // Create vertex buffer
    if (!m_vertex_buffer.create()) {
        m_vao.destroy();
        m_fill_shader.destroy();
        m_line_shader.destroy();
        return false;
    }

    // Set up VAO with vertex attribute layout
    (void)m_vao.bind();
    (void)m_vertex_buffer.bind();

    auto * gl = QOpenGLContext::currentContext()->extraFunctions();
    // Position attribute at location 0, 2 floats per vertex
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl->glEnableVertexAttribArray(0);

    m_vertex_buffer.release();
    m_vao.release();

    m_initialized = true;
    return true;
}

void PreviewRenderer::cleanup() {
    if (!m_initialized) {
        return;
    }

    m_vertex_buffer.destroy();
    m_vao.destroy();
    m_line_shader.destroy();
    m_fill_shader.destroy();
    m_initialized = false;
}

void PreviewRenderer::render(CorePlotting::Interaction::GlyphPreview const & preview,
                              int viewport_width,
                              int viewport_height) {
    if (!m_initialized || !preview.isValid()) {
        return;
    }

    auto * gl = QOpenGLContext::currentContext()->extraFunctions();

    // Create orthographic projection for screen-space rendering
    // Origin at top-left, Y pointing down (canvas coordinates)
    glm::mat4 const ortho = glm::ortho(
        0.0f, static_cast<float>(viewport_width),   // left, right
        static_cast<float>(viewport_height), 0.0f,  // bottom, top (flipped for Y-down)
        -1.0f, 1.0f                                  // near, far
    );

    // Enable blending for transparency
    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Dispatch based on preview type
    switch (preview.type) {
        case CorePlotting::Interaction::GlyphPreview::Type::Rectangle:
            renderRectangle(preview, ortho);
            break;
        case CorePlotting::Interaction::GlyphPreview::Type::Line:
            renderLine(preview, ortho);
            break;
        case CorePlotting::Interaction::GlyphPreview::Type::Point:
            renderPoint(preview, ortho);
            break;
        case CorePlotting::Interaction::GlyphPreview::Type::Polygon:
            renderPolygon(preview, ortho);
            break;
        case CorePlotting::Interaction::GlyphPreview::Type::None:
            // Nothing to render
            break;
    }

    gl->glDisable(GL_BLEND);
}

void PreviewRenderer::renderRectangle(CorePlotting::Interaction::GlyphPreview const & preview,
                                       glm::mat4 const & ortho) {
    // Render ghost of original position first (if modifying)
    if (preview.show_ghost && preview.original_rectangle.has_value()) {
        glm::vec4 const & orig = *preview.original_rectangle;
        renderSingleRectangle(orig, 
                              preview.ghost_color,      // ghost fill
                              preview.ghost_color,      // ghost stroke
                              preview.stroke_width,
                              true,                      // show fill
                              true,                      // show stroke
                              ortho);
    }

    // Render current rectangle
    renderSingleRectangle(preview.rectangle,
                          preview.fill_color,
                          preview.stroke_color,
                          preview.stroke_width,
                          preview.show_fill,
                          preview.show_stroke,
                          ortho);
}

void PreviewRenderer::renderLine(CorePlotting::Interaction::GlyphPreview const & preview,
                                  glm::mat4 const & ortho) {
    // Render ghost of original position first (if modifying)
    if (preview.show_ghost && preview.original_line.has_value()) {
        auto const & [start, end] = *preview.original_line;
        renderSingleLine(start, end, preview.ghost_color, preview.stroke_width, ortho);
    }

    // Render current line
    renderSingleLine(preview.line_start, preview.line_end, 
                     preview.stroke_color, preview.stroke_width, ortho);
}

void PreviewRenderer::renderPoint(CorePlotting::Interaction::GlyphPreview const & preview,
                                   glm::mat4 const & ortho) {
    float const point_radius = preview.stroke_width * 2.0f; // Make points visible

    // Render ghost of original position first (if modifying)
    if (preview.show_ghost && preview.original_point.has_value()) {
        renderSinglePoint(*preview.original_point, preview.ghost_color, point_radius, ortho);
    }

    // Render current point
    renderSinglePoint(preview.point, preview.stroke_color, point_radius, ortho);
}

void PreviewRenderer::renderPolygon(CorePlotting::Interaction::GlyphPreview const & preview,
                                     glm::mat4 const & ortho) {
    if (preview.polygon_vertices.size() < 3) {
        return;
    }

    auto * gl = QOpenGLContext::currentContext()->extraFunctions();

    // For polygons, we'll use a simple triangle fan from centroid
    // This is a basic approach - complex concave polygons may need triangulation
    
    // Calculate centroid
    glm::vec2 centroid{0.0f};
    for (auto const & v : preview.polygon_vertices) {
        centroid += v;
    }
    centroid /= static_cast<float>(preview.polygon_vertices.size());

    // Build triangle fan vertices: centroid, then all vertices, then first vertex again
    std::vector<float> vertices;
    vertices.reserve((preview.polygon_vertices.size() + 2) * 2);
    
    vertices.push_back(centroid.x);
    vertices.push_back(centroid.y);
    
    for (auto const & v : preview.polygon_vertices) {
        vertices.push_back(v.x);
        vertices.push_back(v.y);
    }
    
    // Close the fan
    vertices.push_back(preview.polygon_vertices[0].x);
    vertices.push_back(preview.polygon_vertices[0].y);

    // Upload and render fill
    if (preview.show_fill) {
        (void)m_vao.bind();
        (void)m_vertex_buffer.bind();
        m_vertex_buffer.allocate(vertices.data(), 
                                  static_cast<int>(vertices.size() * sizeof(float)));
        m_vertex_buffer.release();

        (void)m_fill_shader.bind();
        m_fill_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
        m_fill_shader.setUniformValue("u_color", 
                                       preview.fill_color.r, preview.fill_color.g,
                                       preview.fill_color.b, preview.fill_color.a);

        gl->glDrawArrays(GL_TRIANGLE_FAN, 0, 
                         static_cast<int>(preview.polygon_vertices.size() + 2));
        
        m_fill_shader.release();
        m_vao.release();
    }

    // Render outline
    if (preview.show_stroke) {
        // Build line loop vertices (just the polygon vertices)
        std::vector<float> outline_verts;
        outline_verts.reserve(preview.polygon_vertices.size() * 2);
        for (auto const & v : preview.polygon_vertices) {
            outline_verts.push_back(v.x);
            outline_verts.push_back(v.y);
        }

        (void)m_vao.bind();
        (void)m_vertex_buffer.bind();
        m_vertex_buffer.allocate(outline_verts.data(),
                                  static_cast<int>(outline_verts.size() * sizeof(float)));
        m_vertex_buffer.release();

        (void)m_line_shader.bind();
        m_line_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
        m_line_shader.setUniformValue("u_color",
                                       preview.stroke_color.r, preview.stroke_color.g,
                                       preview.stroke_color.b, preview.stroke_color.a);

        gl->glLineWidth(preview.stroke_width);
        gl->glDrawArrays(GL_LINE_LOOP, 0,
                         static_cast<int>(preview.polygon_vertices.size()));

        m_line_shader.release();
        m_vao.release();
    }
}

void PreviewRenderer::renderSingleRectangle(glm::vec4 const & bounds,
                                             glm::vec4 const & fill_color,
                                             glm::vec4 const & stroke_color,
                                             float stroke_width,
                                             bool show_fill,
                                             bool show_stroke,
                                             glm::mat4 const & ortho) {
    auto * gl = QOpenGLContext::currentContext()->extraFunctions();

    float const x = bounds.x;
    float const y = bounds.y;
    float const w = bounds.z;
    float const h = bounds.w;

    // Build quad vertices (two triangles)
    std::array<float, 12> const fill_vertices = {
        x,     y,      // top-left
        x + w, y,      // top-right
        x,     y + h,  // bottom-left
        x + w, y,      // top-right
        x + w, y + h,  // bottom-right
        x,     y + h   // bottom-left
    };

    // Render fill
    if (show_fill) {
        (void)m_vao.bind();
        (void)m_vertex_buffer.bind();
        m_vertex_buffer.allocate(fill_vertices.data(), 
                                  static_cast<int>(fill_vertices.size() * sizeof(float)));
        m_vertex_buffer.release();

        (void)m_fill_shader.bind();
        m_fill_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
        m_fill_shader.setUniformValue("u_color", 
                                       fill_color.r, fill_color.g, 
                                       fill_color.b, fill_color.a);

        gl->glDrawArrays(GL_TRIANGLES, 0, 6);

        m_fill_shader.release();
        m_vao.release();
    }

    // Render stroke/outline
    if (show_stroke) {
        std::array<float, 8> const stroke_vertices = {
            x,     y,      // top-left
            x + w, y,      // top-right
            x + w, y + h,  // bottom-right
            x,     y + h   // bottom-left
        };

        (void)m_vao.bind();
        (void)m_vertex_buffer.bind();
        m_vertex_buffer.allocate(stroke_vertices.data(),
                                  static_cast<int>(stroke_vertices.size() * sizeof(float)));
        m_vertex_buffer.release();

        (void)m_line_shader.bind();
        m_line_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
        m_line_shader.setUniformValue("u_color",
                                       stroke_color.r, stroke_color.g,
                                       stroke_color.b, stroke_color.a);

        gl->glLineWidth(stroke_width);
        gl->glDrawArrays(GL_LINE_LOOP, 0, 4);

        m_line_shader.release();
        m_vao.release();
    }
}

void PreviewRenderer::renderSingleLine(glm::vec2 const & start,
                                        glm::vec2 const & end,
                                        glm::vec4 const & color,
                                        float width,
                                        glm::mat4 const & ortho) {
    auto * gl = QOpenGLContext::currentContext()->extraFunctions();

    std::array<float, 4> const vertices = {
        start.x, start.y,
        end.x,   end.y
    };

    (void)m_vao.bind();
    (void)m_vertex_buffer.bind();
    m_vertex_buffer.allocate(vertices.data(),
                              static_cast<int>(vertices.size() * sizeof(float)));
    m_vertex_buffer.release();

    (void)m_line_shader.bind();
    m_line_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
    m_line_shader.setUniformValue("u_color", color.r, color.g, color.b, color.a);

    gl->glLineWidth(width);
    gl->glDrawArrays(GL_LINES, 0, 2);

    m_line_shader.release();
    m_vao.release();
}

void PreviewRenderer::renderSinglePoint(glm::vec2 const & pos,
                                         glm::vec4 const & color,
                                         float radius,
                                         glm::mat4 const & ortho) {
    auto * gl = QOpenGLContext::currentContext()->extraFunctions();

    // Render point as a small filled circle using triangle fan
    constexpr int num_segments = 16;
    std::array<float, (num_segments + 2) * 2> vertices;

    // Center point
    vertices[0] = pos.x;
    vertices[1] = pos.y;

    // Circle vertices
    for (int i = 0; i <= num_segments; ++i) {
        float const angle = 2.0f * static_cast<float>(M_PI) * 
                           static_cast<float>(i) / static_cast<float>(num_segments);
        vertices[static_cast<size_t>(2 + i * 2)] = pos.x + radius * std::cos(angle);
        vertices[static_cast<size_t>(3 + i * 2)] = pos.y + radius * std::sin(angle);
    }

    (void)m_vao.bind();
    (void)m_vertex_buffer.bind();
    m_vertex_buffer.allocate(vertices.data(),
                              static_cast<int>(vertices.size() * sizeof(float)));
    m_vertex_buffer.release();

    (void)m_fill_shader.bind();
    m_fill_shader.setUniformMatrix4("u_ortho_matrix", glm::value_ptr(ortho));
    m_fill_shader.setUniformValue("u_color", color.r, color.g, color.b, color.a);

    gl->glDrawArrays(GL_TRIANGLE_FAN, 0, num_segments + 2);

    m_fill_shader.release();
    m_vao.release();
}

} // namespace PlottingOpenGL
