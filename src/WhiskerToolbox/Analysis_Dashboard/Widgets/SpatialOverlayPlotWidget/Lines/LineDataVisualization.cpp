#include "LineDataVisualization.hpp"

#include "DataManager/Lines/Line_Data.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "ShaderManager/ShaderSourceType.hpp"

#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/LineSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/NoneSelectionHandler.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/Selection/PolygonSelectionHandler.hpp"

#include <QDebug>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <chrono>

LineDataVisualization::LineDataVisualization(QString const & data_key, std::shared_ptr<LineData> const & line_data)
    : m_line_data_ptr(line_data),
      key(data_key),
      scene_framebuffer(nullptr),
      line_shader_program(nullptr),
      blit_shader_program(nullptr),
      line_intersection_compute_shader(nullptr) {

    initializeOpenGLFunctions();

    color = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);

    buildVertexData();

    initializeOpenGLResources();
    m_dataIsDirty = false;// Data is clean after initial build and buffer creation
}

LineDataVisualization::~LineDataVisualization() {
    cleanupOpenGLResources();
}

void LineDataVisualization::buildVertexData() {
    m_vertex_data.clear();
    m_line_identifiers.clear();
    m_line_vertex_ranges.clear();

    if (!m_line_data_ptr) {
        return;
    }

    // Get canvas size for coordinate normalization
    ImageSize image_size = m_line_data_ptr->getImageSize();
    if (image_size.width <= 0 || image_size.height <= 0) {
        qDebug() << "Invalid image size for LineData, using media data instead";
    }
    if (image_size.width <= 0 || image_size.height <= 0) {
        qDebug() << "Using default canvas size 640x480 for LineData";
        image_size = {640, 480};// Fallback to a default size
    }
    canvas_size = QVector2D(static_cast<float>(image_size.width), static_cast<float>(image_size.height));
    qDebug() << "Canvas size:" << canvas_size.x() << "x" << canvas_size.y();

    // We'll create line segments (pairs of vertices) for the geometry shader
    // Each line segment gets a line ID for picking/hovering
    std::vector<float> segment_vertices;   // All line segments as pairs of vertices
    std::vector<uint32_t> segment_line_ids;// Line ID for each vertex in segments

    uint32_t line_index = 0;

    for (auto const & [time_frame, lines]: m_line_data_ptr->GetAllLinesAsRange()) {
        for (int line_id = 0; line_id < static_cast<int>(lines.size()); ++line_id) {
            Line2D const & line = lines[line_id];

            if (line.size() < 2) {
                continue;
            }

            m_line_identifiers.push_back({time_frame.getValue(), line_id});

            uint32_t line_start_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);

            // Convert line strip to line segments (pairs of consecutive vertices)
            for (size_t i = 0; i < line.size() - 1; ++i) {
                Point2D<float> const & p0 = line[i];
                Point2D<float> const & p1 = line[i + 1];

                segment_vertices.push_back(p0.x);
                segment_vertices.push_back(p0.y);
                segment_line_ids.push_back(line_index + 1);// Use 1-based indexing for picking

                segment_vertices.push_back(p1.x);
                segment_vertices.push_back(p1.y);
                segment_line_ids.push_back(line_index + 1);// Use 1-based indexing for picking
            }

            uint32_t line_end_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);
            uint32_t line_vertex_count = line_end_vertex - line_start_vertex;

            m_line_vertex_ranges.push_back({line_start_vertex, line_vertex_count});

            line_index++;
        }
    }

    m_vertex_data = std::move(segment_vertices);
    m_line_id_data = std::move(segment_line_ids);

    qDebug() << "LineDataVisualization: Built" << m_line_identifiers.size()
             << "lines with" << m_vertex_data.size() / 4 << "segments ("
             << m_vertex_data.size() / 2 << "vertices)";

    // Build fast lookup map from LineIdentifier to index
    line_id_to_index.clear();
    for (size_t i = 0; i < m_line_identifiers.size(); ++i) {
        line_id_to_index[m_line_identifiers[i]] = i;
    }
    
    total_line_count = m_line_identifiers.size();
    hidden_line_count = hidden_lines.size();
}

