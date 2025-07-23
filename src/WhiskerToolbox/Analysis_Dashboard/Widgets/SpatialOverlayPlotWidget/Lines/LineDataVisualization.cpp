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
    : m_line_data(line_data),
      key(data_key),
      scene_framebuffer(nullptr),
      picking_framebuffer(nullptr),
      line_shader_program(nullptr),
      picking_shader_program(nullptr),
      blit_shader_program(nullptr) {

    initializeOpenGLFunctions();

    color = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);

    buildVertexData(m_line_data.get());

    initializeOpenGLResources();
    m_dataIsDirty = false;// Data is clean after initial build and buffer creation
}

LineDataVisualization::~LineDataVisualization() {
    cleanupOpenGLResources();
}

void LineDataVisualization::buildVertexData(LineData const * line_data) {
    vertex_data.clear();
    line_offsets.clear();
    line_lengths.clear();
    line_identifiers.clear();
    line_vertex_ranges.clear();  // Clear vertex ranges too

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

            line_identifiers.push_back({time_frame.getValue(), line_id});
            
            // Record the starting vertex index for this line
            uint32_t line_start_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);

            // Convert line strip to line segments (pairs of consecutive vertices)
            for (size_t i = 0; i < line.size() - 1; ++i) {
                Point2D<float> const & p0 = line[i];
                Point2D<float> const & p1 = line[i + 1];

                // Add first vertex of segment
                segment_vertices.push_back(p0.x);
                segment_vertices.push_back(p0.y);
                segment_line_ids.push_back(line_index + 1);  // Use 1-based indexing for picking

                // Add second vertex of segment
                segment_vertices.push_back(p1.x);
                segment_vertices.push_back(p1.y);
                segment_line_ids.push_back(line_index + 1);  // Use 1-based indexing for picking
            }
            
            // Record the vertex count for this line
            uint32_t line_end_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);
            uint32_t line_vertex_count = line_end_vertex - line_start_vertex;
            
            // Store the range for efficient hover rendering
            line_vertex_ranges.push_back({line_start_vertex, line_vertex_count});
            
            //qDebug() << "Line" << line_index << "range: start=" << line_start_vertex << "count=" << line_vertex_count;

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
    line_id_buffer.create();

    vertex_array_object.create();
    vertex_array_object.bind();

    vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    line_id_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);

    line_id_buffer.release();
    vertex_buffer.release();
    vertex_array_object.release();

    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA8);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);


    scene_framebuffer = new QOpenGLFramebufferObject(1024, 1024, format);
    picking_framebuffer = new QOpenGLFramebufferObject(1024, 1024, format);

    picking_vertex_array_object.create();
    picking_vertex_array_object.bind();

    vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    line_id_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);

    line_id_buffer.release();
    vertex_buffer.release();
    picking_vertex_array_object.release();

    // Fullscreen quad setup for blitting
    fullscreen_quad_vbo.create();
    fullscreen_quad_vao.create();
    fullscreen_quad_vao.bind();
    fullscreen_quad_vbo.bind();

    const float quad_vertices[] = {
        // positions // texCoords
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f,
    };
    fullscreen_quad_vbo.allocate(quad_vertices, sizeof(quad_vertices));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

    fullscreen_quad_vbo.release();
    fullscreen_quad_vao.release();

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
    auto * line_program = shader_manager.getProgram("line_with_geometry");
    if (line_program) {
        line_shader_program = line_program->getNativeProgram();

        // Cache the uniform location for efficient hover rendering
        if (line_shader_program) {
            line_shader_program->bind();
            cached_hover_uniform_location = line_shader_program->uniformLocation("u_hover_line_id");
            line_shader_program->release();
        }

        qDebug() << "Successfully loaded line_with_geometry shader, hover uniform location:"
                 << cached_hover_uniform_location;
    } else {
        qDebug() << "line_with_geometry shader is null!";
        line_shader_program = nullptr;
        cached_hover_uniform_location = -1;
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
    auto * picking_program = shader_manager.getProgram("line_picking");
    if (picking_program) {
        picking_shader_program = picking_program->getNativeProgram();
        qDebug() << "Successfully loaded line_picking shader";
    } else {
        qDebug() << "line_picking shader is null!";
        picking_shader_program = nullptr;
    }

    // Load blit shader
    if (!shader_manager.getProgram("blit")) {
        bool success =
            shader_manager.loadProgram("blit", 
            ":/shaders/blit.vert", ":/shaders/blit.frag", "",ShaderSourceType::Resource);
        if (!success) {
            qDebug() << "Failed to load blit shader!";
        }
    }
    auto * blit_program = shader_manager.getProgram("blit");
    if (blit_program) {
        blit_shader_program = blit_program->getNativeProgram();
        qDebug() << "Successfully loaded blit shader";
    } else {
        qDebug() << "blit shader is null!";
        blit_shader_program = nullptr;
    }

    updateOpenGLBuffers();
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

    if (picking_vertex_array_object.isCreated()) {
        picking_vertex_array_object.destroy();
    }

    if (fullscreen_quad_vbo.isCreated()) {
        fullscreen_quad_vbo.destroy();
    }

    if (fullscreen_quad_vao.isCreated()) {
        fullscreen_quad_vao.destroy();
    }

    if (scene_framebuffer) {
        delete scene_framebuffer;
        scene_framebuffer = nullptr;
    }

    if (picking_framebuffer) {
        delete picking_framebuffer;
        picking_framebuffer = nullptr;
    }
}

