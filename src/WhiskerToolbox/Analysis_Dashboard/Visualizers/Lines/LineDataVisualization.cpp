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
#include <chrono>
#include <cmath>
#include <unordered_map>

LineDataVisualization::LineDataVisualization(QString const & data_key, std::shared_ptr<LineData> const & line_data)
    : m_line_data_ptr(line_data),
      m_key(data_key),
      m_scene_framebuffer(nullptr),
      m_line_shader_program(nullptr),
      m_blit_shader_program(nullptr),
      m_line_intersection_compute_shader(nullptr) {

    initializeOpenGLFunctions();

    m_color = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);

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
    m_canvas_size = QVector2D(static_cast<float>(image_size.width), static_cast<float>(image_size.height));
    qDebug() << "Canvas size:" << m_canvas_size.x() << "x" << m_canvas_size.y();

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
    m_line_id_to_index.clear();
    for (size_t i = 0; i < m_line_identifiers.size(); ++i) {
        m_line_id_to_index[m_line_identifiers[i]] = i;
    }

    m_total_line_count = m_line_identifiers.size();
    m_hidden_line_count = m_hidden_lines.size();
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


    m_scene_framebuffer = new QOpenGLFramebufferObject(1024, 1024, format);

    // Initialize compute shader resources for line intersection
    _initializeComputeShaderResources();

    // Fullscreen quad setup for blitting
    m_fullscreen_quad_vbo.create();
    m_fullscreen_quad_vao.create();
    m_fullscreen_quad_vao.bind();
    m_fullscreen_quad_vbo.bind();

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
    m_fullscreen_quad_vbo.allocate(quad_vertices, sizeof(quad_vertices));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

    m_fullscreen_quad_vbo.release();
    m_fullscreen_quad_vao.release();

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
        m_line_shader_program = line_program->getNativeProgram();

        // Cache the uniform location for efficient hover rendering
        if (m_line_shader_program) {
            m_line_shader_program->bind();
            m_cached_hover_uniform_location = m_line_shader_program->uniformLocation("u_hover_line_id");
            m_line_shader_program->release();
        }

        qDebug() << "Successfully loaded line_with_geometry shader, hover uniform location:"
                 << m_cached_hover_uniform_location;
    } else {
        qDebug() << "line_with_geometry shader is null!";
        m_line_shader_program = nullptr;
        m_cached_hover_uniform_location = -1;
    }

    // Load compute shader directly (ShaderManager doesn't support compute shaders yet)
    m_line_intersection_compute_shader = new QOpenGLShaderProgram();
    if (!m_line_intersection_compute_shader->addShaderFromSourceFile(QOpenGLShader::Compute, ":/shaders/line_intersection.comp")) {
        qDebug() << "Failed to compile line intersection compute shader:" << m_line_intersection_compute_shader->log();
        delete m_line_intersection_compute_shader;
        m_line_intersection_compute_shader = nullptr;
    } else if (!m_line_intersection_compute_shader->link()) {
        qDebug() << "Failed to link line intersection compute shader:" << m_line_intersection_compute_shader->log();
        delete m_line_intersection_compute_shader;
        m_line_intersection_compute_shader = nullptr;
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
        m_blit_shader_program = blit_program->getNativeProgram();
        qDebug() << "Successfully loaded blit shader";
    } else {
        qDebug() << "blit shader is null!";
        m_blit_shader_program = nullptr;
    }

    m_selection_mask_buffer.create();
    m_visibility_mask_buffer.create();

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

    if (m_fullscreen_quad_vbo.isCreated()) {
        m_fullscreen_quad_vbo.destroy();
    }

    if (m_fullscreen_quad_vao.isCreated()) {
        m_fullscreen_quad_vao.destroy();
    }

    if (m_scene_framebuffer) {
        delete m_scene_framebuffer;
        m_scene_framebuffer = nullptr;
    }
    if (m_selection_mask_buffer.isCreated()) {
        m_selection_mask_buffer.destroy();
    }
    if (m_visibility_mask_buffer.isCreated()) {
        m_visibility_mask_buffer.destroy();
    }

    _cleanupComputeShaderResources();

    if (m_line_intersection_compute_shader) {
        delete m_line_intersection_compute_shader;
        m_line_intersection_compute_shader = nullptr;
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
    m_selection_mask.assign(m_line_identifiers.size(), 0);
    m_selection_mask_buffer.bind();
    m_selection_mask_buffer.allocate(m_selection_mask.data(), static_cast<int>(m_selection_mask.size() * sizeof(uint32_t)));
    m_selection_mask_buffer.release();

    // Initialize visibility mask (all visible initially, except those in hidden_lines set)
    m_visibility_mask.assign(m_line_identifiers.size(), 1);// Default to visible
    _updateVisibilityMask();                               // Apply any existing hidden lines

    // Update line segments buffer for compute shader
    _updateLineSegmentsBuffer();
}