void LineDataVisualization::initializeOpenGLResources() {
    // Create vertex buffer
    m_vertex_buffer.create();
    m_line_id_buffer.create();

    m_vertex_array_object.create();
    m_vertex_array_object.bind();

    m_vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_line_id_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);

    m_line_id_buffer.release();
    m_vertex_buffer.release();
    m_vertex_array_object.release();

    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA8);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);


    scene_framebuffer = new QOpenGLFramebufferObject(1024, 1024, format);

    // Initialize compute shader resources for line intersection
    initializeComputeShaderResources();

    // Fullscreen quad setup for blitting
    fullscreen_quad_vbo.create();
    fullscreen_quad_vao.create();
    fullscreen_quad_vao.bind();
    fullscreen_quad_vbo.bind();

    float const quad_vertices[] = {
            // positions // texCoords
            -1.0f,
            1.0f,
            0.0f,
            1.0f,
            -1.0f,
            -1.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            -1.0f,
            1.0f,
            0.0f,
    };
    fullscreen_quad_vbo.allocate(quad_vertices, sizeof(quad_vertices));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

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

    // Load compute shader directly (ShaderManager doesn't support compute shaders yet)
    line_intersection_compute_shader = new QOpenGLShaderProgram();
    if (!line_intersection_compute_shader->addShaderFromSourceFile(QOpenGLShader::Compute, ":/shaders/line_intersection.comp")) {
        qDebug() << "Failed to compile line intersection compute shader:" << line_intersection_compute_shader->log();
        delete line_intersection_compute_shader;
        line_intersection_compute_shader = nullptr;
    } else if (!line_intersection_compute_shader->link()) {
        qDebug() << "Failed to link line intersection compute shader:" << line_intersection_compute_shader->log();
        delete line_intersection_compute_shader;
        line_intersection_compute_shader = nullptr;
    } else {
        qDebug() << "Successfully loaded line_intersection_compute shader";
    }

    // Load blit shader
    if (!shader_manager.getProgram("blit")) {
        bool success =
                shader_manager.loadProgram("blit",
                                           ":/shaders/blit.vert", ":/shaders/blit.frag", "", ShaderSourceType::Resource);
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

    selection_vertex_buffer.create();
    selection_vertex_array_object.create();
    selection_vertex_array_object.bind();
    selection_vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
    selection_vertex_buffer.release();
    selection_vertex_array_object.release();

    // Initialize selection mask buffer
    selection_mask_buffer.create();
    
    // Initialize visibility mask buffer
    visibility_mask_buffer.create();

    updateOpenGLBuffers();
}

