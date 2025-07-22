#include "LineDataVisualization.hpp"

#include "DataManager/Lines/Line_Data.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "ShaderManager/ShaderSourceType.hpp"


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

    // Get canvas size for coordinate normalization
    ImageSize image_size = line_data->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        qDebug() << "Invalid image size for LineData, using media data instead";
    }
    if (image_size.width <= 0 || image_size.height <= 0) {
        qDebug() << "Using default canvas size 640x480 for LineData";
        image_size = {640, 480}; // Fallback to a default size
    }
    canvas_size = QVector2D(static_cast<float>(image_size.width), static_cast<float>(image_size.height));
    qDebug() << "Canvas size:" << canvas_size.x() << "x" << canvas_size.y();

    // We'll create line segments (pairs of vertices) for the geometry shader
    // Each line segment gets a line ID for picking/hovering
    std::vector<float> segment_vertices;      // All line segments as pairs of vertices
    std::vector<uint32_t> segment_line_ids;  // Line ID for each vertex in segments

    uint32_t line_index = 0;

    // Iterate through all time frames and lines
    for (auto const & [time_frame, lines]: line_data->GetAllLinesAsRange()) {
        for (int line_id = 0; line_id < static_cast<int>(lines.size()); ++line_id) {
            Line2D const & line = lines[line_id];

            if (line.size() < 2) {  // Need at least 2 points for a line
                continue;
            }

            // Store line identifier
            line_identifiers.push_back({time_frame.getValue(), line_id});

            // Convert line strip to line segments (pairs of consecutive vertices)
            for (size_t i = 0; i < line.size() - 1; ++i) {
                Point2D<float> const & p0 = line[i];
                Point2D<float> const & p1 = line[i + 1];

                // Add first vertex of segment
                segment_vertices.push_back(p0.x);
                segment_vertices.push_back(p0.y);
                segment_line_ids.push_back(line_index);

                // Add second vertex of segment
                segment_vertices.push_back(p1.x);
                segment_vertices.push_back(p1.y);
                segment_line_ids.push_back(line_index);
            }

            line_index++;
        }
    }

    // Store the processed data
    vertex_data = std::move(segment_vertices);
    line_id_data = std::move(segment_line_ids);

    qDebug() << "LineDataVisualization: Built" << line_identifiers.size() 
             << "lines with" << vertex_data.size() / 4 << "segments (" 
             << vertex_data.size() / 2 << "vertices)";
    
    // Debug: print coordinate range
    if (!vertex_data.empty()) {
        float min_x = vertex_data[0], max_x = vertex_data[0];
        float min_y = vertex_data[1], max_y = vertex_data[1];
        for (size_t i = 0; i < vertex_data.size(); i += 2) {
            min_x = std::min(min_x, vertex_data[i]);
            max_x = std::max(max_x, vertex_data[i]);
            min_y = std::min(min_y, vertex_data[i + 1]);
            max_y = std::max(max_y, vertex_data[i + 1]);
        }
        qDebug() << "Vertex coordinate range: X[" << min_x << "," << max_x << "] Y[" << min_y << "," << max_y << "]";
        
        // Check if coordinates are in expected range for OpenGL
        if (min_x < -10.0f || max_x > 10.0f || min_y < -10.0f || max_y > 10.0f) {
            qDebug() << "WARNING: Coordinates appear to be outside typical OpenGL range [-1,1]";
            qDebug() << "You may need coordinate transformation/normalization for proper display";
        }
    }
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
    line_id_buffer.create();
    line_id_buffer.bind();

    // Use the pre-computed line ID data
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
        bool success = shader_manager.loadProgram("line_with_geometry", 
                                 ":/shaders/line_with_geometry.vert",
                                 ":/shaders/line_with_geometry.frag",
                                 ":/shaders/line_with_geometry.geom",
                                 ShaderSourceType::Resource);
        if (!success) {
            qDebug() << "Failed to load line_with_geometry shader!";
        }
    }
    auto* line_program = shader_manager.getProgram("line_with_geometry");
    if (line_program) {
        line_shader_program = line_program->getNativeProgram();
        qDebug() << "Successfully loaded line_with_geometry shader";
    } else {
        qDebug() << "line_with_geometry shader is null!";
        line_shader_program = nullptr;
    }
    
    // Load picking shader
    if (!shader_manager.getProgram("line_picking")) {
        bool success = shader_manager.loadProgram("line_picking",
                                 ":/shaders/line_picking.vert",
                                 ":/shaders/line_picking.frag",
                                 ":/shaders/line_picking.geom",
                                 ShaderSourceType::Resource);
        if (!success) {
            qDebug() << "Failed to load line_picking shader!";
        }
    }
    auto* picking_program = shader_manager.getProgram("line_picking");
    if (picking_program) {
        picking_shader_program = picking_program->getNativeProgram();
        qDebug() << "Successfully loaded line_picking shader";
    } else {
        qDebug() << "line_picking shader is null!";
        picking_shader_program = nullptr;
    }
}

void LineDataVisualization::cleanupOpenGLResources() {
    if (vertex_buffer.isCreated()) {
        vertex_buffer.destroy();
    }

    if (line_id_buffer.isCreated()) {
        line_id_buffer.destroy();
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
        qDebug() << "LineDataVisualization::renderLines: Skipping render. visible=" << visible 
                 << "vertex_data.size()=" << vertex_data.size() 
                 << "shader_program=" << (shader_program ? "valid" : "null");
        return;
    }

    static bool first_render = true;
    if (first_render) {
        qDebug() << "LineDataVisualization::renderLines: Rendering" << line_identifiers.size() << "lines";
        first_render = false;
    }

    shader_program->bind();

    // Set uniforms
    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));   // Yellow for hover
    shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black for selected
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));// TODO: Get actual viewport
    shader_program->setUniformValue("u_canvas_size", canvas_size);  // For coordinate normalization

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

    // Render ALL line segments in a single draw call - MASSIVE performance improvement!
    // The geometry shader will convert each pair of vertices into a thick line quad
    if (!vertex_data.empty()) {
        uint32_t total_vertices = static_cast<uint32_t>(vertex_data.size() / 2);
        if (first_render) {
            qDebug() << "Drawing" << total_vertices / 2 << "line segments in single draw call";
        }
        glDrawArrays(GL_LINES, 0, total_vertices);
    }

    vertex_array_object.release();
    shader_program->release();
}

void LineDataVisualization::renderLines(float line_width) {
    if (line_shader_program) {
        renderLines(line_shader_program, line_width);
    } else {
        qDebug() << "Cannot render lines: line_shader_program is null (shader compilation failed?)";
    }
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
    picking_shader_program->setUniformValue("u_canvas_size", canvas_size);  // For coordinate normalization

    // Bind picking vertex array object
    picking_vertex_array_object.bind();

    // Render ALL line segments in a single draw call for picking
    if (!vertex_data.empty()) {
        uint32_t total_vertices = static_cast<uint32_t>(vertex_data.size() / 2);
        glDrawArrays(GL_LINES, 0, total_vertices);
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