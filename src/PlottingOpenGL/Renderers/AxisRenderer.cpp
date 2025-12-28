#include "AxisRenderer.hpp"

#include "ShaderManager/ShaderManager.hpp"

#include <QOpenGLFunctions>

#include <array>
#include <iostream>

namespace PlottingOpenGL {

AxisRenderer::AxisRenderer() = default;

AxisRenderer::~AxisRenderer() {
    cleanup();
}

bool AxisRenderer::initialize() {
    if (m_initialized) {
        return true;
    }

    // Create VAO and VBO
    if (!m_vao.create()) {
        std::cerr << "AxisRenderer: Failed to create VAO" << std::endl;
        return false;
    }

    if (!m_vbo.create()) {
        std::cerr << "AxisRenderer: Failed to create VBO" << std::endl;
        return false;
    }

    // Get uniform locations from axes shader
    auto * axes_program = ShaderManager::instance().getProgram("axes");
    if (axes_program && axes_program->getNativeProgram()) {
        auto * native = axes_program->getNativeProgram();
        m_axes_proj_loc = native->uniformLocation("projMatrix");
        m_axes_view_loc = native->uniformLocation("viewMatrix");
        m_axes_model_loc = native->uniformLocation("modelMatrix");
        m_axes_color_loc = native->uniformLocation("u_color");
        m_axes_alpha_loc = native->uniformLocation("u_alpha");
    } else {
        std::cerr << "AxisRenderer: 'axes' shader not found in ShaderManager" << std::endl;
    }

    // Get uniform locations from dashed line shader
    auto * dashed_program = ShaderManager::instance().getProgram("dashed_line");
    if (dashed_program && dashed_program->getNativeProgram()) {
        auto * native = dashed_program->getNativeProgram();
        m_dashed_mvp_loc = native->uniformLocation("u_mvp");
        m_dashed_resolution_loc = native->uniformLocation("u_resolution");
        m_dashed_dash_size_loc = native->uniformLocation("u_dashSize");
        m_dashed_gap_size_loc = native->uniformLocation("u_gapSize");
    } else {
        std::cerr << "AxisRenderer: 'dashed_line' shader not found in ShaderManager" << std::endl;
    }

    m_initialized = true;
    return true;
}

void AxisRenderer::cleanup() {
    if (!m_initialized) {
        return;
    }

    m_vbo.destroy();
    m_vao.destroy();

    m_initialized = false;
}

bool AxisRenderer::isInitialized() const {
    return m_initialized;
}

void AxisRenderer::renderAxis(AxisConfig const & config,
                               glm::mat4 const & view,
                               glm::mat4 const & projection) {
    if (!m_initialized) {
        return;
    }

    auto * axes_program = ShaderManager::instance().getProgram("axes");
    if (!axes_program) {
        return;
    }

    // Get OpenGL functions
    QOpenGLFunctions * gl = QOpenGLContext::currentContext()->functions();

    // Use the axes shader program
    gl->glUseProgram(axes_program->getProgramId());

    // Set uniforms - convert glm matrices to float arrays
    gl->glUniformMatrix4fv(m_axes_proj_loc, 1, GL_FALSE, &projection[0][0]);
    gl->glUniformMatrix4fv(m_axes_view_loc, 1, GL_FALSE, &view[0][0]);
    
    // Model matrix is identity for axis rendering
    glm::mat4 const model{1.0f};
    gl->glUniformMatrix4fv(m_axes_model_loc, 1, GL_FALSE, &model[0][0]);

    // Set color and alpha
    gl->glUniform3f(m_axes_color_loc, config.color.r, config.color.g, config.color.b);
    gl->glUniform1f(m_axes_alpha_loc, config.alpha);

    // Bind VAO
    m_vao.bind();

    // Set up vertex data for vertical axis line (4D coordinates: x, y, 0, 1)
    std::array<float, 8> vertices = {
            config.x_position, config.y_min, 0.0f, 1.0f,
            config.x_position, config.y_max, 0.0f, 1.0f};

    // Upload and draw
    m_vbo.bind();
    m_vbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));

    // Configure vertex attribute (location 0, 4 floats per vertex)
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    // Set line width
    gl->glLineWidth(config.line_width);

    // Draw the line
    gl->glDrawArrays(GL_LINES, 0, 2);

    // Cleanup
    m_vbo.release();
    m_vao.release();
    gl->glUseProgram(0);
}

void AxisRenderer::renderGrid(GridConfig const & config,
                               glm::mat4 const & view,
                               glm::mat4 const & projection,
                               int viewport_width,
                               int viewport_height) {
    if (!m_initialized) {
        return;
    }

    // Validate configuration
    int64_t const time_range = config.time_end - config.time_start;
    if (time_range <= 0 || config.spacing <= 0) {
        return;
    }

    auto * dashed_program = ShaderManager::instance().getProgram("dashed_line");
    if (!dashed_program) {
        return;
    }

    // Compute combined MVP matrix (model is identity for grid)
    glm::mat4 const model{1.0f};
    glm::mat4 const mvp = projection * view * model;

    // Find the first grid line position that's >= time_start
    int64_t first_grid_time = ((config.time_start / config.spacing) * config.spacing);
    if (first_grid_time < config.time_start) {
        first_grid_time += config.spacing;
    }

    // Draw vertical grid lines at regular intervals
    for (int64_t grid_time = first_grid_time; grid_time <= config.time_end; grid_time += config.spacing) {
        // Convert time coordinate to normalized device coordinate (-1 to 1)
        float const normalized_x = 2.0f * static_cast<float>(grid_time - config.time_start) /
                                   static_cast<float>(time_range) - 1.0f;

        // Skip grid lines outside visible range (floating point precision)
        if (normalized_x < -1.0f || normalized_x > 1.0f) {
            continue;
        }

        renderDashedLine(
                normalized_x, config.y_min,
                normalized_x, config.y_max,
                mvp,
                viewport_width, viewport_height,
                config.dash_length, config.gap_length);
    }
}

void AxisRenderer::renderDashedLine(float x_start, float y_start,
                                     float x_end, float y_end,
                                     glm::mat4 const & mvp,
                                     int viewport_width, int viewport_height,
                                     float dash_length, float gap_length) {
    auto * dashed_program = ShaderManager::instance().getProgram("dashed_line");
    if (!dashed_program) {
        return;
    }

    QOpenGLFunctions * gl = QOpenGLContext::currentContext()->functions();

    gl->glUseProgram(dashed_program->getProgramId());

    // Set uniforms
    gl->glUniformMatrix4fv(m_dashed_mvp_loc, 1, GL_FALSE, &mvp[0][0]);
    
    std::array<float, 2> resolution = {
            static_cast<float>(viewport_width),
            static_cast<float>(viewport_height)};
    gl->glUniform2fv(m_dashed_resolution_loc, 1, resolution.data());
    gl->glUniform1f(m_dashed_dash_size_loc, dash_length);
    gl->glUniform1f(m_dashed_gap_size_loc, gap_length);

    // Bind VAO
    m_vao.bind();

    // Vertex data (3D coordinates as expected by dashed_line shader)
    std::array<float, 6> vertices = {
            x_start, y_start, 0.0f,
            x_end, y_end, 0.0f};

    // Upload and draw
    m_vbo.bind();
    m_vbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));

    // Configure vertex attribute (location 0, 3 floats per vertex)
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    gl->glDrawArrays(GL_LINES, 0, 2);

    m_vbo.release();
    m_vao.release();
    gl->glUseProgram(0);
}

}// namespace PlottingOpenGL