void LineDataVisualization::cleanupOpenGLResources() {
    if (m_vertex_buffer.isCreated()) {
        m_vertex_buffer.destroy();
    }

    if (m_line_id_buffer.isCreated()) {
        m_line_id_buffer.destroy();
    }

    if (m_vertex_array_object.isCreated()) {
        m_vertex_array_object.destroy();
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

    if (selection_vertex_buffer.isCreated()) {
        selection_vertex_buffer.destroy();
    }
    if (selection_vertex_array_object.isCreated()) {
        selection_vertex_array_object.destroy();
    }
    if (selection_mask_buffer.isCreated()) {
        selection_mask_buffer.destroy();
    }
    if (visibility_mask_buffer.isCreated()) {
        visibility_mask_buffer.destroy();
    }

    cleanupComputeShaderResources();

    if (line_intersection_compute_shader) {
        delete line_intersection_compute_shader;
        line_intersection_compute_shader = nullptr;
    }
}

void LineDataVisualization::updateOpenGLBuffers() {
    m_vertex_buffer.bind();
    m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
    m_vertex_buffer.release();

    m_line_id_buffer.bind();
    m_line_id_buffer.allocate(m_line_id_data.data(), static_cast<int>(m_line_id_data.size() * sizeof(uint32_t)));
    m_line_id_buffer.release();

    // Initialize selection mask (all unselected initially)
    selection_mask.assign(m_line_identifiers.size(), 0);
    selection_mask_buffer.bind();
    selection_mask_buffer.allocate(selection_mask.data(), static_cast<int>(selection_mask.size() * sizeof(uint32_t)));
    selection_mask_buffer.release();

    // Initialize visibility mask (all visible initially, except those in hidden_lines set)
    visibility_mask.assign(m_line_identifiers.size(), 1); // Default to visible
    updateVisibilityMask(); // Apply any existing hidden lines

    // Update line segments buffer for compute shader
    updateLineSegmentsBuffer();
}

void LineDataVisualization::render(QMatrix4x4 const & mvp_matrix, float line_width) {
    if (!visible || m_vertex_data.empty() || !line_shader_program) {
        return;
    }

    if (m_dataIsDirty) {
        qDebug() << "LineDataVisualization: Data is dirty, rebuilding vertex data";
        buildVertexData();
        updateOpenGLBuffers();
        m_dataIsDirty = false;
        m_viewIsDirty = true;// Data change necessitates a view update
    }

    // Check if MVP matrix has changed (for panning/zooming)
    if (mvp_matrix != m_cachedMvpMatrix) {
        qDebug() << "LineDataVisualization: MVP matrix changed, marking view as dirty";
        m_viewIsDirty = true;
        m_cachedMvpMatrix = mvp_matrix;
    }

    if (m_viewIsDirty) {
        renderLinesToSceneBuffer(mvp_matrix, line_shader_program, line_width);
        m_viewIsDirty = false;
    }

    // Blit the cached scene to the screen
    blitSceneBuffer();

    // Draw hover line on top
    if (has_hover_line) {
        renderHoverLine(mvp_matrix, line_shader_program, line_width);
    }

    // Selection is now handled automatically by the geometry shader using the selection mask buffer
    // No need for separate renderSelection call
}

void LineDataVisualization::renderLinesToSceneBuffer(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width) {
    if (!visible || m_vertex_data.empty() || !shader_program || !scene_framebuffer) {
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

    shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    shader_program->setUniformValue("u_color", color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    shader_program->setUniformValue("u_canvas_size", canvas_size);
    shader_program->setUniformValue("u_is_selected", false);
    shader_program->setUniformValue("u_hover_line_id", 0u);// Don't highlight any hover line in the main scene

    // Bind selection mask buffer for geometry shader
    selection_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, selection_mask_buffer.bufferId());
    selection_mask_buffer.release();
    
    // Bind visibility mask buffer for geometry shader
    visibility_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, visibility_mask_buffer.bufferId());
    visibility_mask_buffer.release();

    m_vertex_array_object.bind();
    if (!m_vertex_data.empty()) {
        uint32_t total_vertices = static_cast<uint32_t>(m_vertex_data.size() / 2);
        glDrawArrays(GL_LINES, 0, total_vertices);
    }
    m_vertex_array_object.release();
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

void LineDataVisualization::renderHoverLine(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width) {
    if (!has_hover_line || cached_hover_line_index >= m_line_vertex_ranges.size()) {
        return;
    }

    shader_program->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
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

    // Bind selection mask buffer for geometry shader
    selection_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, selection_mask_buffer.bufferId());
    selection_mask_buffer.release();
    
    // Bind visibility mask buffer for geometry shader
    visibility_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, visibility_mask_buffer.bufferId());
    visibility_mask_buffer.release();

    // Get the vertex range for the hovered line
    LineVertexRange const & range = m_line_vertex_ranges[cached_hover_line_index];

    // Render just the hovered line
    m_vertex_array_object.bind();
    glDrawArrays(GL_LINES, range.start_vertex, range.vertex_count);
    m_vertex_array_object.release();

    // Reset hover state
    if (cached_hover_uniform_location >= 0) {
        glUniform1ui(cached_hover_uniform_location, 0u);
    } else {
        shader_program->setUniformValue("u_hover_line_id", 0u);
    }

    glDisable(GL_BLEND);
    shader_program->release();
}

