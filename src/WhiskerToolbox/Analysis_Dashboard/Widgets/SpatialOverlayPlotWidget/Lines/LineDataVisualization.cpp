#include "LineDataVisualization.hpp"

#include "DataManager/Lines/Line_Data.hpp"
#include "ShaderManager/ShaderManager.hpp"


#include <QDebug>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cmath>

LineDataVisualization::LineDataVisualization(QString const & data_key, std::shared_ptr<LineData> const & line_data)
    : key(data_key),
      picking_framebuffer(nullptr),
      line_shader_program(nullptr),
      picking_shader_program(nullptr) {

    initializeOpenGLFunctions();

    // Set default color (can be customized later)
    color = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);

    // Build vertex data from LineData
    buildVertexData(line_data);

    // Initialize OpenGL resources
    initializeOpenGLResources();
}

LineDataVisualization::~LineDataVisualization() {
    cleanupOpenGLResources();
}

void LineDataVisualization::buildVertexData(std::shared_ptr<LineData> const & line_data) {
    vertex_data.clear();
    line_offsets.clear();
    line_lengths.clear();
    line_identifiers.clear();

    if (!line_data) {
        return;
    }

    uint32_t current_offset = 0;
    uint32_t line_index = 0;

    // Iterate through all time frames and lines
    for (auto const & [time_frame, lines]: line_data->GetAllLinesAsRange()) {
        for (int line_id = 0; line_id < static_cast<int>(lines.size()); ++line_id) {
            Line2D const & line = lines[line_id];

            if (line.empty()) {
                continue;
            }

            // Store line identifier
            line_identifiers.push_back({time_frame.getValue(), line_id});

            // Store line offset and length
            line_offsets.push_back(current_offset);
            line_lengths.push_back(static_cast<uint32_t>(line.size()));

            // Add vertices for this line
            for (Point2D<float> const & point: line) {
                vertex_data.push_back(point.x);
                vertex_data.push_back(point.y);
            }

            current_offset += static_cast<uint32_t>(line.size());
            line_index++;
        }
    }

    qDebug() << "LineDataVisualization: Built vertex data for" << line_identifiers.size()
             << "lines with" << vertex_data.size() / 2 << "vertices";
}

void LineDataVisualization::initializeOpenGLResources() {
    // Create vertex buffer
    vertex_buffer.create();
    vertex_buffer.bind();
    vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));
    vertex_buffer.release();

    // Create vertex array object
    vertex_array_object.create();
    vertex_array_object.bind();

    // Set up vertex attributes
    vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Create line ID buffer (for geometry shader)
    QOpenGLBuffer line_id_buffer;
    line_id_buffer.create();
    line_id_buffer.bind();

    // Create line ID data (each vertex gets the line index)
    std::vector<uint32_t> line_id_data;
    for (size_t i = 0; i < line_identifiers.size(); ++i) {
        uint32_t line_length = line_lengths[i];
        for (uint32_t j = 0; j < line_length; ++j) {
            line_id_data.push_back(static_cast<uint32_t>(i));
        }
    }

    line_id_buffer.allocate(line_id_data.data(), static_cast<int>(line_id_data.size() * sizeof(uint32_t)));
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);

    line_id_buffer.release();
    vertex_buffer.release();
    vertex_array_object.release();

    // Create picking framebuffer
    QOpenGLFramebufferObjectFormat format;
    //format.setAttachment(QOpenGLFramebufferObject::GL_COLOR_ATTACHMENT0);
    format.setInternalTextureFormat(GL_RGBA8);

    picking_framebuffer = new QOpenGLFramebufferObject(1024, 1024, format);

    // Create picking vertex buffer and VAO
    picking_vertex_buffer.create();
    picking_vertex_buffer.bind();
    picking_vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));
    picking_vertex_buffer.release();

    picking_vertex_array_object.create();
    picking_vertex_array_object.bind();

    picking_vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    line_id_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);

    line_id_buffer.release();
    picking_vertex_buffer.release();
    picking_vertex_array_object.release();

    // Load shader programs
    ShaderManager & shader_manager = ShaderManager::instance();

    // Load line rendering shader
    if (!shader_manager.getProgram("line_with_geometry")) {
        shader_manager.loadProgram("line_with_geometry",
                                   "shaders/line_with_geometry.vert",
                                   "shaders/line_with_geometry.frag",
                                   "shaders/line_with_geometry.geom");
    }
    line_shader_program = shader_manager.getProgram("line_with_geometry")->getNativeProgram();

    // Load picking shader
    if (!shader_manager.getProgram("line_picking")) {
        shader_manager.loadProgram("line_picking",
                                   "shaders/line_picking.vert",
                                   "shaders/line_picking.frag",
                                   "shaders/line_picking.geom");
    }
    picking_shader_program = shader_manager.getProgram("line_picking")->getNativeProgram();
}

void LineDataVisualization::cleanupOpenGLResources() {
    if (vertex_buffer.isCreated()) {
        vertex_buffer.destroy();
    }

    if (vertex_array_object.isCreated()) {
        vertex_array_object.destroy();
    }

    if (picking_vertex_buffer.isCreated()) {
        picking_vertex_buffer.destroy();
    }

    if (picking_vertex_array_object.isCreated()) {
        picking_vertex_array_object.destroy();
    }

    if (picking_framebuffer) {
        delete picking_framebuffer;
        picking_framebuffer = nullptr;
    }
}

