#include "LineDataVisualization.hpp"

#include "DataManager/Lines/Line_Data.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "ShaderManager/ShaderSourceType.hpp"

#include "Selection/LineSelectionHandler.hpp"
#include "Selection/NoneSelectionHandler.hpp"
#include "Selection/PolygonSelectionHandler.hpp"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QSurfaceFormat>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

LineDataVisualization::LineDataVisualization(QString const & data_key, std::shared_ptr<LineData> const & line_data, GroupManager * group_manager)
    : m_line_data_ptr(line_data),
      m_key(data_key),
      m_scene_framebuffer(nullptr),
      m_line_shader_program(nullptr),
      m_blit_shader_program(nullptr),
      m_line_intersection_compute_shader(nullptr),
      m_group_manager(group_manager) {

    // Do NOT initialize OpenGL functions here; the context may not be current yet on Windows.
    // Defer GL initialization until initializeOpenGLResources() is called with a current context.

    m_color = QVector4D(0.0f, 0.0f, 1.0f, 1.0f);

    buildVertexData();

    // Defer GPU resource creation until a valid 4.3 context is current (typically first render call)
    // initializeOpenGLResources();
    m_dataIsDirty = false;// Data is clean after initial build and buffer creation (to be uploaded during GL init)
}

LineDataVisualization::~LineDataVisualization() {
    cleanupOpenGLResources();
}

void LineDataVisualization::buildVertexData() {
    m_vertex_data.clear();
    m_line_vertex_ranges.clear();
    m_line_entity_ids.clear();
    m_entity_id_per_vertex.clear();

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
        auto const & ids_at_time = m_line_data_ptr->getEntityIdsAtTime(time_frame);
        for (int line_id = 0; line_id < static_cast<int>(lines.size()); ++line_id) {
            Line2D const & line = lines[line_id];

            if (line.size() < 2) {
                continue;
            }

            // Record line-level EntityId
            EntityId entity_id = 0;
            if (line_id < static_cast<int>(ids_at_time.size())) {
                entity_id = ids_at_time[line_id];
            }
            m_line_entity_ids.push_back(entity_id);

            uint32_t line_start_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);

            // Convert line strip to line segments (pairs of consecutive vertices)
            for (size_t i = 0; i < line.size() - 1; ++i) {
                Point2D<float> const & p0 = line[i];
                Point2D<float> const & p1 = line[i + 1];

                segment_vertices.push_back(p0.x);
                segment_vertices.push_back(p0.y);
                segment_line_ids.push_back(line_index + 1);// Use 1-based indexing for picking
                m_entity_id_per_vertex.push_back(m_line_entity_ids.back());

                segment_vertices.push_back(p1.x);
                segment_vertices.push_back(p1.y);
                segment_line_ids.push_back(line_index + 1);// Use 1-based indexing for picking
                m_entity_id_per_vertex.push_back(m_line_entity_ids.back());
            }

            uint32_t line_end_vertex = static_cast<uint32_t>(segment_vertices.size() / 2);
            uint32_t line_vertex_count = line_end_vertex - line_start_vertex;

            m_line_vertex_ranges.push_back({line_start_vertex, line_vertex_count});

            line_index++;
        }
    }

    m_vertex_data = std::move(segment_vertices);
    m_line_id_data = std::move(segment_line_ids);

    qDebug() << "LineDataVisualization: Built" << m_line_entity_ids.size()
             << "lines with" << m_vertex_data.size() / 4 << "segments ("
             << m_vertex_data.size() / 2 << "vertices)";

    // Build fast lookup map from EntityId to index
    m_entity_id_to_index.clear();
    for (size_t i = 0; i < m_line_entity_ids.size(); ++i) {
        EntityId entity_id = m_line_entity_ids[i];
        if (entity_id != 0) {  // Only map non-zero entity IDs
            m_entity_id_to_index[entity_id] = i;
        }
    }

    m_total_line_count = m_line_entity_ids.size();
    m_hidden_line_count = m_hidden_lines.size();
}