void LineDataVisualization::setHoverLine(std::optional<LineIdentifier> line_id) {
    if (line_id.has_value()) {
        current_hover_line = line_id.value();
        has_hover_line = true;

        // Cache the line index to avoid expensive linear search during rendering
        auto it = std::find_if(m_line_identifiers.begin(), m_line_identifiers.end(),
                               [this](LineIdentifier const & id) {
                                   return id == current_hover_line;
                               });
        if (it != m_line_identifiers.end()) {
            cached_hover_line_index = static_cast<uint32_t>(std::distance(m_line_identifiers.begin(), it));
        } else {
            has_hover_line = false;// Invalid line ID
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

void LineDataVisualization::clearSelection() {
    qDebug() << "LineDataVisualization::clearSelection: Clearing selection";
    selected_lines.clear();
    updateSelectionMask();
    
    selection_vertex_buffer.bind();
    selection_vertex_buffer.allocate(nullptr, 0);
    selection_vertex_buffer.release();
    m_viewIsDirty = true;
}

void LineDataVisualization::applySelection(SelectionVariant & selection_handler, RenderingContext const& context) {
    if (std::holds_alternative<std::unique_ptr<PolygonSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PolygonSelectionHandler>>(selection_handler));
    } else if (std::holds_alternative<std::unique_ptr<LineSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<LineSelectionHandler>>(selection_handler), context);
    }
    else {
        std::cout << "LineDataVisualization::applySelection: selection_handler is not a PolygonSelectionHandler" << std::endl;
    }
}

void LineDataVisualization::applySelection(PolygonSelectionHandler const & selection_handler) {
    std::cout << "Line Data Polygon Selection not implemented" << std::endl;
}

void LineDataVisualization::applySelection(LineSelectionHandler const & selection_handler, RenderingContext const & context) {
    auto selection_region = dynamic_cast<LineSelectionRegion const *>(selection_handler.getActiveSelectionRegion().get());
    if (!selection_region) {
        return;
    }

    qDebug() << "LineDataVisualization::applySelection: Using compute shader approach";
    qDebug() << "LineDataVisualization::applySelection: Screen coords:" 
             << selection_region->getStartPointScreen().x << "," << selection_region->getStartPointScreen().y
             << "to" << selection_region->getEndPointScreen().x << "," << selection_region->getEndPointScreen().y;

    QMatrix4x4 mvp_matrix = context.projection_matrix * context.view_matrix * context.model_matrix;

    const auto line_width_tolerance = 5.0f;
    auto intersecting_lines = getAllLinesIntersectingLine(
        static_cast<int>(selection_region->getStartPointScreen().x),
        static_cast<int>(selection_region->getStartPointScreen().y),
        static_cast<int>(selection_region->getEndPointScreen().x),
        static_cast<int>(selection_region->getEndPointScreen().y),
        context.viewport_rect.width(),
        context.viewport_rect.height(),
        mvp_matrix,
        line_width_tolerance
    );

    qDebug() << "LineDataVisualization::applySelection: Found" << intersecting_lines.size() << "intersecting lines";

    // Update selection based on keyboard modifiers
    LineSelectionBehavior behavior = selection_region->getBehavior();

    if (behavior == LineSelectionBehavior::Replace) {
        selected_lines.clear();
        for (const auto& line_id : intersecting_lines) {
            selected_lines.insert(line_id);
        }
    } else if (behavior == LineSelectionBehavior::Append) {
        for (const auto& line_id : intersecting_lines) {
            selected_lines.insert(line_id);
        }
    } else if (behavior == LineSelectionBehavior::Remove) {
        for (const auto& line_id : intersecting_lines) {
            selected_lines.erase(line_id);
        }
    }
    
    qDebug() << "LineDataVisualization::applySelection: Selected" << selected_lines.size() << "lines";
    
    // Update GPU selection mask efficiently
    updateSelectionMask();
    
    m_viewIsDirty = true;
}

QString LineDataVisualization::getTooltipText() const {
    if (!has_hover_line) {
        return QString();
    }

    return QString("Dataset: %1\nTimeframe: %2\nLine ID: %3")
            .arg(key)
            .arg(current_hover_line.time_frame)
            .arg(current_hover_line.line_id);
}

void LineDataVisualization::renderSelection(QMatrix4x4 const & mvp_matrix, float line_width)
{
    if (selected_lines.empty() || !line_shader_program) {
        qDebug() << "LineDataVisualization::renderSelection: Skipping - selected_lines empty or no shader";
        return;
    }

    qDebug() << "LineDataVisualization::renderSelection: Rendering" << selected_lines.size() << "selected lines";

    line_shader_program->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    line_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    line_shader_program->setUniformValue("u_color", color);
    line_shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    line_shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f)); // Black color
    line_shader_program->setUniformValue("u_line_width", line_width + 2.0f); // Thicker for visibility
    line_shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    line_shader_program->setUniformValue("u_canvas_size", canvas_size);
    line_shader_program->setUniformValue("u_is_selected", true);
    line_shader_program->setUniformValue("u_hover_line_id", 0u);

    selection_vertex_array_object.bind();
    selection_vertex_buffer.bind();
    int buffer_size = selection_vertex_buffer.size();
    int vertex_count = buffer_size / (2 * sizeof(float));
    qDebug() << "LineDataVisualization::renderSelection: Buffer size:" << buffer_size << "bytes, vertex count:" << vertex_count;
    qDebug() << "LineDataVisualization::renderSelection: sizeof(float)=" << sizeof(float) << ", 2*sizeof(float)=" << (2 * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glDrawArrays(GL_LINES, 0, vertex_count);
    selection_vertex_buffer.release();
    selection_vertex_array_object.release();

    line_shader_program->setUniformValue("u_is_selected", false); // Reset state
    glDisable(GL_BLEND);
    line_shader_program->release();
    
    qDebug() << "LineDataVisualization::renderSelection: Finished rendering selected lines";
}