void LineDataVisualization::renderLines(QOpenGLShaderProgram * shader_program, float line_width) {
    if (!visible || vertex_data.empty() || !shader_program) {
        return;
    }

    shader_program->bind();

    // Set uniforms
    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));   // Yellow for hover
    shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black for selected
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));// TODO: Get actual viewport

    // Set hover state
    if (has_hover_line) {
        // Find the line index for the hovered line
        auto it = std::find_if(line_identifiers.begin(), line_identifiers.end(),
                               [this](LineIdentifier const & id) {
                                   return id == current_hover_line;
                               });
        if (it != line_identifiers.end()) {
            uint32_t line_index = static_cast<uint32_t>(std::distance(line_identifiers.begin(), it));
            shader_program->setUniformValue("u_hover_line_id", line_index);
            shader_program->setUniformValue("u_is_hovered", true);
        } else {
            shader_program->setUniformValue("u_is_hovered", false);
        }
    } else {
        shader_program->setUniformValue("u_is_hovered", false);
    }

    shader_program->setUniformValue("u_is_selected", false);// TODO: Implement selection

    // Bind vertex array object
    vertex_array_object.bind();

    // Render lines using line strips
    for (size_t i = 0; i < line_identifiers.size(); ++i) {
        uint32_t offset = line_offsets[i];
        uint32_t length = line_lengths[i];

        if (length > 1) {
            glDrawArrays(GL_LINE_STRIP, offset, length);
        }
    }

    vertex_array_object.release();
    shader_program->release();
}

void LineDataVisualization::renderLinesToPickingBuffer(float line_width) {
    if (!visible || vertex_data.empty() || !picking_shader_program || !picking_framebuffer) {
        return;
    }

    // Bind picking framebuffer
    picking_framebuffer->bind();

    // Clear framebuffer
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    picking_shader_program->bind();

    // Set uniforms
    picking_shader_program->setUniformValue("u_line_width", line_width);
    picking_shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));// TODO: Get actual viewport

    // Bind picking vertex array object
    picking_vertex_array_object.bind();

    // Render lines using line strips
    for (size_t i = 0; i < line_identifiers.size(); ++i) {
        uint32_t offset = line_offsets[i];
        uint32_t length = line_lengths[i];

        if (length > 1) {
            glDrawArrays(GL_LINE_STRIP, offset, length);
        }
    }

    picking_vertex_array_object.release();
    picking_shader_program->release();

    // Unbind framebuffer
    picking_framebuffer->release();
}

std::optional<LineIdentifier> LineDataVisualization::getLineAtScreenPosition(int screen_x, int screen_y) {
    if (!picking_framebuffer || !picking_shader_program) {
        return std::nullopt;
    }

    // Render to picking buffer
    const_cast<LineDataVisualization *>(this)->renderLinesToPickingBuffer(5.0f);// Use reasonable line width

    // Read pixel at screen position
    picking_framebuffer->bind();

    // Convert screen coordinates to framebuffer coordinates
    int fb_x = static_cast<int>((screen_x / 1024.0f) * picking_framebuffer->width());
    int fb_y = static_cast<int>((screen_y / 1024.0f) * picking_framebuffer->height());

    // Clamp coordinates
    fb_x = std::max(0, std::min(fb_x, picking_framebuffer->width() - 1));
    fb_y = std::max(0, std::min(fb_y, picking_framebuffer->height() - 1));

    // Read pixel
    unsigned char pixel[4];
    glReadPixels(fb_x, fb_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    picking_framebuffer->release();

    // Convert pixel color back to line ID
    uint32_t line_id = (static_cast<uint32_t>(pixel[0]) << 16) |
                       (static_cast<uint32_t>(pixel[1]) << 8) |
                       static_cast<uint32_t>(pixel[2]);

    // Check if we found a valid line
    if (line_id < line_identifiers.size()) {
        return line_identifiers[line_id];
    }

    return std::nullopt;
}

void LineDataVisualization::setHoverLine(std::optional<LineIdentifier> line_id) {
    if (line_id.has_value()) {
        current_hover_line = line_id.value();
        has_hover_line = true;
    } else {
        has_hover_line = false;
    }
}

std::optional<LineIdentifier> LineDataVisualization::getHoverLine() const {
    if (has_hover_line) {
        return current_hover_line;
    }
    return std::nullopt;
}

BoundingBox LineDataVisualization::calculateBoundsForLineData(LineData const * line_data) const {
    if (!line_data) {
        return BoundingBox(0, 0, 0, 0);
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();

    bool has_data = false;

    for (auto const & [time_frame, lines]: line_data->GetAllLinesAsRange()) {
        for (Line2D const & line: lines) {
            for (Point2D<float> const & point: line) {
                if (!has_data) {
                    min_x = max_x = point.x;
                    min_y = max_y = point.y;
                    has_data = true;
                } else {
                    min_x = std::min(min_x, point.x);
                    max_x = std::max(max_x, point.x);
                    min_y = std::min(min_y, point.y);
                    max_y = std::max(max_y, point.y);
                }
            }
        }
    }

    return BoundingBox(min_x, min_y, max_x, max_y);
}