void LineDataVisualization::render(QMatrix4x4 const & mvp_matrix, float line_width) {
    if (!m_visible || m_vertex_data.empty() || !m_line_shader_program) {
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
        _renderLinesToSceneBuffer(mvp_matrix, m_line_shader_program, line_width);
        m_viewIsDirty = false;
    }

    // Blit the cached scene to the screen
    _blitSceneBuffer();

    // Draw hover line on top
    if (m_has_hover_line) {
        _renderHoverLine(mvp_matrix, m_line_shader_program, line_width);
    }

    // Selection is now handled automatically by the geometry shader using the selection mask buffer
    // No need for separate renderSelection call
}

void LineDataVisualization::_renderLinesToSceneBuffer(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width) {
    if (!m_visible || m_vertex_data.empty() || !shader_program || !m_scene_framebuffer) {
        qDebug() << "renderLinesToSceneBuffer: Skipping render - missing resources";
        return;
    }

    // Store current viewport
    GLint old_viewport[4];
    glGetIntegerv(GL_VIEWPORT, old_viewport);

    m_scene_framebuffer->bind();
    glViewport(0, 0, m_scene_framebuffer->width(), m_scene_framebuffer->height());

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    shader_program->bind();

    shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    shader_program->setUniformValue("u_color", m_color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    shader_program->setUniformValue("u_canvas_size", m_canvas_size);
    shader_program->setUniformValue("u_is_selected", false);
    shader_program->setUniformValue("u_hover_line_id", 0u);// Don't highlight any hover line in the main scene

    // Bind selection mask buffer for geometry shader
    m_selection_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_selection_mask_buffer.bufferId());
    m_selection_mask_buffer.release();

    // Bind visibility mask buffer for geometry shader
    m_visibility_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_visibility_mask_buffer.bufferId());
    m_visibility_mask_buffer.release();

    m_vertex_array_object.bind();
    if (!m_vertex_data.empty()) {
        uint32_t total_vertices = static_cast<uint32_t>(m_vertex_data.size() / 2);
        glDrawArrays(GL_LINES, 0, total_vertices);
    }
    m_vertex_array_object.release();
    shader_program->release();

    m_scene_framebuffer->release();
    glDisable(GL_DEPTH_TEST);

    // Restore viewport
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
}

void LineDataVisualization::_blitSceneBuffer() {
    if (!m_blit_shader_program || !m_scene_framebuffer) {
        return;
    }

    m_blit_shader_program->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_scene_framebuffer->texture());
    m_blit_shader_program->setUniformValue("u_texture", 0);

    m_fullscreen_quad_vao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_fullscreen_quad_vao.release();

    glBindTexture(GL_TEXTURE_2D, 0);
    m_blit_shader_program->release();
}

void LineDataVisualization::_renderHoverLine(QMatrix4x4 const & mvp_matrix, QOpenGLShaderProgram * shader_program, float line_width) {
    if (!m_has_hover_line || m_cached_hover_line_index >= m_line_vertex_ranges.size()) {
        return;
    }

    shader_program->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    shader_program->setUniformValue("u_color", m_color);
    shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    shader_program->setUniformValue("u_line_width", line_width);
    shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    shader_program->setUniformValue("u_canvas_size", m_canvas_size);

    // Set hover state
    uint32_t shader_line_id = m_cached_hover_line_index + 1;
    if (m_cached_hover_uniform_location >= 0) {
        glUniform1ui(m_cached_hover_uniform_location, shader_line_id);
    } else {
        shader_program->setUniformValue("u_hover_line_id", shader_line_id);
    }

    // Bind selection mask buffer for geometry shader
    m_selection_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_selection_mask_buffer.bufferId());
    m_selection_mask_buffer.release();

    // Bind visibility mask buffer for geometry shader
    m_visibility_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_visibility_mask_buffer.bufferId());
    m_visibility_mask_buffer.release();

    // Get the vertex range for the hovered line
    LineVertexRange const & range = m_line_vertex_ranges[m_cached_hover_line_index];

    // Render just the hovered line
    m_vertex_array_object.bind();
    glDrawArrays(GL_LINES, range.start_vertex, range.vertex_count);
    m_vertex_array_object.release();

    // Reset hover state
    if (m_cached_hover_uniform_location >= 0) {
        glUniform1ui(m_cached_hover_uniform_location, 0u);
    } else {
        shader_program->setUniformValue("u_hover_line_id", 0u);
    }

    glDisable(GL_BLEND);
    shader_program->release();
}