void LineDataVisualization::initializeComputeShaderResources() {
    // Create buffers for compute shader
    line_segments_buffer.create();
    intersection_results_buffer.create();
    intersection_count_buffer.create();

    // Initialize intersection count buffer with zero
    intersection_count_buffer.bind();
    intersection_count_buffer.allocate(sizeof(uint32_t));
    uint32_t zero = 0;
    intersection_count_buffer.write(0, &zero, sizeof(uint32_t));
    intersection_count_buffer.release();

    // Allocate a reasonable size for intersection results (can be expanded if needed)
    intersection_results_buffer.bind();
    intersection_results_buffer.allocate(100000 * sizeof(uint32_t)); // Space for up to 100,000 results
    intersection_results_buffer.release();

    qDebug() << "LineDataVisualization: Initialized compute shader resources";
}

void LineDataVisualization::cleanupComputeShaderResources() {
    if (line_segments_buffer.isCreated()) {
        line_segments_buffer.destroy();
    }
    if (intersection_results_buffer.isCreated()) {
        intersection_results_buffer.destroy();
    }
    if (intersection_count_buffer.isCreated()) {
        intersection_count_buffer.destroy();
    }
}

void LineDataVisualization::updateLineSegmentsBuffer() {
    if (!line_segments_buffer.isCreated()) {
        return;
    }

    // Create line segments data for compute shader
    // Each segment: x1, y1, x2, y2, line_id (4 floats + 1 uint, but we'll pack line_id as float for simplicity)
    segments_data.clear();
    segments_data.reserve(m_vertex_data.size() / 2 * 5); // m_vertex_data has x,y pairs, we need x1,y1,x2,y2,id

    for (size_t i = 0; i < m_vertex_data.size(); i += 4) { // Each line segment is 4 floats (x1,y1,x2,y2)
        if (i + 3 < m_vertex_data.size()) {
            // Extract segment coordinates
            segments_data.push_back(m_vertex_data[i]);     // x1
            segments_data.push_back(m_vertex_data[i + 1]); // y1
            segments_data.push_back(m_vertex_data[i + 2]); // x2
            segments_data.push_back(m_vertex_data[i + 3]); // y2
            
            // Get line ID from m_line_id_data (same index as vertex data)
            uint32_t line_id = (i/2 < m_line_id_data.size()) ? m_line_id_data[i/2] : 0;
            segments_data.push_back(*reinterpret_cast<float*>(&line_id)); // Pack uint as float bits
        }
    }

    line_segments_buffer.bind();
    line_segments_buffer.allocate(segments_data.data(), static_cast<int>(segments_data.size() * sizeof(float)));
    line_segments_buffer.release();

    qDebug() << "LineDataVisualization: Updated line segments buffer with" << segments_data.size() / 5 << "segments";
}