void LineDataVisualization::updateOpenGLBuffers() {
    vertex_buffer.bind();
    vertex_buffer.allocate(vertex_data.data(), static_cast<int>(vertex_data.size() * sizeof(float)));
    vertex_buffer.release();

    line_id_buffer.bind();
    line_id_buffer.allocate(line_id_data.data(), static_cast<int>(line_id_data.size() * sizeof(uint32_t)));
    line_id_buffer.release();
}

void LineDataVisualization::renderLines(QOpenGLShaderProgram * shader_program, float line_width) {
    if (!visible || vertex_data.empty() || !shader_program) {
        return;
    }

    renderLinesDirect(shader_program, line_width);
    
}

void LineDataVisualization::renderLinesDirect(QOpenGLShaderProgram * shader_program, float line_width) {

    if (m_dataIsDirty) {
        qDebug() << "LineDataVisualization: Data is dirty, rebuilding vertex data";
        buildVertexData(m_line_data.get());
        updateOpenGLBuffers();
        m_dataIsDirty = false;
        m_viewIsDirty = true;// Data change necessitates a view update
    }

    if (m_viewIsDirty) {
        renderLinesToSceneBuffer(shader_program, line_width);
        renderLinesToPickingBuffer(line_width);
        m_viewIsDirty = false;
    }

    // Blit the cached scene to the screen
    blitSceneBuffer();

    // Draw hover line on top
    if (has_hover_line) {
        renderHoverLine(shader_program, line_width);
    }
}

void LineDataVisualization::renderLines(float line_width) {
    if (line_shader_program) {
        renderLines(line_shader_program, line_width);
    } else {
        qDebug() << "Cannot render lines: line_shader_program is null (shader compilation failed?)";
    }
}

void LineDataVisualization::renderLinesToSceneBuffer(QOpenGLShaderProgram * shader_program, float line_width) {
    if (!visible || vertex_data.empty() || !shader_program || !scene_framebuffer) {
        qDebug() << "renderLinesToSceneBuffer: Skipping render - missing resources";
        return;
    }

    // Store current viewport
    GLint old_viewport[4];
    glGetIntegerv(GL_VIEWPORT, old_viewport);

    scene_framebuffer->bind();
    glViewport(0, 0, scene_framebuffer->width(), scene_framebuffer->height());

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    shader_program->bind();

    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    shader_program->setUniformValue("u_canvas_size", canvas_size);
    shader_program->setUniformValue("u_is_selected", false);
    shader_program->setUniformValue("u_hover_line_id", 0u);// Don't highlight any hover line in the main scene

    vertex_array_object.bind();
    if (!vertex_data.empty()) {
        uint32_t total_vertices = static_cast<uint32_t>(vertex_data.size() / 2);
        glDrawArrays(GL_LINES, 0, total_vertices);
    }
    vertex_array_object.release();
    shader_program->release();

    scene_framebuffer->release();
    glDisable(GL_DEPTH_TEST);

    // Restore viewport
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
}

void LineDataVisualization::blitSceneBuffer() {
    if (!blit_shader_program || !scene_framebuffer) {
        return;
    }

    blit_shader_program->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_framebuffer->texture());
    blit_shader_program->setUniformValue("u_texture", 0);

    fullscreen_quad_vao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    fullscreen_quad_vao.release();

    glBindTexture(GL_TEXTURE_2D, 0);
    blit_shader_program->release();
}

void LineDataVisualization::renderHoverLine(QOpenGLShaderProgram * shader_program, float line_width) {
    if (!has_hover_line || cached_hover_line_index >= line_vertex_ranges.size()) {
        return;
    }

    shader_program->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    shader_program->setUniformValue("u_canvas_size", canvas_size);

    // Set hover state
    uint32_t shader_line_id = cached_hover_line_index + 1;
    if (cached_hover_uniform_location >= 0) {
        glUniform1ui(cached_hover_uniform_location, shader_line_id);
    } else {
        shader_program->setUniformValue("u_hover_line_id", shader_line_id);
    }

    // Get the vertex range for the hovered line
    LineVertexRange const & range = line_vertex_ranges[cached_hover_line_index];

    // Render just the hovered line
    vertex_array_object.bind();
    glDrawArrays(GL_LINES, range.start_vertex, range.vertex_count);
    vertex_array_object.release();

    // Reset hover state
    if (cached_hover_uniform_location >= 0) {
        glUniform1ui(cached_hover_uniform_location, 0u);
    } else {
        shader_program->setUniformValue("u_hover_line_id", 0u);
    }

    glDisable(GL_BLEND);
    shader_program->release();
}