void LineDataVisualization::setHoverLine(std::optional<LineIdentifier> line_id) {
    if (line_id.has_value()) {
        m_current_hover_line = line_id.value();
        m_has_hover_line = true;

        // Cache the line index to avoid expensive linear search during rendering
        auto it = std::find_if(m_line_identifiers.begin(), m_line_identifiers.end(),
                               [this](LineIdentifier const & id) {
                                   return id == m_current_hover_line;
                               });
        if (it != m_line_identifiers.end()) {
            m_cached_hover_line_index = static_cast<uint32_t>(std::distance(m_line_identifiers.begin(), it));
        } else {
            m_has_hover_line = false;// Invalid line ID
        }
    } else {
        m_has_hover_line = false;
        m_cached_hover_line_index = 0;
    }
}

std::optional<LineIdentifier> LineDataVisualization::getHoverLine() const {
    if (m_has_hover_line) {
        return m_current_hover_line;
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
    m_selected_lines.clear();
    _updateSelectionMask();

    //selection_vertex_buffer.bind();
    //selection_vertex_buffer.allocate(nullptr, 0);
    //selection_vertex_buffer.release();
    m_viewIsDirty = true;
}

void LineDataVisualization::applySelection(SelectionVariant & selection_handler, RenderingContext const & context) {
    if (std::holds_alternative<std::unique_ptr<PolygonSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<PolygonSelectionHandler>>(selection_handler));
    } else if (std::holds_alternative<std::unique_ptr<LineSelectionHandler>>(selection_handler)) {
        applySelection(*std::get<std::unique_ptr<LineSelectionHandler>>(selection_handler), context);
    } else {
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

    auto const line_width_tolerance = 5.0f;
    auto intersecting_lines = getAllLinesIntersectingLine(
            static_cast<int>(selection_region->getStartPointScreen().x),
            static_cast<int>(selection_region->getStartPointScreen().y),
            static_cast<int>(selection_region->getEndPointScreen().x),
            static_cast<int>(selection_region->getEndPointScreen().y),
            context.viewport_rect.width(),
            context.viewport_rect.height(),
            mvp_matrix,
            line_width_tolerance);

    qDebug() << "LineDataVisualization::applySelection: Found" << intersecting_lines.size() << "intersecting lines";

    // Update selection based on keyboard modifiers
    LineSelectionBehavior behavior = selection_region->getBehavior();

    if (behavior == LineSelectionBehavior::Replace) {
        m_selected_lines.clear();
        for (auto const & line_id: intersecting_lines) {
            m_selected_lines.insert(line_id);
        }
    } else if (behavior == LineSelectionBehavior::Append) {
        for (auto const & line_id: intersecting_lines) {
            m_selected_lines.insert(line_id);
        }
    } else if (behavior == LineSelectionBehavior::Remove) {
        for (auto const & line_id: intersecting_lines) {
            m_selected_lines.erase(line_id);
        }
    }

    qDebug() << "LineDataVisualization::applySelection: Selected" << m_selected_lines.size() << "lines";

    // Update GPU selection mask efficiently
    _updateSelectionMask();

    m_viewIsDirty = true;
}

QString LineDataVisualization::getTooltipText() const {
    if (!m_has_hover_line) {
        return QString();
    }

    return QString("Dataset: %1\nTimeframe: %2\nLine ID: %3")
            .arg(m_key)
            .arg(m_current_hover_line.time_frame)
            .arg(m_current_hover_line.line_id);
}

void LineDataVisualization::_renderSelection(QMatrix4x4 const & mvp_matrix, float line_width) {
    if (m_selected_lines.empty() || !m_line_shader_program) {
        qDebug() << "LineDataVisualization::renderSelection: Skipping - selected_lines empty or no shader";
        return;
    }

    qDebug() << "LineDataVisualization::renderSelection: Rendering" << m_selected_lines.size() << "selected lines";

    m_line_shader_program->bind();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_line_shader_program->setUniformValue("u_mvp_matrix", mvp_matrix);
    m_line_shader_program->setUniformValue("u_color", m_color);
    m_line_shader_program->setUniformValue("u_hover_color", QVector4D(1.0f, 1.0f, 0.0f, 1.0f));
    m_line_shader_program->setUniformValue("u_selected_color", QVector4D(0.0f, 0.0f, 0.0f, 1.0f));// Black color
    m_line_shader_program->setUniformValue("u_line_width", line_width + 2.0f);                    // Thicker for visibility
    m_line_shader_program->setUniformValue("u_viewport_size", QVector2D(1024.0f, 1024.0f));
    m_line_shader_program->setUniformValue("u_canvas_size", m_canvas_size);
    m_line_shader_program->setUniformValue("u_is_selected", true);
    m_line_shader_program->setUniformValue("u_hover_line_id", 0u);

    m_line_shader_program->setUniformValue("u_is_selected", false);// Reset state
    glDisable(GL_BLEND);
    m_line_shader_program->release();

    qDebug() << "LineDataVisualization::renderSelection: Finished rendering selected lines";
}

void LineDataVisualization::_initializeComputeShaderResources() {
    // Create buffers for compute shader
    m_line_segments_buffer.create();
    m_intersection_results_buffer.create();
    m_intersection_count_buffer.create();

    // Initialize intersection count buffer with zero
    m_intersection_count_buffer.bind();
    m_intersection_count_buffer.allocate(sizeof(uint32_t));
    uint32_t zero = 0;
    m_intersection_count_buffer.write(0, &zero, sizeof(uint32_t));
    m_intersection_count_buffer.release();

    // Allocate a reasonable size for intersection results (can be expanded if needed)
    m_intersection_results_buffer.bind();
    m_intersection_results_buffer.allocate(100000 * sizeof(uint32_t));// Space for up to 100,000 results
    m_intersection_results_buffer.release();

    qDebug() << "LineDataVisualization: Initialized compute shader resources";
}

void LineDataVisualization::_cleanupComputeShaderResources() {
    if (m_line_segments_buffer.isCreated()) {
        m_line_segments_buffer.destroy();
    }
    if (m_intersection_results_buffer.isCreated()) {
        m_intersection_results_buffer.destroy();
    }
    if (m_intersection_count_buffer.isCreated()) {
        m_intersection_count_buffer.destroy();
    }
}

void LineDataVisualization::_updateLineSegmentsBuffer() {
    if (!m_line_segments_buffer.isCreated()) {
        return;
    }

    // Create line segments data for compute shader
    // Each segment: x1, y1, x2, y2, line_id (4 floats + 1 uint, but we'll pack line_id as float for simplicity)
    m_segments_data.clear();
    m_segments_data.reserve(m_vertex_data.size() / 2 * 5);// m_vertex_data has x,y pairs, we need x1,y1,x2,y2,id

    for (size_t i = 0; i < m_vertex_data.size(); i += 4) {// Each line segment is 4 floats (x1,y1,x2,y2)
        if (i + 3 < m_vertex_data.size()) {
            // Extract segment coordinates
            m_segments_data.push_back(m_vertex_data[i]);    // x1
            m_segments_data.push_back(m_vertex_data[i + 1]);// y1
            m_segments_data.push_back(m_vertex_data[i + 2]);// x2
            m_segments_data.push_back(m_vertex_data[i + 3]);// y2

            // Get line ID from m_line_id_data (same index as vertex data)
            uint32_t line_id = (i / 2 < m_line_id_data.size()) ? m_line_id_data[i / 2] : 0;
            m_segments_data.push_back(*reinterpret_cast<float *>(&line_id));// Pack uint as float bits
        }
    }

    m_line_segments_buffer.bind();
    m_line_segments_buffer.allocate(m_segments_data.data(), static_cast<int>(m_segments_data.size() * sizeof(float)));
    m_line_segments_buffer.release();

    qDebug() << "LineDataVisualization: Updated line segments buffer with" << m_segments_data.size() / 5 << "segments";
}

void LineDataVisualization::_updateSelectionMask() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset all selections
    std::fill(m_selection_mask.begin(), m_selection_mask.end(), 0);

    // Mark selected lines using fast hash map lookup
    for (auto const & line_id: m_selected_lines) {
        auto it = m_line_id_to_index.find(line_id);
        if (it != m_line_id_to_index.end()) {
            size_t index = it->second;
            if (index < m_selection_mask.size()) {
                m_selection_mask[index] = 1;// Mark as selected
            }
        }
    }

    auto cpu_time = std::chrono::high_resolution_clock::now();

    // Update GPU buffer
    m_selection_mask_buffer.bind();
    m_selection_mask_buffer.write(0, m_selection_mask.data(), static_cast<int>(m_selection_mask.size() * sizeof(uint32_t)));
    m_selection_mask_buffer.release();

    auto end_time = std::chrono::high_resolution_clock::now();

    auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(cpu_time - start_time);
    auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - cpu_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    qDebug() << "LineDataVisualization: Updated selection mask for" << m_selected_lines.size() << "lines in"
             << total_duration.count() << "μs (CPU:" << cpu_duration.count() << "μs, GPU:" << gpu_duration.count() << "μs)";
}