void LineDataVisualization::updateSelectionMask() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset all selections
    std::fill(selection_mask.begin(), selection_mask.end(), 0);
    
    // Mark selected lines using fast hash map lookup
    for (const auto& line_id : selected_lines) {
        auto it = line_id_to_index.find(line_id);
        if (it != line_id_to_index.end()) {
            size_t index = it->second;
            if (index < selection_mask.size()) {
                selection_mask[index] = 1; // Mark as selected
            }
        }
    }
    
    auto cpu_time = std::chrono::high_resolution_clock::now();
    
    // Update GPU buffer
    selection_mask_buffer.bind();
    selection_mask_buffer.write(0, selection_mask.data(), static_cast<int>(selection_mask.size() * sizeof(uint32_t)));
    selection_mask_buffer.release();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(cpu_time - start_time);
    auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - cpu_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    qDebug() << "LineDataVisualization: Updated selection mask for" << selected_lines.size() << "lines in" 
             << total_duration.count() << "μs (CPU:" << cpu_duration.count() << "μs, GPU:" << gpu_duration.count() << "μs)";
}

void LineDataVisualization::updateVisibilityMask() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset all to visible
    std::fill(visibility_mask.begin(), visibility_mask.end(), 1);
    
    // Mark hidden lines using fast hash map lookup
    for (const auto& line_id : hidden_lines) {
        auto it = line_id_to_index.find(line_id);
        if (it != line_id_to_index.end()) {
            size_t index = it->second;
            if (index < visibility_mask.size()) {
                visibility_mask[index] = 0; // Mark as hidden
            }
        }
    }
    
    // Apply time range filtering if enabled
    if (time_range_enabled && !m_line_identifiers.empty()) {
        for (size_t i = 0; i < m_line_identifiers.size() && i < visibility_mask.size(); ++i) {
            const LineIdentifier& line_id = m_line_identifiers[i];
            int64_t line_time_frame = line_id.time_frame;
            
            // Hide lines outside the time range
            if (line_time_frame < time_range_start || line_time_frame > time_range_end) {
                visibility_mask[i] = 0; // Mark as hidden due to time range
            }
        }
        
        qDebug() << "Applied time range filtering: frames" << time_range_start << "to" << time_range_end;
    }
    
    auto cpu_time = std::chrono::high_resolution_clock::now();
    
    // Update GPU buffer
    visibility_mask_buffer.bind();
    visibility_mask_buffer.allocate(visibility_mask.data(), static_cast<int>(visibility_mask.size() * sizeof(uint32_t)));
    visibility_mask_buffer.release();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(cpu_time - start_time);
    auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - cpu_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    size_t total_filters = hidden_lines.size() + (time_range_enabled ? 1 : 0);
    qDebug() << "LineDataVisualization: Updated visibility mask with" << total_filters << "filters in" 
             << total_duration.count() << "μs (CPU:" << cpu_duration.count() << "μs, GPU:" << gpu_duration.count() << "μs)";
}