void LineDataVisualization::initializeOpenGLResources() {
    if (m_gl_initialized) {
        return;
    }

    QOpenGLContext * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning() << "LineDataVisualization: No current OpenGL context; deferring GL resource init";
        return;
    }

    QSurfaceFormat fmt = ctx->format();
    if ((fmt.majorVersion() < 4) || (fmt.majorVersion() == 4 && fmt.minorVersion() < 3)) {
        qWarning() << "LineDataVisualization: Requires OpenGL 4.3 core for compute shader, current is"
                   << fmt.majorVersion() << "." << fmt.minorVersion() << "- skipping init";
        return;
    }

    if (!m_gl_initialized) {
        if (!initializeOpenGLFunctions()) {
            qWarning() << "LineDataVisualization: initializeOpenGLFunctions() failed";
            return;
        }
        m_gl_initialized = true;
    }

    // Create vertex buffer
    m_vertex_buffer.create();
    m_line_id_buffer.create();
    m_group_id_buffer.create();

    m_vertex_array_object.create();
    m_vertex_array_object.bind();

    m_vertex_buffer.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    m_line_id_buffer.bind();
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
    m_line_id_buffer.release();

    // Attribute 2: group palette index as float
    m_group_id_buffer.bind();
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);

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

    // Load compute shader through ShaderManager for proper context management
    if (!shader_manager.getProgram("line_intersection_compute")) {
        bool success = shader_manager.loadComputeProgram("line_intersection_compute", 
                                                        ":/shaders/line_intersection.comp", 
                                                        ShaderSourceType::Resource);
        if (!success) {
            qDebug() << "Failed to load line_intersection_compute shader!";
            m_line_intersection_compute_shader = nullptr;
        } else {
            qDebug() << "Successfully loaded line_intersection_compute shader through ShaderManager";
        }
    }
    auto * compute_program = shader_manager.getProgram("line_intersection_compute");
    if (compute_program) {
        m_line_intersection_compute_shader = compute_program->getNativeProgram();
    } else {
        qDebug() << "line_intersection_compute shader is null!";
        m_line_intersection_compute_shader = nullptr;
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

    m_gl_initialized = true;
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

    // Note: m_line_intersection_compute_shader is now managed by ShaderManager
    // Don't delete it manually - ShaderManager will handle cleanup
    m_line_intersection_compute_shader = nullptr;

    m_gl_initialized = false;
}

void LineDataVisualization::updateOpenGLBuffers() {
    m_vertex_buffer.bind();
    m_vertex_buffer.allocate(m_vertex_data.data(), static_cast<int>(m_vertex_data.size() * sizeof(float)));
    m_vertex_buffer.release();

    m_line_id_buffer.bind();
    m_line_id_buffer.allocate(m_line_id_data.data(), static_cast<int>(m_line_id_data.size() * sizeof(uint32_t)));
    m_line_id_buffer.release();

    // Update group palette indices buffer (compute or default to 0)
    _updateGroupVertexData();

    // Initialize selection mask (all unselected initially)
    m_selection_mask.assign(m_line_entity_ids.size(), 0);
    m_selection_mask_buffer.bind();
    m_selection_mask_buffer.allocate(m_selection_mask.data(), static_cast<int>(m_selection_mask.size() * sizeof(uint32_t)));
    m_selection_mask_buffer.release();

    // Initialize visibility mask (all visible initially, except those in hidden_lines set)
    m_visibility_mask.assign(m_line_entity_ids.size(), 1);// Default to visible
    _updateVisibilityMask();                               // Apply any existing hidden lines

    // Update line segments buffer for compute shader
    _updateLineSegmentsBuffer();
}