void LineDataVisualization::_updateVisibilityMask() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset all to visible
    std::fill(m_visibility_mask.begin(), m_visibility_mask.end(), 1);

    // Mark hidden lines using fast hash map lookup
    for (auto const & line_id: m_hidden_lines) {
        auto it = m_line_id_to_index.find(line_id);
        if (it != m_line_id_to_index.end()) {
            size_t index = it->second;
            if (index < m_visibility_mask.size()) {
                m_visibility_mask[index] = 0;// Mark as hidden
            }
        }
    }

    // Apply time range filtering if enabled
    if (m_time_range_enabled && !m_line_identifiers.empty()) {
        for (size_t i = 0; i < m_line_identifiers.size() && i < m_visibility_mask.size(); ++i) {
            LineIdentifier const & line_id = m_line_identifiers[i];
            int64_t line_time_frame = line_id.time_frame;

            // Hide lines outside the time range
            if (line_time_frame < m_time_range_start || line_time_frame > m_time_range_end) {
                m_visibility_mask[i] = 0;// Mark as hidden due to time range
            }
        }

        qDebug() << "Applied time range filtering: frames" << m_time_range_start << "to" << m_time_range_end;
    }

    auto cpu_time = std::chrono::high_resolution_clock::now();

    // Update GPU buffer
    m_visibility_mask_buffer.bind();
    m_visibility_mask_buffer.allocate(m_visibility_mask.data(), static_cast<int>(m_visibility_mask.size() * sizeof(uint32_t)));
    m_visibility_mask_buffer.release();

    auto end_time = std::chrono::high_resolution_clock::now();

    auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(cpu_time - start_time);
    auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - cpu_time);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    size_t total_filters = m_hidden_lines.size() + (m_time_range_enabled ? 1 : 0);
    qDebug() << "LineDataVisualization: Updated visibility mask with" << total_filters << "filters in"
             << total_duration.count() << "μs (CPU:" << cpu_duration.count() << "μs, GPU:" << gpu_duration.count() << "μs)";
}