std::vector<LineIdentifier> LineDataVisualization::getAllLinesIntersectingLine(
        int start_x, int start_y, int end_x, int end_y,
        int widget_width, int widget_height,
        const QMatrix4x4& mvp_matrix, float line_width) {
    
    if (!line_intersection_compute_shader || m_vertex_data.empty()) {
        qDebug() << "LineDataVisualization: Compute shader not available or no vertex data";
        return {};
    }

    // Update line segments buffer if dirty
    if (m_dataIsDirty) {
        qDebug() << "LineDataVisualization: Data is dirty, updating line segments buffer";
        updateLineSegmentsBuffer();
    } else {
        qDebug() << "LineDataVisualization: Data is clean, using existing segments buffer";
    }

    // Convert screen coordinates to normalized device coordinates [-1, 1]
    float ndc_start_x = (2.0f * start_x / widget_width) - 1.0f;
    float ndc_start_y = 1.0f - (2.0f * start_y / widget_height); // Flip Y
    float ndc_end_x = (2.0f * end_x / widget_width) - 1.0f;
    float ndc_end_y = 1.0f - (2.0f * end_y / widget_height); // Flip Y

    // Create the selection line in NDC space (this is what the compute shader will work with)
    QVector2D query_start(ndc_start_x, ndc_start_y);
    QVector2D query_end(ndc_end_x, ndc_end_y);

    qDebug() << "LineDataVisualization: Screen coords:" << start_x << "," << start_y << "to" << end_x << "," << end_y;
    qDebug() << "LineDataVisualization: NDC coords:" << query_start.x() << "," << query_start.y() 
             << "to" << query_end.x() << "," << query_end.y();

    // Reset intersection count to zero
    uint32_t zero = 0;
    intersection_count_buffer.bind();
    intersection_count_buffer.write(0, &zero, sizeof(uint32_t));
    intersection_count_buffer.release();

    // Bind compute shader and set uniforms
    line_intersection_compute_shader->bind();
    line_intersection_compute_shader->setUniformValue("u_query_line_start", query_start);
    line_intersection_compute_shader->setUniformValue("u_query_line_end", query_end);
    line_intersection_compute_shader->setUniformValue("u_line_width", line_width * 0.01f); // Scale down for NDC space
    line_intersection_compute_shader->setUniformValue("u_mvp_matrix", mvp_matrix);
    line_intersection_compute_shader->setUniformValue("u_canvas_size", canvas_size);

    // Bind storage buffers
    line_segments_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, line_segments_buffer.bufferId());
    line_segments_buffer.release();

    intersection_results_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, intersection_results_buffer.bufferId());
    intersection_results_buffer.release();

    intersection_count_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, intersection_count_buffer.bufferId());
    intersection_count_buffer.release();

    // Dispatch compute shader
    uint32_t num_segments = static_cast<uint32_t>(segments_data.size() / 5); // 5 floats per segment (x1,y1,x2,y2,line_id)
    uint32_t num_work_groups = (num_segments + 63) / 64; // 64 is the local_size_x in the compute shader
    
    qDebug() << "LineDataVisualization: Dispatching compute shader with" << num_segments << "segments," << num_work_groups << "work groups";
    qDebug() << "LineDataVisualization: segments_data size:" << segments_data.size() << "floats";
    
    if (num_segments == 0) {
        qDebug() << "LineDataVisualization: No segments to process!";
        line_intersection_compute_shader->release();
        return {};
    }
    
    glDispatchCompute(num_work_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    line_intersection_compute_shader->release();

    // Read back results
    intersection_count_buffer.bind();
    uint32_t* count_data = static_cast<uint32_t*>(intersection_count_buffer.map(QOpenGLBuffer::ReadOnly));
    uint32_t result_count = count_data ? *count_data : 0;
    intersection_count_buffer.unmap();
    intersection_count_buffer.release();

    qDebug() << "LineDataVisualization: Found" << result_count << "intersecting line segments";

    if (result_count == 0) {
        return {};
    }

    // Read back intersection results
    intersection_results_buffer.bind();
    uint32_t* results_data = static_cast<uint32_t*>(intersection_results_buffer.map(QOpenGLBuffer::ReadOnly));
    
    std::vector<LineIdentifier> intersecting_lines;
    std::unordered_set<uint32_t> unique_line_ids; // To avoid duplicates

    if (results_data) {
        for (uint32_t i = 0; i < result_count && i < 100000; ++i) {
            uint32_t line_id = results_data[i];
            if (line_id > 0 && line_id <= m_line_identifiers.size() && unique_line_ids.find(line_id) == unique_line_ids.end()) {
                unique_line_ids.insert(line_id);
                intersecting_lines.push_back(m_line_identifiers[line_id - 1]); // Convert from 1-based to 0-based indexing
            }
        }
    }

    intersection_results_buffer.unmap();
    intersection_results_buffer.release();

    qDebug() << "LineDataVisualization: Returning" << intersecting_lines.size() << "unique intersecting lines";
    return intersecting_lines;
}

std::optional<LineIdentifier> LineDataVisualization::getLineAtScreenPosition(
        int screen_x, int screen_y, int widget_width, int widget_height) {
    qDebug() << "LineDataVisualization::getLineAtScreenPosition: This method needs MVP matrix, use handleHover instead";
    return std::nullopt;
}