void LineDataVisualization::render(QMatrix4x4 const & mvp_matrix, float line_width) {
    // Lazy-init GL if needed and possible
    if (!m_gl_initialized) {
        initializeOpenGLResources();
        if (!m_gl_initialized) {
            // Cannot render without GL resources (no context or insufficient GL version)
            return;
        }
    }

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

    if (m_viewIsDirty || m_group_data_needs_update) {
        if (m_group_data_needs_update) {
            _updateGroupVertexData();
            m_group_data_needs_update = false;
        }
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

    // Set group palette uniform array (256 entries). If group manager is null, default to m_color
    {
        std::vector<QVector4D> group_colors(256, m_color);
        if (m_group_manager) {
            auto const & groups = m_group_manager->getGroups();
            int color_index = 1;
            for (auto it = groups.begin(); it != groups.end() && color_index < 256; ++it) {
                auto const & group = it.value();
                group_colors[color_index] = QVector4D(group.color.redF(), group.color.greenF(), group.color.blueF(), group.color.alphaF());
                color_index++;
            }
        }
        shader_program->setUniformValueArray("u_group_colors", group_colors.data(), 256);
        shader_program->setUniformValue("u_num_groups", 256);
    }

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

void LineDataVisualization::setHoverLine(std::optional<EntityId> entity_id) {
    if (entity_id.has_value()) {
        m_current_hover_line = entity_id.value();
        m_has_hover_line = true;

        // Cache the line index to avoid expensive lookup during rendering
        auto it = m_entity_id_to_index.find(m_current_hover_line);
        if (it != m_entity_id_to_index.end()) {
            m_cached_hover_line_index = static_cast<uint32_t>(it->second);
        } else {
            m_has_hover_line = false;// Invalid EntityId
        }
    } else {
        m_has_hover_line = false;
        m_cached_hover_line_index = 0;
    }
}

std::optional<EntityId> LineDataVisualization::getHoverLine() const {
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
        std::cout << "LineDataVisualization::applySelection: selection_handler is not a supported type (PolygonSelectionHandler or LineSelectionHandler)" << std::endl;
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

    // Ensure GL is ready; compute shader requires 4.3
    if (!m_gl_initialized) {
        initializeOpenGLResources();
        if (!m_gl_initialized) {
            qDebug() << "LineDataVisualization::applySelection: GL resources not initialized (no 4.3 context), skipping";
            return;
        }
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

    return QString("Dataset: %1\nEntityId: %2")
            .arg(m_key)
            .arg(m_current_hover_line);
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
    for (auto const & entity_id: m_selected_lines) {
        auto it = m_entity_id_to_index.find(entity_id);
        if (it != m_entity_id_to_index.end()) {
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
    for (auto const & entity_id: m_hidden_lines) {
        auto it = m_entity_id_to_index.find(entity_id);
        if (it != m_entity_id_to_index.end()) {
            size_t index = it->second;
            if (index < m_visibility_mask.size()) {
                m_visibility_mask[index] = 0;// Mark as hidden
            }
        }
    }

    // TODO: Time range filtering needs to be reimplemented with EntityId
    // For now, time filtering is disabled since EntityId doesn't contain time information
    if (m_time_range_enabled) {
        qDebug() << "Time range filtering not yet implemented for EntityId-based system";
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

void LineDataVisualization::_updateGroupVertexData() {
    // Build per-vertex palette indices (float) from per-line EntityIds and GroupManager
    std::vector<float> palette_indices;
    palette_indices.reserve(m_entity_id_per_vertex.size());

    if (!m_group_manager || m_entity_id_per_vertex.empty()) {
        // Default all to 0 (ungrouped)
        palette_indices.assign(m_vertex_data.size() / 2, 0.0f);
    } else {
        // Map group IDs to palette slots [1..255], 0 = ungrouped
        std::unordered_map<int, int> group_id_to_slot;
        int next_slot = 1;

        for (auto const eid: m_entity_id_per_vertex) {
            int gid = m_group_manager->getEntityGroup(eid);
            if (gid == -1) {
                palette_indices.push_back(0.0f);
                continue;
            }
            auto it = group_id_to_slot.find(gid);
            int slot;
            if (it == group_id_to_slot.end()) {
                slot = (next_slot < 256 ? next_slot++ : 255);
                group_id_to_slot.emplace(gid, slot);
            } else {
                slot = it->second;
            }
            palette_indices.push_back(static_cast<float>(slot));
        }
    }

    // Upload to GPU buffer
    m_group_id_buffer.bind();
    m_group_id_buffer.allocate(palette_indices.data(), static_cast<int>(palette_indices.size() * sizeof(float)));
    m_group_id_buffer.release();
}

std::vector<EntityId> LineDataVisualization::getAllLinesIntersectingLine(
        int start_x, int start_y, int end_x, int end_y,
        int widget_width, int widget_height,
        QMatrix4x4 const & mvp_matrix, float line_width) {

    // Ensure GL resources and 4.3 context are available
    if (!m_gl_initialized) {
        initializeOpenGLResources();
        if (!m_gl_initialized) {
            qDebug() << "LineDataVisualization: Compute shader not available due to missing 4.3 context";
            return {};
        }
    }

    // Check if compute shader needs to be (re)created for this context
    if (!m_line_intersection_compute_shader) {
        qDebug() << "LineDataVisualization: Compute shader is null, attempting to get/create it via ShaderManager";
        
        ShaderManager & shader_manager = ShaderManager::instance();
        auto * compute_program = shader_manager.getProgram("line_intersection_compute");
        if (!compute_program) {
            qDebug() << "LineDataVisualization: Compute program not found in ShaderManager, loading it";
            bool success = shader_manager.loadComputeProgram("line_intersection_compute", 
                                                            ":/shaders/line_intersection.comp", 
                                                            ShaderSourceType::Resource);
            if (!success) {
                qDebug() << "Failed to load line_intersection_compute shader via ShaderManager!";
                return {};
            }
            compute_program = shader_manager.getProgram("line_intersection_compute");
        }
        
        if (compute_program) {
            m_line_intersection_compute_shader = compute_program->getNativeProgram();
            qDebug() << "Successfully got/loaded line_intersection_compute shader via ShaderManager";
        } else {
            qDebug() << "Failed to get compute shader from ShaderManager";
            return {};
        }
    }
    
    if (m_vertex_data.empty()) {
        qDebug() << "LineDataVisualization: No vertex data available";
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
    float tolerance = std::max(line_width * 0.01f, 0.05f);  // Minimum tolerance of 0.05 in NDC space
    m_line_intersection_compute_shader->bind();
    m_line_intersection_compute_shader->setUniformValue("u_query_line_start", query_start);
    m_line_intersection_compute_shader->setUniformValue("u_query_line_end", query_end);
    m_line_intersection_compute_shader->setUniformValue("u_line_width", tolerance);// Scale down for NDC space
    m_line_intersection_compute_shader->setUniformValue("u_mvp_matrix", mvp_matrix);
    m_line_intersection_compute_shader->setUniformValue("u_canvas_size", m_canvas_size);
    
    // Provide explicit sizes to shader to avoid driver-dependent SSBO length behavior
    uint32_t total_segments = static_cast<uint32_t>(m_segments_data.size() / 5);
    m_line_intersection_compute_shader->setUniformValue("u_total_segments", total_segments);
    m_line_intersection_compute_shader->setUniformValue("u_visibility_count", static_cast<GLuint>(m_visibility_mask.size()));
    m_line_intersection_compute_shader->setUniformValue("u_results_capacity", static_cast<GLuint>(100000));
    
    qDebug() << "LineDataVisualization: Using tolerance:" << tolerance << "(line_width:" << line_width << ")";
    qDebug() << "LineDataVisualization: Canvas size:" << m_canvas_size.x() << "x" << m_canvas_size.y();
    qDebug() << "LineDataVisualization: MVP matrix:";
    for (int row = 0; row < 4; ++row) {
        qDebug() << "  [" << mvp_matrix(row, 0) << mvp_matrix(row, 1) << mvp_matrix(row, 2) << mvp_matrix(row, 3) << "]";
    }
    qDebug() << "LineDataVisualization: Visibility mask size:" << m_visibility_mask.size() << "first few values:";
    for (size_t i = 0; i < std::min(m_visibility_mask.size(), size_t(10)); ++i) {
        qDebug() << "  visibility_mask[" << i << "] =" << m_visibility_mask[i];
    }

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

    // Dispatch compute shader in batches to respect hardware limits
    uint32_t num_segments = static_cast<uint32_t>(m_segments_data.size() / 5);// 5 floats per segment (x1,y1,x2,y2,line_id)

    // Query hardware limits
    GLint max_work_groups_x = 65535; // Safe default per spec minimum
    GLint queried = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_COUNT, &queried); // Not the correct query; we'll prefer glGetIntegeri_v below
    // Prefer indexed query if available
    GLint max_count_x = 0;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_count_x);
    if (max_count_x > 0) {
        max_work_groups_x = max_count_x;
    }

    // local_size_x is 64 per shader
    const uint32_t local_size_x = 64u;
    uint64_t max_invocations_per_dispatch = static_cast<uint64_t>(max_work_groups_x) * static_cast<uint64_t>(local_size_x);
    if (max_invocations_per_dispatch == 0) {
        max_invocations_per_dispatch = 65535ull * local_size_x; // fallback
    }

    qDebug() << "LineDataVisualization: Dispatching compute shader with" << num_segments << "segments in batches, max workgroups x per dispatch:" << max_work_groups_x;
    qDebug() << "LineDataVisualization: segments_data size:" << m_segments_data.size() << "floats";
    
    // Debug: show first few line segments to check coordinates
    qDebug() << "LineDataVisualization: First few line segments (world coords):";
    for (size_t i = 0; i < std::min(m_segments_data.size(), size_t(25)); i += 5) {
        if (i + 4 < m_segments_data.size()) {
            float x1 = m_segments_data[i];
            float y1 = m_segments_data[i + 1]; 
            float x2 = m_segments_data[i + 2];
            float y2 = m_segments_data[i + 3];
            uint32_t line_id = *reinterpret_cast<const uint32_t*>(&m_segments_data[i + 4]);
            qDebug() << "  Segment" << (i/5) << ": (" << x1 << "," << y1 << ") to (" << x2 << "," << y2 << ") line_id:" << line_id;
        }
    }

    if (num_segments == 0) {
        qDebug() << "LineDataVisualization: No segments to process!";
        m_line_intersection_compute_shader->release();
        return {};
    }

    // Batched dispatch loop
    uint64_t remaining = num_segments;
    uint64_t offset = 0;
    while (remaining > 0) {
        uint64_t batch = std::min<uint64_t>(remaining, max_invocations_per_dispatch);
        uint32_t groups_x = static_cast<uint32_t>((batch + local_size_x - 1) / local_size_x);

        // Set batching uniforms
        m_line_intersection_compute_shader->setUniformValue("u_segment_offset", static_cast<GLuint>(offset));
        m_line_intersection_compute_shader->setUniformValue("u_segments_in_batch", static_cast<GLuint>(batch));

        glDispatchCompute(groups_x, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        offset += batch;
        remaining -= batch;
    }

    // Ensure all writes are visible before mapping
    glFinish();

    m_line_intersection_compute_shader->release();

    // Read back results
    m_intersection_count_buffer.bind();
    uint32_t * count_data = static_cast<uint32_t *>(m_intersection_count_buffer.map(QOpenGLBuffer::ReadOnly));
    uint32_t result_count = count_data ? *count_data : 0;
    m_intersection_count_buffer.unmap();
    m_intersection_count_buffer.release();

    qDebug() << "LineDataVisualization: Found" << result_count << "intersecting line segments";

    if (result_count == 0) {
        // Optional CPU-side validation when enabled
        if (qEnvironmentVariableIsSet("WT_DEBUG_COMPUTE_VALIDATE")) {
            auto cpuDistancePointToSegment = [](QVector2D const & p, QVector2D const & a, QVector2D const & b) {
                QVector2D ab = b - a;
                float len2 = QVector2D::dotProduct(ab, ab);
                if (len2 == 0.0f) return (p - a).length();
                float t = std::max(0.0f, std::min(1.0f, QVector2D::dotProduct(p - a, ab) / len2));
                QVector2D proj = a + t * ab;
                return (p - proj).length();
            };
            auto cpuSegmentsIntersect = [&](QVector2D const & a1, QVector2D const & a2, QVector2D const & b1, QVector2D const & b2, float tol) {
                if (cpuDistancePointToSegment(a1, b1, b2) <= tol) return true;
                if (cpuDistancePointToSegment(a2, b1, b2) <= tol) return true;
                if (cpuDistancePointToSegment(b1, a1, a2) <= tol) return true;
                if (cpuDistancePointToSegment(b2, a1, a2) <= tol) return true;
                // exact intersection test
                auto cross = [](QVector2D const & u, QVector2D const & v) { return u.x() * v.y() - u.y() * v.x(); };
                QVector2D r = a2 - a1; QVector2D s = b2 - b1;
                float denom = cross(r, s);
                if (std::abs(denom) < 1e-6f) return false;
                QVector2D diff = b1 - a1;
                float t = cross(diff, s) / denom;
                float u = cross(diff, r) / denom;
                return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
            };
            auto toNdc = [&](float x, float y) {
                QVector4D w(x, y, 0.0f, 1.0f);
                QVector4D ndc4 = mvp_matrix * w;
                return QVector2D(ndc4.x() / ndc4.w(), ndc4.y() / ndc4.w());
            };
            QVector2D a(query_start), b(query_end);
            size_t cpu_hits = 0;
            size_t inspect = std::min<size_t>(m_segments_data.size() / 5, 20000);
            for (size_t i = 0; i < inspect; ++i) {
                size_t base = i * 5;
                float x1 = m_segments_data[base + 0];
                float y1 = m_segments_data[base + 1];
                float x2 = m_segments_data[base + 2];
                float y2 = m_segments_data[base + 3];
                QVector2D p = toNdc(x1, y1);
                QVector2D q = toNdc(x2, y2);
                if (cpuSegmentsIntersect(a, b, p, q, tolerance)) cpu_hits++;
            }
            qDebug() << "CPU validation: checked" << inspect << "segments, hits =" << cpu_hits;
        }
        return {};
    }

    // Read back intersection results
    m_intersection_results_buffer.bind();
    uint32_t * results_data = static_cast<uint32_t *>(m_intersection_results_buffer.map(QOpenGLBuffer::ReadOnly));

    std::vector<EntityId> intersecting_lines;
    std::unordered_set<uint32_t> unique_line_ids;// To avoid duplicates

    if (results_data) {
        for (uint32_t i = 0; i < result_count && i < 100000; ++i) {
            uint32_t line_id = results_data[i];
            if (line_id > 0 && line_id <= m_line_entity_ids.size() && unique_line_ids.find(line_id) == unique_line_ids.end()) {
                unique_line_ids.insert(line_id);
                EntityId entity_id = m_line_entity_ids[line_id - 1];// Convert from 1-based to 0-based indexing
                if (entity_id != 0) {  // Only add valid EntityIds
                    intersecting_lines.push_back(entity_id);
                }
            }
        }
    }

    m_intersection_results_buffer.unmap();
    m_intersection_results_buffer.release();

    qDebug() << "LineDataVisualization: Returning" << intersecting_lines.size() << "unique intersecting lines";
    return intersecting_lines;
}

std::optional<EntityId> LineDataVisualization::getLineAtScreenPosition(
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
        EntityId entity_id = intersecting_lines[0];// Take the first intersecting line
        if (!m_has_hover_line || m_current_hover_line != entity_id) {
            qDebug() << "LineDataVisualization::handleHover: Setting hover line to EntityId" << entity_id;
            setHoverLine(entity_id);
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