std::vector<LineIdentifier> LineDataVisualization::getAllLinesIntersectingLine(
        int start_x, int start_y, int end_x, int end_y,
        int widget_width, int widget_height,
        QMatrix4x4 const & mvp_matrix, float line_width) {

    if (!m_line_intersection_compute_shader || m_vertex_data.empty()) {
        qDebug() << "LineDataVisualization: Compute shader not available or no vertex data";
        return {};
    }

    // Update line segments buffer if dirty
    if (m_dataIsDirty) {
        qDebug() << "LineDataVisualization: Data is dirty, updating line segments buffer";
        _updateLineSegmentsBuffer();
    } else {
        qDebug() << "LineDataVisualization: Data is clean, using existing segments buffer";
    }

    // Convert screen coordinates to normalized device coordinates [-1, 1]
    float ndc_start_x = (2.0f * start_x / widget_width) - 1.0f;
    float ndc_start_y = 1.0f - (2.0f * start_y / widget_height);// Flip Y
    float ndc_end_x = (2.0f * end_x / widget_width) - 1.0f;
    float ndc_end_y = 1.0f - (2.0f * end_y / widget_height);// Flip Y

    // Create the selection line in NDC space (this is what the compute shader will work with)
    QVector2D query_start(ndc_start_x, ndc_start_y);
    QVector2D query_end(ndc_end_x, ndc_end_y);

    qDebug() << "LineDataVisualization: Screen coords:" << start_x << "," << start_y << "to" << end_x << "," << end_y;
    qDebug() << "LineDataVisualization: NDC coords:" << query_start.x() << "," << query_start.y()
             << "to" << query_end.x() << "," << query_end.y();

    // Reset intersection count to zero
    uint32_t zero = 0;
    m_intersection_count_buffer.bind();
    m_intersection_count_buffer.write(0, &zero, sizeof(uint32_t));
    m_intersection_count_buffer.release();

    // Bind compute shader and set uniforms
    m_line_intersection_compute_shader->bind();
    m_line_intersection_compute_shader->setUniformValue("u_query_line_start", query_start);
    m_line_intersection_compute_shader->setUniformValue("u_query_line_end", query_end);
    m_line_intersection_compute_shader->setUniformValue("u_line_width", line_width * 0.01f);// Scale down for NDC space
    m_line_intersection_compute_shader->setUniformValue("u_mvp_matrix", mvp_matrix);
    m_line_intersection_compute_shader->setUniformValue("u_canvas_size", m_canvas_size);

    // Bind storage buffers
    m_line_segments_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_line_segments_buffer.bufferId());
    m_line_segments_buffer.release();

    m_intersection_results_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_intersection_results_buffer.bufferId());
    m_intersection_results_buffer.release();

    m_intersection_count_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_intersection_count_buffer.bufferId());
    m_intersection_count_buffer.release();

    // Bind visibility mask buffer for compute shader
    m_visibility_mask_buffer.bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_visibility_mask_buffer.bufferId());
    m_visibility_mask_buffer.release();

    // Dispatch compute shader
    uint32_t num_segments = static_cast<uint32_t>(m_segments_data.size() / 5);// 5 floats per segment (x1,y1,x2,y2,line_id)
    uint32_t num_work_groups = (num_segments + 63) / 64;                      // 64 is the local_size_x in the compute shader

    qDebug() << "LineDataVisualization: Dispatching compute shader with" << num_segments << "segments," << num_work_groups << "work groups";
    qDebug() << "LineDataVisualization: segments_data size:" << m_segments_data.size() << "floats";

    if (num_segments == 0) {
        qDebug() << "LineDataVisualization: No segments to process!";
        m_line_intersection_compute_shader->release();
        return {};
    }

    glDispatchCompute(num_work_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_line_intersection_compute_shader->release();

    // Read back results
    m_intersection_count_buffer.bind();
    uint32_t * count_data = static_cast<uint32_t *>(m_intersection_count_buffer.map(QOpenGLBuffer::ReadOnly));
    uint32_t result_count = count_data ? *count_data : 0;
    m_intersection_count_buffer.unmap();
    m_intersection_count_buffer.release();

    qDebug() << "LineDataVisualization: Found" << result_count << "intersecting line segments";

    if (result_count == 0) {
        return {};
    }

    // Read back intersection results
    m_intersection_results_buffer.bind();
    uint32_t * results_data = static_cast<uint32_t *>(m_intersection_results_buffer.map(QOpenGLBuffer::ReadOnly));

    std::vector<LineIdentifier> intersecting_lines;
    std::unordered_set<uint32_t> unique_line_ids;// To avoid duplicates

    if (results_data) {
        for (uint32_t i = 0; i < result_count && i < 100000; ++i) {
            uint32_t line_id = results_data[i];
            if (line_id > 0 && line_id <= m_line_identifiers.size() && unique_line_ids.find(line_id) == unique_line_ids.end()) {
                unique_line_ids.insert(line_id);
                intersecting_lines.push_back(m_line_identifiers[line_id - 1]);// Convert from 1-based to 0-based indexing
            }
        }
    }

    m_intersection_results_buffer.unmap();
    m_intersection_results_buffer.release();

    qDebug() << "LineDataVisualization: Returning" << intersecting_lines.size() << "unique intersecting lines";
    return intersecting_lines;
}