bool LineDataVisualization::handleHover(const QPoint & screen_pos, const QSize & widget_size, const QMatrix4x4& mvp_matrix) {
    qDebug() << "LineDataVisualization::handleHover: Called with screen pos" << screen_pos.x() << "," << screen_pos.y();
    
    // Use the new compute shader approach for hover detection
    // Create a small line segment around the point for intersection testing
    int tolerance = 3; // 3 pixel tolerance for hover
    
    auto intersecting_lines = getAllLinesIntersectingLine(
        screen_pos.x() - tolerance, screen_pos.y() - tolerance,
        screen_pos.x() + tolerance, screen_pos.y() + tolerance,
        widget_size.width(), widget_size.height(),
        mvp_matrix,
        3.0f // Small line width for hover queries
    );

    qDebug() << "LineDataVisualization::handleHover: Found" << intersecting_lines.size() << "intersecting lines";

    bool hover_changed = false;
    if (!intersecting_lines.empty()) {
        LineIdentifier line_id = intersecting_lines[0]; // Take the first intersecting line
        if (!has_hover_line || !(current_hover_line == line_id)) {
            qDebug() << "LineDataVisualization::handleHover: Setting hover line to" << line_id.time_frame << "," << line_id.line_id;
            setHoverLine(line_id);
            hover_changed = true;
        }
    } else {
        if (has_hover_line) {
            qDebug() << "LineDataVisualization::handleHover: Clearing hover line";
            setHoverLine(std::nullopt);
            hover_changed = true;
        }
    }

    return hover_changed;
}

//========== Visibility Management ==========

size_t LineDataVisualization::hideSelectedLines() {
    if (selected_lines.empty()) {
        return 0;
    }
    
    size_t hidden_count = 0;
    
    // Add selected lines to hidden set
    for (const auto& line_id : selected_lines) {
        if (hidden_lines.insert(line_id).second) { // .second is true if insertion happened
            hidden_count++;
        }
    }
    
    // Clear selection since hidden lines should not be selected
    selected_lines.clear();
    
    // Update statistics
    hidden_line_count = hidden_lines.size();
    
    // Update GPU buffers
    updateSelectionMask();
    updateVisibilityMask();
    
    // Mark view as dirty to trigger re-render
    m_viewIsDirty = true;
    
    qDebug() << "LineDataVisualization: Hidden" << hidden_count << "lines, total hidden:" << hidden_line_count;
    
    return hidden_count;
}

size_t LineDataVisualization::showAllLines() {
    size_t shown_count = hidden_lines.size();
    
    // Clear all hidden lines
    hidden_lines.clear();
    hidden_line_count = 0;
    
    // Update GPU visibility buffer
    updateVisibilityMask();
    
    // Mark view as dirty to trigger re-render
    m_viewIsDirty = true;
    
    qDebug() << "LineDataVisualization: Showed" << shown_count << "lines, all lines now visible";
    
    return shown_count;
}

std::pair<size_t, size_t> LineDataVisualization::getVisibilityStats() const {
    return {total_line_count, hidden_line_count};
}

void LineDataVisualization::setTimeRange(int start_frame, int end_frame) {
    qDebug() << "LineDataVisualization::setTimeRange(" << start_frame << "," << end_frame << ")";
    
    time_range_start = start_frame;
    time_range_end = end_frame;
    
    // Update visibility mask to apply time range filtering
    updateVisibilityMask();
    
    qDebug() << "Time range updated and visibility mask refreshed";
}

void LineDataVisualization::setTimeRangeEnabled(bool enabled) {
    qDebug() << "LineDataVisualization::setTimeRangeEnabled(" << enabled << ")";
    
    if (time_range_enabled != enabled) {
        time_range_enabled = enabled;
        
        // Update visibility mask to apply or remove time range filtering
        updateVisibilityMask();
        
        qDebug() << "Time range filtering" << (enabled ? "enabled" : "disabled");
    }
}

std::tuple<int, int, bool> LineDataVisualization::getTimeRange() const {
    return {time_range_start, time_range_end, time_range_enabled};
}