void LineDataVisualization::renderLinesToPickingBuffer(float line_width) {
    if (!visible || vertex_data.empty() || !picking_shader_program || !picking_framebuffer) {
        qDebug() << "renderLinesToPickingBuffer: Skipping render - missing resources";
        return;
    }

    static bool first_picking_render = true;
    if (first_picking_render) {
        qDebug() << "renderLinesToPickingBuffer: First render with" << vertex_data.size() / 2 << "vertices";
        first_picking_render = false;
    }

    // Store current viewport
    GLint old_viewport[4];
    glGetIntegerv(GL_VIEWPORT, old_viewport);

    // Bind picking framebuffer
    picking_framebuffer->bind();
    glViewport(0, 0, picking_framebuffer->width(), picking_framebuffer->height());


    // Clear framebuffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

    // Restore viewport
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
}

std::optional<LineIdentifier> LineDataVisualization::getLineAtScreenPosition(
    int screen_x, int screen_y, int widget_width, int widget_height) {
    if (!picking_framebuffer || !picking_shader_program) {
        qDebug() << "getLineAtScreenPosition: Missing framebuffer or shader";
        return std::nullopt;
    }

    qDebug() << "getLineAtScreenPosition called at (" << screen_x << "," << screen_y << ")";

    // Always re-render to picking buffer for accurate hover detection.
    // Use a larger line width for easier picking.
    renderLinesToPickingBuffer(20.0f);

    // Read pixel at screen position
    picking_framebuffer->bind();

    // Convert screen coordinates to framebuffer coordinates.
    // The mouse y-coordinate is typically from the top, while OpenGL's is from the bottom.
    int inverted_y = widget_height - screen_y;

    // Normalize coordinates based on widget size, then scale to framebuffer size.
    float normalized_x = static_cast<float>(screen_x) / static_cast<float>(widget_width);
    float normalized_y = static_cast<float>(inverted_y) / static_cast<float>(widget_height);

    int fb_x = static_cast<int>(normalized_x * picking_framebuffer->width());
    int fb_y = static_cast<int>(normalized_y * picking_framebuffer->height());

    // Clamp coordinates to be safe
    fb_x = std::max(0, std::min(fb_x, picking_framebuffer->width() - 1));
    fb_y = std::max(0, std::min(fb_y, picking_framebuffer->height() - 1));

    qDebug() << "Screen (" << screen_x << "," << screen_y << ") in widget (" << widget_width << "x"
             << widget_height << ") -> FB (" << fb_x << "," << fb_y << ")";

    // Read pixel
    unsigned char pixel[4];
    glReadPixels(fb_x, fb_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    picking_framebuffer->release();

    //qDebug() << "Pixel color: R=" << (int)pixel[0] << " G=" << (int)pixel[1] 
    //         << " B=" << (int)pixel[2] << " A=" << (int)pixel[3];

    // Convert pixel color back to line ID
    uint32_t line_id = (static_cast<uint32_t>(pixel[0]) << 16) |
                       (static_cast<uint32_t>(pixel[1]) << 8) |
                       static_cast<uint32_t>(pixel[2]);

    //qDebug() << "Decoded line_id:" << line_id << "total lines:" << line_identifiers.size();

    // Check if we found a valid line
    if (line_id > 0 && line_id <= line_identifiers.size()) {
        //qDebug() << "Found line at index:" << (line_id - 1);
        return line_identifiers[line_id - 1];  // Adjust for 1-based indexing
    }

    qDebug() << "No line found at this position";
    return std::nullopt;
}

void LineDataVisualization::setHoverLine(std::optional<LineIdentifier> line_id) {
    if (line_id.has_value()) {
        current_hover_line = line_id.value();
        has_hover_line = true;
        
        // Cache the line index to avoid expensive linear search during rendering
        auto it = std::find_if(line_identifiers.begin(), line_identifiers.end(),
                               [this](LineIdentifier const & id) {
                                   return id == current_hover_line;
                               });
        if (it != line_identifiers.end()) {
            cached_hover_line_index = static_cast<uint32_t>(std::distance(line_identifiers.begin(), it));
        } else {
            has_hover_line = false; // Invalid line ID
        }
    } else {
        has_hover_line = false;
        cached_hover_line_index = 0;
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