std::optional<LineIdentifier> LineDataVisualization::getLineAtScreenPosition(
        int screen_x, int screen_y, int widget_width, int widget_height) {
    qDebug() << "LineDataVisualization::getLineAtScreenPosition: This method needs MVP matrix, use handleHover instead";
    return std::nullopt;
}

bool LineDataVisualization::handleHover(QPoint const & screen_pos, QSize const & widget_size, QMatrix4x4 const & mvp_matrix) {
    qDebug() << "LineDataVisualization::handleHover: Called with screen pos" << screen_pos.x() << "," << screen_pos.y();

    // Use the new compute shader approach for hover detection
    // Create a small line segment around the point for intersection testing
    int tolerance = 3;// 3 pixel tolerance for hover

    auto intersecting_lines = getAllLinesIntersectingLine(
            screen_pos.x() - tolerance, screen_pos.y() - tolerance,
            screen_pos.x() + tolerance, screen_pos.y() + tolerance,
            widget_size.width(), widget_size.height(),
            mvp_matrix,
            3.0f// Small line width for hover queries
    );

    qDebug() << "LineDataVisualization::handleHover: Found" << intersecting_lines.size() << "intersecting lines";

    bool hover_changed = false;
    if (!intersecting_lines.empty()) {
        LineIdentifier line_id = intersecting_lines[0];// Take the first intersecting line
        if (!m_has_hover_line || !(m_current_hover_line == line_id)) {
            qDebug() << "LineDataVisualization::handleHover: Setting hover line to" << line_id.time_frame << "," << line_id.line_id;
            setHoverLine(line_id);
            hover_changed = true;
        }
    } else {
        if (m_has_hover_line) {
            qDebug() << "LineDataVisualization::handleHover: Clearing hover line";
            setHoverLine(std::nullopt);
            hover_changed = true;
        }
    }

    return hover_changed;
}

//========== Visibility Management ==========

size_t LineDataVisualization::hideSelectedLines() {
    if (m_selected_lines.empty()) {
        return 0;
    }

    size_t hidden_count = 0;

    // Add selected lines to hidden set
    for (auto const & line_id: m_selected_lines) {
        if (m_hidden_lines.insert(line_id).second) {// .second is true if insertion happened
            hidden_count++;
        }
    }

    // Clear selection since hidden lines should not be selected
    m_selected_lines.clear();

    // Update statistics
    m_hidden_line_count = m_hidden_lines.size();

    // Update GPU buffers
    _updateSelectionMask();
    _updateVisibilityMask();

    // Mark view as dirty to trigger re-render
    m_viewIsDirty = true;

    qDebug() << "LineDataVisualization: Hidden" << hidden_count << "lines, total hidden:" << m_hidden_line_count;

    return hidden_count;
}

size_t LineDataVisualization::showAllLines() {
    size_t shown_count = m_hidden_lines.size();

    // Clear all hidden lines
    m_hidden_lines.clear();
    m_hidden_line_count = 0;

    // Update GPU visibility buffer
    _updateVisibilityMask();

    // Mark view as dirty to trigger re-render
    m_viewIsDirty = true;

    qDebug() << "LineDataVisualization: Showed" << shown_count << "lines, all lines now visible";

    return shown_count;
}

std::pair<size_t, size_t> LineDataVisualization::getVisibilityStats() const {
    return {m_total_line_count, m_hidden_line_count};
}

void LineDataVisualization::setTimeRange(int start_frame, int end_frame) {
    qDebug() << "LineDataVisualization::setTimeRange(" << start_frame << "," << end_frame << ")";

    m_time_range_start = start_frame;
    m_time_range_end = end_frame;

    // Update visibility mask to apply time range filtering
    _updateVisibilityMask();

    qDebug() << "Time range updated and visibility mask refreshed";
}

void LineDataVisualization::setTimeRangeEnabled(bool enabled) {
    qDebug() << "LineDataVisualization::setTimeRangeEnabled(" << enabled << ")";

    if (m_time_range_enabled != enabled) {
        m_time_range_enabled = enabled;

        // Update visibility mask to apply or remove time range filtering
        _updateVisibilityMask();

        qDebug() << "Time range filtering" << (enabled ? "enabled" : "disabled");
    }
}

std::tuple<int, int, bool> LineDataVisualization::getTimeRange() const {
    return {m_time_range_start, m_time_range_end, m_time_range_enabled};
}