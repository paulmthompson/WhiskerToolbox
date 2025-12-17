#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "CorePlotting/CoordinateTransform/SeriesMatrices.hpp"
#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/CoordinateTransform/SeriesCoordinateQuery.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/Layout/NormalizationHelpers.hpp"
#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "DataManager/utils/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"
#include "PlottingOpenGL/ShaderManager/ShaderManager.hpp"
#include "SceneBuildingHelpers.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
#include <QPointer>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <ranges>

// ============================================================================
// Widget-specific transform composition helpers
// ============================================================================
// These functions compose data normalization + layout + user adjustments into
// a single LayoutTransform. This logic is widget-specific and doesn't belong
// in the CorePlotting library (per DESIGN.md guidelines).

namespace {

/**
 * @brief Compose Y transform for analog series rendering
 * 
 * Follows the pattern from CorePlotting/DESIGN.md:
 * 1. Data normalization (z-score style: maps ±3σ to ±1)
 * 2. User adjustments (intrinsic scale, user scale, vertical offset)
 * 3. Layout positioning (from LayoutEngine)
 * 4. Global scaling (from ViewState)
 */
[[nodiscard]] CorePlotting::LayoutTransform composeAnalogYTransform(
        CorePlotting::SeriesLayout const& layout,
        float data_mean,
        float std_dev,
        float intrinsic_scale,
        float user_scale_factor,
        float user_vertical_offset,
        float global_zoom,
        float global_vertical_scale) {
    
    // Use NormalizationHelpers to create the data normalization transform
    // forStdDevRange maps mean ± 3*std_dev to ±1
    auto data_norm = CorePlotting::NormalizationHelpers::forStdDevRange(data_mean, std_dev, 3.0f);
    
    // User adjustments: additional scaling and offset
    auto user_adj = CorePlotting::NormalizationHelpers::manual(
        intrinsic_scale * user_scale_factor,
        user_vertical_offset
    );
    
    // Layout provides: offset = center, gain = half_height
    // Apply 80% margin factor within allocated space
    constexpr float margin_factor = 0.8f;
    CorePlotting::LayoutTransform layout_with_margin{
        layout.y_transform.offset,
        layout.y_transform.gain * margin_factor
    };
    
    // Global scaling
    auto global_adj = CorePlotting::NormalizationHelpers::manual(
        global_zoom * global_vertical_scale,
        0.0f
    );
    
    // Compose in order: data_norm -> user_adj -> layout -> global
    // Result = global.compose(layout.compose(user_adj.compose(data_norm)))
    return global_adj.compose(layout_with_margin.compose(user_adj.compose(data_norm)));
}

/**
 * @brief Compose Y transform for event series (stacked mode)
 */
[[nodiscard]] CorePlotting::LayoutTransform composeEventYTransform(
        CorePlotting::SeriesLayout const& layout,
        float margin_factor,
        float global_vertical_scale) {
    
    // Events map [-1, 1] to allocated space with margin
    float const half_height = layout.y_transform.gain * margin_factor * 0.5f * global_vertical_scale;
    float const center = layout.y_transform.offset;
    
    return CorePlotting::LayoutTransform{center, half_height};
}

/**
 * @brief Compose Y transform for event series (full canvas mode)
 */
[[nodiscard]] CorePlotting::LayoutTransform composeEventFullCanvasYTransform(
        float viewport_y_min,
        float viewport_y_max,
        float margin_factor) {
    
    // Full canvas: map [-1, 1] to viewport bounds with margin
    float const height = (viewport_y_max - viewport_y_min) * margin_factor;
    float const center = (viewport_y_max + viewport_y_min) * 0.5f;
    float const half_height = height * 0.5f;
    
    return CorePlotting::LayoutTransform{center, half_height};
}

/**
 * @brief Compose Y transform for interval series
 */
[[nodiscard]] CorePlotting::LayoutTransform composeIntervalYTransform(
        CorePlotting::SeriesLayout const& layout,
        float margin_factor,
        float global_zoom,
        float global_vertical_scale) {
    
    // Intervals map [-1, 1] to allocated space with margin and global scaling
    float const half_height = layout.y_transform.gain * margin_factor * 0.5f * 
                             global_zoom * global_vertical_scale;
    float const center = layout.y_transform.offset;
    
    return CorePlotting::LayoutTransform{center, half_height};
}

} // anonymous namespace


OpenGLWidget::OpenGLWidget(QWidget * parent)
    : QOpenGLWidget(parent) {
    setMouseTracking(true);// Enable mouse tracking for hover events

    // Initialize tooltip timer
    _tooltip_timer = new QTimer(this);
    _tooltip_timer->setSingleShot(true);
    _tooltip_timer->setInterval(TOOLTIP_DELAY_MS);
    connect(_tooltip_timer, &QTimer::timeout, this, [this]() {
        showSeriesInfoTooltip(_tooltip_hover_pos);
    });
}

OpenGLWidget::~OpenGLWidget() {
    // Disconnect the context destruction signal BEFORE cleanup to prevent lambda from accessing destroyed object
    if (_ctxAboutToBeDestroyedConn) {
        disconnect(_ctxAboutToBeDestroyedConn);
        _ctxAboutToBeDestroyedConn = QMetaObject::Connection();
    }
    cleanup();
}

void OpenGLWidget::updateCanvas(int time) {
    _time = time;
    // Mark layout and scene as dirty since external changes may have occurred
    // (e.g., display_mode changes, series visibility changes)
    _layout_response_dirty = true;
    _scene_dirty = true;
    //std::cout << "Redrawing at " << _time << std::endl;
    update();
}

// Add these implementations:
void OpenGLWidget::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        // Check if we're clicking near an interval edge for dragging
        auto edge_info = findIntervalEdgeAtPosition(static_cast<float>(event->pos().x()), static_cast<float>(event->pos().y()));
        if (edge_info.has_value()) {
            auto const [series_key, is_left_edge] = edge_info.value();
            startIntervalDrag(series_key, is_left_edge, event->pos());
            return;// Don't start panning when dragging intervals
        }

        _isPanning = true;
        _lastMousePos = event->pos();

        // Emit click coordinates for interval selection
        float const canvas_x = static_cast<float>(event->pos().x());
        float const canvas_y = static_cast<float>(event->pos().y());

        // Convert canvas X to time coordinate
        float const time_coord = canvasXToTime(canvas_x);

        // Find the closest analog series for Y coordinate conversion (similar to mouseMoveEvent)
        QString series_info = "";
        if (!_analog_series.empty()) {
            for (auto const & [key, data]: _analog_series) {
                if (data.display_options->style.is_visible) {
                    float const analog_value = canvasYToAnalogValue(canvas_y, key);
                    series_info = QString("Series: %1, Value: %2").arg(QString::fromStdString(key)).arg(analog_value, 0, 'f', 3);
                    break;
                }
            }
        }

        emit mouseClick(time_coord, canvas_y, series_info);
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
    if (_interval_drag_controller.isActive()) {
        // Update interval drag
        updateIntervalDrag(event->pos());
        cancelTooltipTimer();// Cancel tooltip during drag
        return;              // Don't do other mouse move processing while dragging
    }

    if (_is_creating_new_interval) {
        // Update new interval creation
        updateNewIntervalCreation(event->pos());
        cancelTooltipTimer();// Cancel tooltip during interval creation
        return;              // Don't do other mouse move processing while creating
    }

    if (_isPanning) {
        // Calculate vertical movement in pixels
        int const deltaY = event->pos().y() - _lastMousePos.y();

        // Convert to normalized device coordinates
        // A positive deltaY (moving down) should move the view up
        float const normalizedDeltaY = -1.0f * static_cast<float>(deltaY) / static_cast<float>(height()) * 2.0f;

        // Adjust vertical offset based on movement using ViewState method
        _view_state.applyVerticalPanDelta(normalizedDeltaY);

        _lastMousePos = event->pos();
        update();            // Request redraw
        cancelTooltipTimer();// Cancel tooltip during panning
    } else {
        // Check for cursor changes when hovering near interval edges
        auto edge_info = findIntervalEdgeAtPosition(static_cast<float>(event->pos().x()), static_cast<float>(event->pos().y()));
        if (edge_info.has_value()) {
            setCursor(Qt::SizeHorCursor);
            cancelTooltipTimer();// Don't show tooltip when hovering over interval edges
        } else {
            setCursor(Qt::ArrowCursor);
            // Start tooltip timer for series info
            startTooltipTimer(event->pos());
        }
    }

    // Emit hover coordinates for coordinate display
    float const canvas_x = static_cast<float>(event->pos().x());
    float const canvas_y = static_cast<float>(event->pos().y());

    // Convert canvas X to time coordinate
    float const time_coord = canvasXToTime(canvas_x);

    // Find the closest analog series for Y coordinate conversion
    QString series_info = "";
    if (!_analog_series.empty()) {
        // For now, use the first visible analog series for Y coordinate conversion
        for (auto const & [key, data]: _analog_series) {
            if (data.display_options->style.is_visible) {
                float const analog_value = canvasYToAnalogValue(canvas_y, key);
                series_info = QString("Series: %1, Value: %2").arg(QString::fromStdString(key)).arg(analog_value, 0, 'f', 3);
                break;
            }
        }
    }

    emit mouseHover(time_coord, canvas_y, series_info);

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        if (_interval_drag_controller.isActive()) {
            finishIntervalDrag();
        } else if (_is_creating_new_interval) {
            finishNewIntervalCreation();
        } else {
            _isPanning = false;
        }
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLWidget::leaveEvent(QEvent * event) {
    // Cancel tooltip when mouse leaves the widget
    cancelTooltipTimer();
    QOpenGLWidget::leaveEvent(event);
}

void OpenGLWidget::setBackgroundColor(std::string const & hexColor) {
    m_background_color = hexColor;
    updateCanvas(_time);
}

void OpenGLWidget::setPlotTheme(PlotTheme theme) {
    _plot_theme = theme;

    if (theme == PlotTheme::Dark) {
        // Dark theme: black background, white axes
        m_background_color = "#000000";
        m_axis_color = "#FFFFFF";
    } else {
        // Light theme: white background, dark axes
        m_background_color = "#FFFFFF";
        m_axis_color = "#333333";
    }

    updateCanvas(_time);
}

void OpenGLWidget::cleanup() {
    // Avoid re-entrancy or cleanup without a valid context
    if (!_gl_initialized) {
        return;
    }

    // Guard: QOpenGLContext may already be gone during teardown
    if (QOpenGLContext::currentContext() == nullptr && context() == nullptr) {
        _gl_initialized = false;
        return;
    }

    // Safe to release our GL resources
    makeCurrent();

    if (m_program) {
        delete m_program;
        m_program = nullptr;
    }
    if (m_dashedProgram) {
        delete m_dashedProgram;
        m_dashedProgram = nullptr;
    }

    // Cleanup PlottingOpenGL SceneRenderer
    if (_scene_renderer) {
        _scene_renderer->cleanup();
        _scene_renderer.reset();
    }

    m_vbo.destroy();
    m_vao.destroy();

    doneCurrent();

    _gl_initialized = false;
}

void OpenGLWidget::initializeGL() {
    // Ensure QOpenGLFunctions is initialized
    initializeOpenGLFunctions();

    // Track GL init and connect context destruction
    _gl_initialized = true;
    if (context()) {
        // Disconnect any previous connection to avoid duplicates
        if (_ctxAboutToBeDestroyedConn) {
            disconnect(_ctxAboutToBeDestroyedConn);
        }
        // Use QPointer for safer object lifetime tracking
        QPointer<OpenGLWidget> self(this);
        _ctxAboutToBeDestroyedConn = connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [self]() {
            // Only call cleanup if the widget still exists
            if (self) {
                self->cleanup();
            }
        });
    }

    auto fmt = format();
    std::cout << "OpenGL major version: " << fmt.majorVersion() << std::endl;
    std::cout << "OpenGL minor version: " << fmt.minorVersion() << std::endl;
    int r, g, b;
    hexToRGB(m_background_color, r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Load axes shader
    ShaderManager::instance().loadProgram(
            "axes",
            m_shaderSourceType == ShaderSourceType::Resource ? ":/shaders/colored_vertex.vert" : "src/WhiskerToolbox/shaders/colored_vertex.vert",
            m_shaderSourceType == ShaderSourceType::Resource ? ":/shaders/colored_vertex.frag" : "src/WhiskerToolbox/shaders/colored_vertex.frag",
            "",
            m_shaderSourceType);
    // Load dashed line shader
    ShaderManager::instance().loadProgram(
            "dashed_line",
            m_shaderSourceType == ShaderSourceType::Resource ? ":/shaders/dashed_line.vert" : "src/WhiskerToolbox/shaders/dashed_line.vert",
            m_shaderSourceType == ShaderSourceType::Resource ? ":/shaders/dashed_line.frag" : "src/WhiskerToolbox/shaders/dashed_line.frag",
            "",
            m_shaderSourceType);

    // Get uniform locations for axes shader
    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) {
        auto nativeProgram = axesProgram->getNativeProgram();
        if (nativeProgram) {
            m_projMatrixLoc = nativeProgram->uniformLocation("projMatrix");
            m_viewMatrixLoc = nativeProgram->uniformLocation("viewMatrix");
            m_modelMatrixLoc = nativeProgram->uniformLocation("modelMatrix");
            m_colorLoc = nativeProgram->uniformLocation("u_color");
            m_alphaLoc = nativeProgram->uniformLocation("u_alpha");
        }
    }

    // Get uniform locations for dashed line shader
    auto dashedProgram = ShaderManager::instance().getProgram("dashed_line");
    if (dashedProgram) {
        auto nativeProgram = dashedProgram->getNativeProgram();
        if (nativeProgram) {
            m_dashedProjMatrixLoc = nativeProgram->uniformLocation("u_mvp");
            m_dashedResolutionLoc = nativeProgram->uniformLocation("u_resolution");
            m_dashedDashSizeLoc = nativeProgram->uniformLocation("u_dashSize");
            m_dashedGapSizeLoc = nativeProgram->uniformLocation("u_gapSize");
        }
    }

    // Connect reload signal to redraw
    connect(&ShaderManager::instance(), &ShaderManager::shaderReloaded, this, [this](std::string const &) { update(); });
    m_vao.create();
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    setupVertexAttribs();

    // Initialize PlottingOpenGL SceneRenderer
    _scene_renderer = std::make_unique<PlottingOpenGL::SceneRenderer>();
    if (!_scene_renderer->initialize()) {
        std::cerr << "Warning: Failed to initialize SceneRenderer" << std::endl;
        _scene_renderer.reset();
    }
}

void OpenGLWidget::setupVertexAttribs() {

    m_vbo.bind();                     // glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
    int const vertex_argument_num = 4;// Position (x, y, 0, 1) for axes shader

    // Attribute 0: vertex positions (x, y, 0, 1) for axes shader
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), nullptr);

    // Disable unused vertex attributes
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    m_vbo.release();
}

///////////////////////////////////////////////////////////////////////////////
// All series rendering now uses SceneRenderer path via renderWithSceneRenderer()
// See SceneBuildingHelpers for batch construction and PlottingOpenGL for rendering
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::paintGL() {
    int r, g, b;
    hexToRGB(m_background_color, r, g, b);
    glClearColor(
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //This has been converted to master coordinates
    int const currentTime = _time;

    // Keep the current zoom level (range width) and center on current time
    int64_t const zoom = _view_state.time_range.getWidth();
    _view_state.time_range.setCenterAndZoom(currentTime, zoom);

    if (_scene_renderer && _scene_renderer->isInitialized()) {
        renderWithSceneRenderer();
    }

    drawAxis();

    drawGridLines();

    // Overlay rendering for interactive states (uses legacy shader path)
    // These are temporary overlays during user interaction
    drawDraggedInterval();
    drawNewIntervalBeingCreated();
}

void OpenGLWidget::resizeGL(int w, int h) {

    // Set the viewport to match the widget dimensions
    // This is crucial for proper scaling - it tells OpenGL the actual pixel dimensions
    glViewport(0, 0, w, h);

    // Store the new dimensions
    // Note: width() and height() will return the new values after this call

    // For 2D plotting, we should use orthographic projection
    m_proj.setToIdentity();
    m_proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);// Use orthographic projection for 2D plotting

    m_view.setToIdentity();
    m_view.translate(0, 0, -1);// Move slightly back for orthographic view

    // Trigger a repaint with the new dimensions
    update();
}

void OpenGLWidget::drawAxis() {

    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, m_proj.constData());
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, m_view.constData());
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, m_model.constData());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    // Get axis color from theme and set uniforms
    float r, g, b;
    hexToRGB(m_axis_color, r, g, b);
    glUniform3f(m_colorLoc, r, g, b);
    glUniform1f(m_alphaLoc, 1.0f);

    // Draw horizontal line at x=0 with 4D coordinates (x, y, 0, 1)
    std::array<GLfloat, 8> lineVertices = {
            0.0f, _view_state.y_min, 0.0f, 1.0f,
            0.0f, _view_state.y_max, 0.0f, 1.0f};

    m_vbo.bind();
    m_vbo.allocate(lineVertices.data(), lineVertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_LINES, 0, 2);
    glUseProgram(0);
}

void OpenGLWidget::addAnalogTimeSeries(
        std::string const & key,
        std::shared_ptr<AnalogTimeSeries> series,
        std::string const & color) {

    auto display_options = std::make_unique<NewAnalogTimeSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_analog_series.size()) : color;
    display_options->style.is_visible = true;

    // Calculate scale factor based on standard deviation
    auto start_time = std::chrono::high_resolution_clock::now();
    setAnalogIntrinsicProperties(series.get(), *display_options);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Standard deviation calculation took " << duration.count() << " milliseconds" << std::endl;
    display_options->scale_factor = display_options->data_cache.cached_std_dev * 5.0f;
    display_options->user_scale_factor = 1.0f;// Default user scale

    if (series->getTimeFrame()->getTotalFrameCount() / 5 > series->getNumSamples()) {
        display_options->gap_handling = AnalogGapHandling::AlwaysConnect;
        display_options->enable_gap_detection = false;

    } else {
        display_options->enable_gap_detection = true;
        display_options->gap_handling = AnalogGapHandling::DetectGaps;
        // Set gap threshold to 0.1% of total frames, with a minimum floor of 2 to work with integer time frames
        float const calculated_threshold = static_cast<float>(series->getTimeFrame()->getTotalFrameCount()) / 1000.0f;
        display_options->gap_threshold = std::max(2.0f, calculated_threshold);
    }

    _analog_series[key] = AnalogSeriesData{
            std::move(series),
            std::move(display_options)};

    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::removeAnalogTimeSeries(std::string const & key) {
    auto item = _analog_series.find(key);
    if (item != _analog_series.end()) {
        _analog_series.erase(item);
    }
    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalEventSeries(
        std::string const & key,
        std::shared_ptr<DigitalEventSeries> series,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalEventSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_event_series.size()) : color;
    display_options->style.is_visible = true;

    _digital_event_series[key] = DigitalEventSeriesData{
            std::move(series),
            std::move(display_options)};

    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalEventSeries(std::string const & key) {
    auto item = _digital_event_series.find(key);
    if (item != _digital_event_series.end()) {
        _digital_event_series.erase(item);
    }
    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalIntervalSeries(
        std::string const & key,
        std::shared_ptr<DigitalIntervalSeries> series,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalIntervalSeriesDisplayOptions>();

    // Set color
    display_options->style.hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_interval_series.size()) : color;
    display_options->style.is_visible = true;

    _digital_interval_series[key] = DigitalIntervalSeriesData{
            std::move(series),
            std::move(display_options)};

    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalIntervalSeries(std::string const & key) {
    auto item = _digital_interval_series.find(key);
    if (item != _digital_interval_series.end()) {
        _digital_interval_series.erase(item);
    }
    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::clearSeries() {
    _analog_series.clear();
    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::drawDashedLine(LineParameters const & params) {

    auto dashedProgram = ShaderManager::instance().getProgram("dashed_line");
    if (dashedProgram) glUseProgram(dashedProgram->getProgramId());

    glUniformMatrix4fv(m_dashedProjMatrixLoc, 1, GL_FALSE, (m_proj * m_view * m_model).constData());
    std::array<GLfloat, 2> hw = {static_cast<float>(width()), static_cast<float>(height())};
    glUniform2fv(m_dashedResolutionLoc, 1, hw.data());
    glUniform1f(m_dashedDashSizeLoc, params.dashLength);
    glUniform1f(m_dashedGapSizeLoc, params.gapLength);

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);

    // Pass 3D coordinates (z=0) to match the vertex shader input format
    std::array<float, 6> vertices = {
            params.xStart, params.yStart, 0.0f,
            params.xEnd, params.yEnd, 0.0f};

    m_vbo.bind();
    m_vbo.allocate(vertices.data(), vertices.size() * sizeof(float));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);

    glDrawArrays(GL_LINES, 0, 2);

    m_vbo.release();

    glUseProgram(0);
}

void OpenGLWidget::drawGridLines() {
    if (!_grid_lines_enabled) {
        return;// Grid lines are disabled
    }

    auto const start_time = _view_state.time_range.start;
    auto const end_time = _view_state.time_range.end;

    // Calculate the range of time values
    auto const time_range = end_time - start_time;

    // Avoid drawing grid lines if the range is too small or invalid
    if (time_range <= 0 || _grid_spacing <= 0) {
        return;
    }

    // Find the first grid line position that's >= start_time
    // Use integer division to ensure proper alignment
    int64_t first_grid_time = ((start_time / _grid_spacing) * _grid_spacing);
    if (first_grid_time < start_time) {
        first_grid_time += _grid_spacing;
    }

    // Draw vertical grid lines at regular intervals
    for (int64_t grid_time = first_grid_time; grid_time <= end_time; grid_time += _grid_spacing) {
        // Convert time coordinate to normalized device coordinate (-1 to 1)
        float const normalized_x = 2.0f * static_cast<float>(grid_time - start_time) / static_cast<float>(time_range) - 1.0f;

        // Skip grid lines that are outside the visible range due to floating point precision
        if (normalized_x < -1.0f || normalized_x > 1.0f) {
            continue;
        }

        LineParameters gridLine;
        gridLine.xStart = normalized_x;
        gridLine.xEnd = normalized_x;
        gridLine.yStart = _view_state.y_min;
        gridLine.yEnd = _view_state.y_max;
        gridLine.dashLength = 3.0f;// Shorter dashes for grid lines
        gridLine.gapLength = 3.0f; // Shorter gaps for grid lines

        drawDashedLine(gridLine);
    }
}

///////////////////////////////////////////////////////////////////////////////
// TimeRange / ViewState Methods
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::setXLimit(int xmax) {
    // Update the TimeRange max bound
    // TimeRange uses inclusive bounds, so max_bound = xmax - 1 for total frame count
    _view_state.time_range.max_bound = xmax > 0 ? xmax - 1 : 0;
    
    // Re-clamp current visible range to new bounds
    _view_state.time_range.setVisibleRange(
        _view_state.time_range.start,
        _view_state.time_range.end
    );
}

void OpenGLWidget::setMasterTimeFrame(std::shared_ptr<TimeFrame> master_time_frame) {
    _master_time_frame = master_time_frame;
    
    // Initialize TimeRange bounds from TimeFrame
    if (master_time_frame) {
        _view_state.time_range = CorePlotting::TimeRange::fromTimeFrame(*master_time_frame);
        
        // Set a reasonable initial visible range (not the entire TimeFrame!)
        // Default to 10,000 samples or the full range if smaller
        constexpr int64_t DEFAULT_INITIAL_RANGE = 10000;
        int64_t const total_range = _view_state.time_range.getTotalBoundedWidth();
        int64_t const initial_range = std::min(DEFAULT_INITIAL_RANGE, total_range);
        
        // Center at the beginning of the data
        int64_t const initial_center = _view_state.time_range.min_bound + initial_range / 2;
        _view_state.time_range.setCenterAndZoom(initial_center, initial_range);
    } else {
        // Reset to default empty range
        _view_state.time_range = CorePlotting::TimeRange();
    }
}

void OpenGLWidget::changeRangeWidth(int64_t range_delta) {
    int64_t const center = _view_state.time_range.getCenter();
    int64_t const current_range = _view_state.time_range.getWidth();
    int64_t const new_range = current_range + range_delta;  // Add delta to current range
    _view_state.time_range.setCenterAndZoom(center, new_range);
    updateCanvas(_time);
}

int64_t OpenGLWidget::setRangeWidth(int64_t range_width) {
    int64_t const center = _view_state.time_range.getCenter();
    int64_t const actual_range = _view_state.time_range.setCenterAndZoom(center, range_width);
    updateCanvas(_time);
    return actual_range;  // Return the actual range width achieved
}

///////////////////////////////////////////////////////////////////////////////

float OpenGLWidget::canvasXToTime(float canvas_x) const {
    // Use CorePlotting coordinate transform
    CorePlotting::TimeAxisParams params(_view_state.time_range.start, _view_state.time_range.end, width());
    return CorePlotting::canvasXToTime(canvas_x, params);
}

float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const & series_key) const {
    // Find the series
    auto const analog_it = _analog_series.find(series_key);
    if (analog_it == _analog_series.end()) {
        return 0.0f;  // Series not found
    }

    auto const & display_options = analog_it->second.display_options;

    // Step 1: Convert canvas Y to world Y using CorePlotting
    CorePlotting::YAxisParams y_params(_view_state.y_min, _view_state.y_max, height(), _view_state.vertical_pan_offset);
    float const world_y = CorePlotting::canvasYToWorldY(canvas_y, y_params);

    // Step 2: Get layout from cached response (or fallback to display_options)
    auto const * series_layout = _cached_layout_response.findLayout(series_key);
    CorePlotting::SeriesLayout layout;
    if (series_layout) {
        layout = *series_layout;
    } else {
        // Fallback to display_options layout
        layout = CorePlotting::SeriesLayout{
            series_key,
            CorePlotting::LayoutTransform{
                display_options->layout.allocated_y_center,
                display_options->layout.allocated_height * 0.5f
            },
            0
        };
    }

    // Step 3: Compose the same Y transform used for rendering
    CorePlotting::LayoutTransform y_transform = composeAnalogYTransform(
            layout,
            display_options->data_cache.cached_mean,
            display_options->data_cache.cached_std_dev,
            display_options->scaling.intrinsic_scale,
            display_options->user_scale_factor,
            display_options->scaling.user_vertical_offset,
            _view_state.global_zoom,
            _view_state.global_vertical_scale);

    // Step 4: Use inverse transform to get data value
    return y_transform.inverse(world_y);
}

// Interval selection methods
void OpenGLWidget::setSelectedInterval(std::string const & series_key, int64_t start_time, int64_t end_time) {
    _selected_intervals[series_key] = std::make_pair(start_time, end_time);
    updateCanvas(_time);
}

void OpenGLWidget::clearSelectedInterval(std::string const & series_key) {
    auto it = _selected_intervals.find(series_key);
    if (it != _selected_intervals.end()) {
        _selected_intervals.erase(it);
        updateCanvas(_time);
    }
}

std::optional<std::pair<int64_t, int64_t>> OpenGLWidget::getSelectedInterval(std::string const & series_key) const {
    auto it = _selected_intervals.find(series_key);
    if (it != _selected_intervals.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::pair<int64_t, int64_t>> OpenGLWidget::findIntervalAtTime(std::string const & series_key, float time_coord) const {
    auto it = _digital_interval_series.find(series_key);
    if (it == _digital_interval_series.end()) {
        return std::nullopt;
    }

    auto const & interval_data = it->second;
    auto const & series = interval_data.series;

    // Convert time coordinate from master time frame to series time frame
    int64_t query_time_index;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame - use time coordinate directly
        query_time_index = static_cast<int64_t>(std::round(time_coord));
    } else {
        // Different time frame - convert master time to series time frame index
        query_time_index = series->getTimeFrame()->getIndexAtTime(time_coord).getValue();
    }

    // Find all intervals that contain this time point in the series' time frame
    auto intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(query_time_index, query_time_index);

    if (!intervals.empty()) {
        // Return the first interval found, converted to master time frame coordinates
        auto const & interval = intervals.front();
        int64_t interval_start_master, interval_end_master;

        if (series->getTimeFrame().get() == _master_time_frame.get()) {
            // Same time frame - use indices directly as time coordinates
            interval_start_master = interval.start;
            interval_end_master = interval.end;
        } else {
            // Convert series indices to master time frame coordinates
            interval_start_master = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval.start)));
            interval_end_master = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval.end)));
        }

        return std::make_pair(interval_start_master, interval_end_master);
    }

    return std::nullopt;
}

// Interval edge dragging methods
std::optional<std::pair<std::string, bool>> OpenGLWidget::findIntervalEdgeAtPosition(float canvas_x, float canvas_y) const {

    // Ensure scene and layout are up-to-date for hit testing
    // Note: const_cast is safe here because we're only updating cache state
    if (_scene_dirty || _layout_response_dirty) {
        // Force a scene rebuild by requesting a paint - but for hit testing
        // during mouse events, we can use the current cached values
        // The scene will be rebuilt on next paintGL() call
    }

    // If we have no cached scene yet (e.g., before first paint), fall back to time-based check
    if (_cached_scene.rectangle_batches.empty() && _selected_intervals.empty()) {
        return std::nullopt;
    }

    // Use CorePlotting time axis utilities for coordinate conversion
    CorePlotting::TimeAxisParams const time_params(
        _view_state.time_range.start,
        _view_state.time_range.end,
        width()
    );
    
    // Convert canvas position to time (world X coordinate)
    float const world_x = CorePlotting::canvasXToTime(canvas_x, time_params);
    
    // Configure hit tester with edge tolerance in world units
    constexpr float EDGE_TOLERANCE_PX = 10.0f;
    float const time_per_pixel = static_cast<float>(time_params.getTimeSpan()) / 
                                  static_cast<float>(time_params.viewport_width_px);
    float const edge_tolerance = EDGE_TOLERANCE_PX * time_per_pixel;
    
    CorePlotting::HitTestConfig config;
    config.edge_tolerance = edge_tolerance;
    config.point_tolerance = edge_tolerance;
    
    CorePlotting::SceneHitTester tester(config);
    
    // Convert selected intervals to the format expected by SceneHitTester
    std::map<std::string, std::pair<int64_t, int64_t>> selected_intervals_map;
    for (auto const & [key, bounds] : _selected_intervals) {
        selected_intervals_map[key] = bounds;
    }
    
    // Use SceneHitTester to find interval edges
    CorePlotting::HitTestResult result = tester.findIntervalEdge(
        world_x,
        _cached_scene,
        selected_intervals_map,
        _rectangle_batch_key_map
    );
    
    if (result.isIntervalEdge()) {
        bool is_left_edge = (result.hit_type == CorePlotting::HitType::IntervalEdgeLeft);
        return std::make_pair(result.series_key, is_left_edge);
    }

    // Fall back to checking selected intervals directly if scene doesn't have the data
    // This handles the case where intervals are selected but not yet rendered in scene
    if (!_cached_scene.rectangle_batches.empty()) {
        return std::nullopt;
    }
    
    // Legacy fallback for when scene isn't available yet
    for (auto const & [series_key, interval_bounds]: _selected_intervals) {
        auto const [start_time, end_time] = interval_bounds;
        
        auto const start_time_f = static_cast<float>(start_time);
        auto const end_time_f = static_cast<float>(end_time);

        // Quick bounds check with tolerance (in time units)
        if (world_x < start_time_f - edge_tolerance || 
            world_x > end_time_f + edge_tolerance) {
            continue;
        }

        // Check if we're close to the left edge
        if (std::abs(world_x - start_time_f) <= edge_tolerance) {
            return std::make_pair(series_key, true);// true = left edge
        }

        // Check if we're close to the right edge
        if (std::abs(world_x - end_time_f) <= edge_tolerance) {
            return std::make_pair(series_key, false);// false = right edge
        }
    }

    static_cast<void>(canvas_y);  // Y not used for edge detection
    return std::nullopt;
}

void OpenGLWidget::startIntervalDrag(std::string const & series_key, bool is_left_edge, QPoint const & start_pos) {
    auto selected_interval = getSelectedInterval(series_key);
    if (!selected_interval.has_value()) {
        return;
    }

    auto const [start_time, end_time] = selected_interval.value();

    // Create HitTestResult for the IntervalDragController
    auto hit_result = CorePlotting::HitTestResult::intervalEdgeHit(
        series_key,
        EntityId{0},  // EntityId not used for edge drag, could be enhanced later
        is_left_edge,
        start_time,
        end_time,
        is_left_edge ? static_cast<float>(start_time) : static_cast<float>(end_time),
        0.0f  // distance not relevant here
    );

    // Configure the drag controller with time frame bounds if available
    CorePlotting::IntervalDragConfig config;
    config.min_width = 1;
    config.snap_to_integer = true;
    config.allow_edge_swap = false;
    
    // Set time bounds from master time frame if available
    if (_master_time_frame && _master_time_frame->getTotalFrameCount() > 0) {
        config.min_time = static_cast<int64_t>(_master_time_frame->getTimeAtIndex(TimeFrameIndex(0)));
        config.max_time = static_cast<int64_t>(_master_time_frame->getTimeAtIndex(
            TimeFrameIndex(static_cast<int64_t>(_master_time_frame->getTotalFrameCount() - 1))));
    }
    
    _interval_drag_controller.setConfig(config);

    // Start the drag
    if (!_interval_drag_controller.startDrag(hit_result)) {
        return;
    }

    // Disable normal mouse interactions during drag
    setCursor(Qt::SizeHorCursor);

    std::cout << "Started dragging " << (is_left_edge ? "left" : "right")
              << " edge of interval [" << start_time << ", " << end_time << "]" << std::endl;
    
    static_cast<void>(start_pos);  // start_pos no longer needed with controller
}

void OpenGLWidget::updateIntervalDrag(QPoint const & current_pos) {
    if (!_interval_drag_controller.isActive()) {
        return;
    }

    auto const & state = _interval_drag_controller.getState();
    std::string const & series_key = state.series_key;

    // Convert mouse position to time coordinate (in master time frame)
    float const current_time_master = canvasXToTime(static_cast<float>(current_pos.x()));

    // Get the series data
    auto it = _digital_interval_series.find(series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - abort drag
        cancelIntervalDrag();
        return;
    }

    auto const & series = it->second.series;

    // Convert master time coordinate to series time frame index for collision detection
    int64_t current_time_series_index;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame - use time coordinate directly
        current_time_series_index = static_cast<int64_t>(std::round(current_time_master));
    } else {
        // Different time frame - convert master time to series time frame index
        current_time_series_index = series->getTimeFrame()->getIndexAtTime(current_time_master).getValue();
    }

    // Convert original interval bounds to series time frame for collision detection
    int64_t original_start_series, original_end_series;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame
        original_start_series = state.original_start;
        original_end_series = state.original_end;
    } else {
        // Convert master time coordinates to series time frame indices
        original_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(state.original_start)).getValue();
        original_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(state.original_end)).getValue();
    }

    // Perform collision detection in series time frame
    int64_t constrained_time = current_time_series_index;
    bool const is_dragging_left = (state.edge == CorePlotting::DraggedEdge::Left);

    if (is_dragging_left) {
        // Dragging left edge - check for collision with other intervals
        auto overlapping_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                constrained_time, constrained_time);

        for (auto const & interval: overlapping_intervals) {
            // Skip the interval we're currently editing
            if (interval.start == original_start_series && interval.end == original_end_series) {
                continue;
            }

            // If we would overlap with another interval, stop 1 index after it
            if (constrained_time <= interval.end) {
                constrained_time = interval.end + 1;
                break;
            }
        }
    } else {
        // Dragging right edge - check for collision with other intervals
        auto overlapping_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                constrained_time, constrained_time);

        for (auto const & interval: overlapping_intervals) {
            // Skip the interval we're currently editing
            if (interval.start == original_start_series && interval.end == original_end_series) {
                continue;
            }

            // If we would overlap with another interval, stop 1 index before it
            if (constrained_time >= interval.start) {
                constrained_time = interval.start - 1;
                break;
            }
        }
    }

    // Convert constrained time back to master time frame for the controller
    float world_x_for_controller;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        world_x_for_controller = static_cast<float>(constrained_time);
    } else {
        try {
            world_x_for_controller = series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(constrained_time));
        } catch (...) {
            // Conversion failed - abort drag
            cancelIntervalDrag();
            return;
        }
    }

    // Update the controller with collision-constrained position
    _interval_drag_controller.updateDrag(world_x_for_controller);

    // Trigger redraw to show the dragged interval
    updateCanvas(_time);
}

void OpenGLWidget::finishIntervalDrag() {
    if (!_interval_drag_controller.isActive()) {
        return;
    }

    // Get final state from controller
    auto final_state = _interval_drag_controller.finishDrag();
    std::string const & series_key = final_state.series_key;

    // Get the series data
    auto it = _digital_interval_series.find(series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - just reset cursor
        setCursor(Qt::ArrowCursor);
        updateCanvas(_time);
        return;
    }

    auto const & series = it->second.series;

    // Only apply changes if the interval was actually modified
    if (!final_state.hasChanged()) {
        setCursor(Qt::ArrowCursor);
        updateCanvas(_time);
        return;
    }

    try {
        // Convert all coordinates to series time frame for data operations
        int64_t original_start_series, original_end_series, new_start_series, new_end_series;

        if (series->getTimeFrame().get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            original_start_series = final_state.original_start;
            original_end_series = final_state.original_end;
            new_start_series = final_state.current_start;
            new_end_series = final_state.current_end;
        } else {
            // Convert master time coordinates to series time frame indices
            original_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(final_state.original_start)).getValue();
            original_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(final_state.original_end)).getValue();
            new_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(final_state.current_start)).getValue();
            new_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(final_state.current_end)).getValue();
        }

        // Validate converted coordinates
        if (new_start_series >= new_end_series ||
            new_start_series < 0 || new_end_series < 0) {
            throw std::runtime_error("Invalid interval bounds after conversion");
        }

        // Update the interval data in the series' native time frame
        // First, remove the original interval completely
        for (int64_t time = original_start_series; time <= original_end_series; ++time) {
            series->setEventAtTime(TimeFrameIndex(time), false);
        }

        // Add the new interval
        series->addEvent(TimeFrameIndex(new_start_series), TimeFrameIndex(new_end_series));

        // Update the selection to the new interval (stored in master time frame coordinates)
        setSelectedInterval(series_key, final_state.current_start, final_state.current_end);

        std::cout << "Finished dragging interval. Original: ["
                  << final_state.original_start << ", " << final_state.original_end
                  << "] -> New: [" << final_state.current_start << ", " << final_state.current_end << "]" << std::endl;

    } catch (...) {
        // Error occurred during conversion or data update
        std::cout << "Error during interval drag completion - keeping original interval" << std::endl;
    }

    // Reset cursor and redraw
    setCursor(Qt::ArrowCursor);
    updateCanvas(_time);
}

void OpenGLWidget::cancelIntervalDrag() {
    if (!_interval_drag_controller.isActive()) {
        return;
    }

    std::cout << "Cancelled interval drag" << std::endl;

    // Cancel the drag in the controller
    _interval_drag_controller.cancelDrag();
    
    // Reset cursor
    setCursor(Qt::ArrowCursor);

    // Trigger redraw to remove the dragged interval visualization
    updateCanvas(_time);
}

void OpenGLWidget::drawDraggedInterval() {
    if (!_interval_drag_controller.isActive()) {
        return;
    }

    auto const & drag_state = _interval_drag_controller.getState();

    // Get the series data for rendering
    auto it = _digital_interval_series.find(drag_state.series_key);
    if (it == _digital_interval_series.end()) {
        return;
    }

    auto const & display_options = it->second.display_options;

    auto const start_time = static_cast<float>(_view_state.time_range.start);
    auto const end_time = static_cast<float>(_view_state.time_range.end);
    auto const min_y = _view_state.y_min;
    auto const max_y = _view_state.y_max;

    // Check if the dragged interval is visible
    if (drag_state.current_end < static_cast<int64_t>(start_time) || drag_state.current_start > static_cast<int64_t>(end_time)) {
        return;
    }

    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    // Set up matrices (same as normal interval rendering)
    auto Model = glm::mat4(1.0f);
    auto View = glm::mat4(1.0f);
    auto Projection = glm::ortho(start_time, end_time, min_y, max_y);

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

    // Get colors
    int r, g, b;
    hexToRGB(display_options->style.hex_color, r, g, b);
    float const rNorm = static_cast<float>(r) / 255.0f;
    float const gNorm = static_cast<float>(g) / 255.0f;
    float const bNorm = static_cast<float>(b) / 255.0f;

    // Clip the dragged interval to visible range
    float const dragged_start = std::max(static_cast<float>(drag_state.current_start), start_time);
    float const dragged_end = std::min(static_cast<float>(drag_state.current_end), end_time);

    // Draw the original interval dimmed (alpha = 0.2)
    float const original_start = std::max(static_cast<float>(drag_state.original_start), start_time);
    float const original_end = std::min(static_cast<float>(drag_state.original_end), end_time);

    // Set color and alpha uniforms for original interval (dimmed)
    glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
    glUniform1f(m_alphaLoc, 0.2f);

    // Create 4D vertices (x, y, 0, 1) to match the shader expectations
    std::array<GLfloat, 16> original_vertices = {
            original_start, min_y, 0.0f, 1.0f,
            original_end, min_y, 0.0f, 1.0f,
            original_end, max_y, 0.0f, 1.0f,
            original_start, max_y, 0.0f, 1.0f};

    m_vbo.bind();
    m_vbo.allocate(original_vertices.data(), original_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Set color and alpha uniforms for dragged interval (semi-transparent)
    glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
    glUniform1f(m_alphaLoc, 0.8f);

    // Create 4D vertices (x, y, 0, 1) to match the shader expectations
    std::array<GLfloat, 16> dragged_vertices = {
            dragged_start, min_y, 0.0f, 1.0f,
            dragged_end, min_y, 0.0f, 1.0f,
            dragged_end, max_y, 0.0f, 1.0f,
            dragged_start, max_y, 0.0f, 1.0f};

    m_vbo.bind();
    m_vbo.allocate(dragged_vertices.data(), dragged_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glUseProgram(0);
}

void OpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        // Check if we're double-clicking over a digital interval series
        //float const canvas_x = static_cast<float>(event->pos().x());
        //float const canvas_y = static_cast<float>(event->pos().y());

        // Find which digital interval series (if any) is at this Y position
        // For now, use the first visible digital interval series
        // TODO: Improve this to detect which series based on Y coordinate
        for (auto const & [series_key, data]: _digital_interval_series) {
            if (data.display_options->style.is_visible) {
                startNewIntervalCreation(series_key, event->pos());
                return;
            }
        }
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OpenGLWidget::startNewIntervalCreation(std::string const & series_key, QPoint const & start_pos) {
    // Don't start if we're already dragging an interval
    if (_interval_drag_controller.isActive() || _is_creating_new_interval) {
        return;
    }

    // Check if the series exists
    auto it = _digital_interval_series.find(series_key);
    if (it == _digital_interval_series.end()) {
        return;
    }

    _is_creating_new_interval = true;
    _new_interval_series_key = series_key;
    _new_interval_click_pos = start_pos;

    // Convert click position to time coordinate (in master time frame)
    float const click_time_master = canvasXToTime(static_cast<float>(start_pos.x()));
    _new_interval_click_time = static_cast<int64_t>(std::round(click_time_master));

    // Initialize start and end to the click position
    _new_interval_start_time = _new_interval_click_time;
    _new_interval_end_time = _new_interval_click_time;

    // Set cursor to indicate creation mode
    setCursor(Qt::SizeHorCursor);

    std::cout << "Started new interval creation for series " << series_key
              << " at time " << _new_interval_click_time << std::endl;
}

void OpenGLWidget::updateNewIntervalCreation(QPoint const & current_pos) {
    if (!_is_creating_new_interval) {
        return;
    }

    // Convert current mouse position to time coordinate (in master time frame)
    float const current_time_master = canvasXToTime(static_cast<float>(current_pos.x()));
    int64_t const current_time_coord = static_cast<int64_t>(std::round(current_time_master));

    // Get the series data for constraint checking
    auto it = _digital_interval_series.find(_new_interval_series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - abort creation
        cancelNewIntervalCreation();
        return;
    }

    auto const & series = it->second.series;

    // Convert coordinates to series time frame for collision detection
    int64_t click_time_series, current_time_series;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame - use coordinates directly
        click_time_series = _new_interval_click_time;
        current_time_series = current_time_coord;
    } else {
        // Convert master time coordinates to series time frame indices
        click_time_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_new_interval_click_time)).getValue();
        current_time_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(current_time_coord)).getValue();
    }

    // Determine interval bounds (always ensure start < end)
    int64_t new_start_series = std::min(click_time_series, current_time_series);
    int64_t new_end_series = std::max(click_time_series, current_time_series);

    // Ensure minimum interval size of 1
    if (new_start_series == new_end_series) {
        if (current_time_series >= click_time_series) {
            new_end_series = new_start_series + 1;
        } else {
            new_start_series = new_end_series - 1;
        }
    }

    // Check for collision with existing intervals in series time frame
    auto overlapping_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
            new_start_series, new_end_series);

    // If there are overlapping intervals, constrain the new interval
    for (auto const & interval: overlapping_intervals) {
        if (current_time_series >= click_time_series) {
            // Dragging right - stop before the first overlapping interval
            if (new_end_series >= interval.start) {
                new_end_series = interval.start - 1;
                if (new_end_series <= new_start_series) {
                    new_end_series = new_start_series + 1;
                }
                break;
            }
        } else {
            // Dragging left - stop after the last overlapping interval
            if (new_start_series <= interval.end) {
                new_start_series = interval.end + 1;
                if (new_start_series >= new_end_series) {
                    new_start_series = new_end_series - 1;
                }
                break;
            }
        }
    }

    // Convert back to master time frame for display
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame
        _new_interval_start_time = new_start_series;
        _new_interval_end_time = new_end_series;
    } else {
        // Convert series indices back to master time coordinates
        try {
            _new_interval_start_time = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(new_start_series)));
            _new_interval_end_time = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(new_end_series)));
        } catch (...) {
            // Conversion failed - abort creation
            cancelNewIntervalCreation();
            return;
        }
    }

    // Trigger redraw to show the new interval being created
    updateCanvas(_time);
}

void OpenGLWidget::finishNewIntervalCreation() {
    if (!_is_creating_new_interval) {
        return;
    }

    // Get the series data
    auto it = _digital_interval_series.find(_new_interval_series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - abort creation
        cancelNewIntervalCreation();
        return;
    }

    auto const & series = it->second.series;

    try {
        // Convert coordinates to series time frame for data operations
        int64_t new_start_series, new_end_series;

        if (series->getTimeFrame().get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            new_start_series = _new_interval_start_time;
            new_end_series = _new_interval_end_time;
        } else {
            // Convert master time coordinates to series time frame indices
            new_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_new_interval_start_time)).getValue();
            new_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_new_interval_end_time)).getValue();
        }

        // Validate converted coordinates
        if (new_start_series >= new_end_series || new_start_series < 0 || new_end_series < 0) {
            throw std::runtime_error("Invalid interval bounds after conversion");
        }

        // Add the new interval to the series
        series->addEvent(TimeFrameIndex(new_start_series), TimeFrameIndex(new_end_series));

        // Set the new interval as selected (stored in master time frame coordinates)
        setSelectedInterval(_new_interval_series_key, _new_interval_start_time, _new_interval_end_time);

        std::cout << "Created new interval [" << _new_interval_start_time
                  << ", " << _new_interval_end_time << "] for series "
                  << _new_interval_series_key << std::endl;

    } catch (...) {
        // Error occurred during conversion or data update - abort creation
        std::cout << "Error during new interval creation" << std::endl;
        cancelNewIntervalCreation();
        return;
    }

    // Reset creation state
    _is_creating_new_interval = false;
    _new_interval_series_key.clear();
    setCursor(Qt::ArrowCursor);

    // Trigger final redraw
    updateCanvas(_time);
}

void OpenGLWidget::cancelNewIntervalCreation() {
    if (!_is_creating_new_interval) {
        return;
    }

    std::cout << "Cancelled new interval creation" << std::endl;

    // Reset creation state without applying changes
    _is_creating_new_interval = false;
    _new_interval_series_key.clear();
    setCursor(Qt::ArrowCursor);

    // Trigger redraw to remove the new interval visualization
    updateCanvas(_time);
}

void OpenGLWidget::drawNewIntervalBeingCreated() {
    if (!_is_creating_new_interval) {
        return;
    }

    // Get the series data for rendering
    auto it = _digital_interval_series.find(_new_interval_series_key);
    if (it == _digital_interval_series.end()) {
        return;
    }

    auto const & display_options = it->second.display_options;

    auto const start_time = static_cast<float>(_view_state.time_range.start);
    auto const end_time = static_cast<float>(_view_state.time_range.end);
    auto const min_y = _view_state.y_min;
    auto const max_y = _view_state.y_max;

    // Check if the new interval is visible
    if (_new_interval_end_time < static_cast<int64_t>(start_time) || _new_interval_start_time > static_cast<int64_t>(end_time)) {
        return;
    }

    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    // Set up matrices (same as normal interval rendering)
    auto Model = glm::mat4(1.0f);
    auto View = glm::mat4(1.0f);
    auto Projection = glm::ortho(start_time, end_time, min_y, max_y);

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

    // Get colors
    int r, g, b;
    hexToRGB(display_options->style.hex_color, r, g, b);
    float const rNorm = static_cast<float>(r) / 255.0f;
    float const gNorm = static_cast<float>(g) / 255.0f;
    float const bNorm = static_cast<float>(b) / 255.0f;

    // Set color and alpha uniforms for new interval (50% transparency)
    glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
    glUniform1f(m_alphaLoc, 0.5f);

    // Clip the new interval to visible range
    float const new_start = std::max(static_cast<float>(_new_interval_start_time), start_time);
    float const new_end = std::min(static_cast<float>(_new_interval_end_time), end_time);

    // Draw the new interval being created with 50% transparency
    // Create 4D vertices (x, y, 0, 1) to match the shader expectations
    std::array<GLfloat, 16> new_interval_vertices = {
            new_start, min_y, 0.0f, 1.0f,
            new_end, min_y, 0.0f, 1.0f,
            new_end, max_y, 0.0f, 1.0f,
            new_start, max_y, 0.0f, 1.0f};

    m_vbo.bind();
    m_vbo.allocate(new_interval_vertices.data(), new_interval_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////
// Tooltip functionality
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::startTooltipTimer(QPoint const & pos) {
    // Restart the timer with new position
    _tooltip_hover_pos = pos;
    _tooltip_timer->start();
}

void OpenGLWidget::cancelTooltipTimer() {
    _tooltip_timer->stop();
    QToolTip::hideText();
}

std::optional<std::pair<std::string, std::string>> OpenGLWidget::findSeriesAtPosition(float canvas_x, float canvas_y) const {
    // Use CorePlotting SceneHitTester for comprehensive hit testing
    // This queries both spatial index (for discrete elements) and layout (for series regions)
    
    // Compute layout if dirty (const_cast needed for lazy evaluation pattern)
    // Note: This is safe because computeAndApplyLayout only modifies cache state
    if (_layout_response_dirty) {
        const_cast<OpenGLWidget*>(this)->computeAndApplyLayout();
    }
    
    if (_cached_layout_response.layouts.empty()) {
        return std::nullopt;
    }

    // Convert canvas Y to world Y coordinate
    // In OpenGL, Y is inverted: top of window is -1, bottom is +1 in our view
    float const ndc_y = -1.0f + 2.0f * (static_cast<float>(height()) - canvas_y) / static_cast<float>(height());

    // Apply vertical pan offset to get the actual Y position in the coordinate system
    float const world_y = ndc_y - _view_state.vertical_pan_offset;
    float const world_x = canvasXToTime(canvas_x);
    
    // First try full hit test if we have a cached scene with spatial index
    // This can identify specific discrete elements (events, points)
    if (_cached_scene.spatial_index) {
        CorePlotting::HitTestResult result = _hit_tester.hitTest(
            world_x, world_y, _cached_scene, _cached_layout_response);
        
        if (result.hasHit()) {
            std::string series_type;
            std::string series_key = result.series_key;
            
            // For discrete hits, look up series key from batch maps if needed
            if (series_key.empty() && result.hit_type == CorePlotting::HitType::DigitalEvent) {
                // Try to find series key from glyph batch maps
                // The QuadTree stores EntityId but not series key, so region query is fallback
                auto region_result = _hit_tester.querySeriesRegion(world_x, world_y, _cached_layout_response);
                if (region_result.hasHit()) {
                    series_key = region_result.series_key;
                }
            }
            
            // Determine series type
            if (_analog_series.find(series_key) != _analog_series.end()) {
                series_type = "Analog";
            } else if (_digital_event_series.find(series_key) != _digital_event_series.end()) {
                series_type = "Event";
            } else if (_digital_interval_series.find(series_key) != _digital_interval_series.end()) {
                series_type = "Interval";
            } else {
                series_type = "Unknown";
            }
            
            return std::make_pair(series_type, series_key);
        }
    }
    
    // Fall back to series region query (always works, uses layout)
    CorePlotting::HitTestResult result = _hit_tester.querySeriesRegion(
        world_x, world_y, _cached_layout_response);
    
    if (result.hasHit()) {
        // Determine series type from the key by checking our series maps
        std::string series_type;
        if (_analog_series.find(result.series_key) != _analog_series.end()) {
            series_type = "Analog";
        } else if (_digital_event_series.find(result.series_key) != _digital_event_series.end()) {
            series_type = "Event";
        } else if (_digital_interval_series.find(result.series_key) != _digital_interval_series.end()) {
            series_type = "Interval";
        } else {
            series_type = "Unknown";
        }
        
        return std::make_pair(series_type, result.series_key);
    }

    return std::nullopt;
}

void OpenGLWidget::showSeriesInfoTooltip(QPoint const & pos) {
    float const canvas_x = static_cast<float>(pos.x());
    float const canvas_y = static_cast<float>(pos.y());

    // Find which series is under the cursor
    auto series_info = findSeriesAtPosition(canvas_x, canvas_y);

    if (series_info.has_value()) {
        auto const & [series_type, series_key] = series_info.value();

        // Build tooltip text
        QString tooltip_text;
        if (series_type == "Analog") {
            // Get the analog value at this Y coordinate
            float const analog_value = canvasYToAnalogValue(canvas_y, series_key);
            tooltip_text = QString("<b>Analog Series</b><br>Key: %1<br>Value: %2")
                                   .arg(QString::fromStdString(series_key))
                                   .arg(analog_value, 0, 'f', 3);
        } else if (series_type == "Event") {
            tooltip_text = QString("<b>Event Series</b><br>Key: %1")
                                   .arg(QString::fromStdString(series_key));
        }

        // Show tooltip at cursor position
        QToolTip::showText(mapToGlobal(pos), tooltip_text, this);
    } else {
        // No series under cursor, hide tooltip
        QToolTip::hideText();
    }
}
///////////////////////////////////////////////////////////////////////////////
// New SceneRenderer-based rendering methods
///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::renderWithSceneRenderer() {
    if (!_scene_renderer || !_master_time_frame) {
        return;
    }

    // Compute layout if dirty - this updates _cached_layout_response
    // and all display_options->layout fields
    computeAndApplyLayout();

    auto const start_time = TimeFrameIndex(_view_state.time_range.start);
    auto const end_time = TimeFrameIndex(_view_state.time_range.end);

    // Build shared View and Projection matrices
    CorePlotting::ViewProjectionParams view_params;
    view_params.vertical_pan_offset = _view_state.vertical_pan_offset;

    // Shared projection matrix (time range to NDC)
    glm::mat4 projection = CorePlotting::getAnalogProjectionMatrix(start_time, end_time, _view_state.y_min, _view_state.y_max);

    // Shared view matrix (global pan)
    glm::mat4 view = CorePlotting::getAnalogViewMatrix(view_params);

    // Create SceneBuilder and set bounds for spatial indexing
    // Bounds are in world coordinates: X = [start_time, end_time], Y = [y_min, y_max]
    BoundingBox scene_bounds(
        static_cast<float>(start_time.getValue()),  // min_x
        _view_state.y_min,                          // min_y
        static_cast<float>(end_time.getValue()),    // max_x
        _view_state.y_max                           // max_y
    );
    
    CorePlotting::SceneBuilder builder;
    builder.setBounds(scene_bounds);
    builder.setMatrices(view, projection);

    // Add all series batches to the scene builder
    addAnalogBatchesToBuilder(builder);
    addEventBatchesToBuilder(builder);
    addIntervalBatchesToBuilder(builder);

    // Build the scene (this also builds spatial index for discrete elements)
    _cached_scene = builder.build();
    _scene_dirty = false;
    
    // Store batch key maps for hit testing
    _rectangle_batch_key_map = builder.getRectangleBatchKeyMap();
    _glyph_batch_key_map = builder.getGlyphBatchKeyMap();

    // Upload scene to renderer and render
    _scene_renderer->uploadScene(_cached_scene);
    _scene_renderer->render(view, projection);
}

void OpenGLWidget::addAnalogBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const start_time = TimeFrameIndex(_view_state.time_range.start);
    auto const end_time = TimeFrameIndex(_view_state.time_range.end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cached_layout_response

    for (auto const & [key, analog_data]: _analog_series) {
        auto const & series = analog_data.series;
        auto const & display_options = analog_data.display_options;

        if (!display_options->style.is_visible) continue;

        // Look up layout from cached response
        auto const * series_layout = _cached_layout_response.findLayout(key);
        if (!series_layout) {
            // Fallback to display_options layout if not found (shouldn't happen)
            CorePlotting::SeriesLayout fallback_layout{
                key,
                CorePlotting::LayoutTransform{
                    display_options->layout.allocated_y_center,
                    display_options->layout.allocated_height * 0.5f
                },
                0
            };
            series_layout = &fallback_layout;
        }

        // Compose the full Y transform using the new simplified API
        CorePlotting::LayoutTransform y_transform = composeAnalogYTransform(
                *series_layout,
                display_options->data_cache.cached_mean,
                display_options->data_cache.cached_std_dev,
                display_options->scaling.intrinsic_scale,
                display_options->user_scale_factor,
                display_options->scaling.user_vertical_offset,
                _view_state.global_zoom,
                _view_state.global_vertical_scale);
        
        // Create model matrix directly from the composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color
        int r, g, b;
        hexToRGB(display_options->style.hex_color, r, g, b);

        // Build batch parameters
        DataViewerHelpers::AnalogBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.gap_threshold = display_options->gap_threshold;
        batch_params.detect_gaps = (display_options->gap_handling == AnalogGapHandling::DetectGaps);
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                1.0f);
        batch_params.thickness = static_cast<float>(display_options->style.line_thickness);
        
        // Choose render mode based on gap handling setting
        if (display_options->gap_handling == AnalogGapHandling::ShowMarkers) {
            batch_params.render_mode = DataViewerHelpers::AnalogRenderMode::Markers;
        } else {
            batch_params.render_mode = DataViewerHelpers::AnalogRenderMode::Line;
        }

        // Build and add batch to builder using simplified API (no intermediate param structs)
        if (batch_params.render_mode == DataViewerHelpers::AnalogRenderMode::Markers) {
            auto batch = DataViewerHelpers::buildAnalogSeriesMarkerBatchSimplified(
                    *series, _master_time_frame, batch_params, model_matrix);
            if (!batch.positions.empty()) {
                builder.addGlyphBatch(std::move(batch));
            }
        } else {
            auto batch = DataViewerHelpers::buildAnalogSeriesBatchSimplified(
                    *series, _master_time_frame, batch_params, model_matrix);
            if (!batch.vertices.empty()) {
                builder.addPolyLineBatch(std::move(batch));
            }
        }
    }
}

void OpenGLWidget::addEventBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const start_time = TimeFrameIndex(_view_state.time_range.start);
    auto const end_time = TimeFrameIndex(_view_state.time_range.end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cached_layout_response

    for (auto const & [key, event_data]: _digital_event_series) {
        auto const & series = event_data.series;
        auto const & display_options = event_data.display_options;

        if (!display_options->style.is_visible) continue;

        // Determine plotting mode based on display mode
        display_options->plotting_mode = (display_options->display_mode == EventDisplayMode::Stacked)
                                                 ? EventPlottingMode::Stacked
                                                 : EventPlottingMode::FullCanvas;

        // Compose the Y transform based on plotting mode
        CorePlotting::LayoutTransform y_transform;
        if (display_options->plotting_mode == EventPlottingMode::FullCanvas) {
            // Full canvas mode - events extend full viewport height
            y_transform = composeEventFullCanvasYTransform(
                    _view_state.y_min, _view_state.y_max, display_options->margin_factor);
        } else {
            // Stacked mode - look up layout from cached response
            auto const * series_layout = _cached_layout_response.findLayout(key);
            if (series_layout) {
                y_transform = composeEventYTransform(
                        *series_layout, display_options->margin_factor, _view_state.global_vertical_scale);
            } else {
                // Fallback if layout not found
                CorePlotting::SeriesLayout fallback{
                    key,
                    CorePlotting::LayoutTransform{
                        display_options->layout.allocated_y_center,
                        display_options->layout.allocated_height * 0.5f
                    },
                    0
                };
                y_transform = composeEventYTransform(
                        fallback, display_options->margin_factor, _view_state.global_vertical_scale);
            }
        }
        
        // Create model matrix from composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color
        int r, g, b;
        hexToRGB(display_options->style.hex_color, r, g, b);

        // Build batch parameters
        DataViewerHelpers::EventBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                display_options->style.alpha);
        batch_params.glyph_size = static_cast<float>(display_options->style.line_thickness);
        batch_params.glyph_type = CorePlotting::RenderableGlyphBatch::GlyphType::Tick;

        // Build and add batch using simplified API
        auto batch = DataViewerHelpers::buildEventSeriesBatchSimplified(
                *series, _master_time_frame, batch_params, model_matrix);

        if (!batch.positions.empty()) {
            builder.addGlyphBatch(std::move(batch));
        }
    }
}

void OpenGLWidget::addIntervalBatchesToBuilder(CorePlotting::SceneBuilder & builder) {
    if (!_master_time_frame) {
        return;
    }

    auto const start_time = TimeFrameIndex(_view_state.time_range.start);
    auto const end_time = TimeFrameIndex(_view_state.time_range.end);

    // Layout has already been computed by computeAndApplyLayout() in renderWithSceneRenderer()
    // Each series' layout is available in _cached_layout_response

    for (auto const & [key, interval_data]: _digital_interval_series) {
        auto const & series = interval_data.series;
        auto const & display_options = interval_data.display_options;

        if (!display_options->style.is_visible) continue;

        // Look up layout from cached response
        auto const * series_layout = _cached_layout_response.findLayout(key);
        CorePlotting::LayoutTransform y_transform;
        if (series_layout) {
            y_transform = composeIntervalYTransform(
                    *series_layout, display_options->margin_factor,
                    _view_state.global_zoom, _view_state.global_vertical_scale);
        } else {
            // Fallback if layout not found
            CorePlotting::SeriesLayout fallback{
                key,
                CorePlotting::LayoutTransform{
                    display_options->layout.allocated_y_center,
                    display_options->layout.allocated_height * 0.5f
                },
                0
            };
            y_transform = composeIntervalYTransform(
                    fallback, display_options->margin_factor,
                    _view_state.global_zoom, _view_state.global_vertical_scale);
        }
        
        // Create model matrix from composed transform
        glm::mat4 model_matrix = CorePlotting::createModelMatrix(y_transform);

        // Parse color
        int r, g, b;
        hexToRGB(display_options->style.hex_color, r, g, b);

        // Build batch parameters
        DataViewerHelpers::IntervalBatchParams batch_params;
        batch_params.start_time = start_time;
        batch_params.end_time = end_time;
        batch_params.color = glm::vec4(
                static_cast<float>(r) / 255.0f,
                static_cast<float>(g) / 255.0f,
                static_cast<float>(b) / 255.0f,
                display_options->style.alpha);

        // Build and add batch using simplified API
        auto batch = DataViewerHelpers::buildIntervalSeriesBatchSimplified(
                *series, _master_time_frame, batch_params, model_matrix);

        if (!batch.bounds.empty()) {
            builder.addRectangleBatch(std::move(batch));
        }
        
        // Draw selection highlight if this series has a selected interval
        // (and we're not currently dragging it)
        auto selected_interval = getSelectedInterval(key);
        auto const & drag_state = _interval_drag_controller.getState();
        if (selected_interval.has_value() && !(_interval_drag_controller.isActive() && drag_state.series_key == key)) {
            auto const [sel_start_time, sel_end_time] = selected_interval.value();
            
            // Check if the selected interval overlaps with visible range
            if (sel_end_time >= _view_state.time_range.start && sel_start_time <= _view_state.time_range.end) {
                // Clip to visible range
                int64_t const highlighted_start = std::max(sel_start_time, _view_state.time_range.start);
                int64_t const highlighted_end = std::min(sel_end_time, _view_state.time_range.end);
                
                // Create a brighter version of the color for highlighting
                glm::vec4 highlight_color(
                        std::min(1.0f, static_cast<float>(r) / 255.0f + 0.3f),
                        std::min(1.0f, static_cast<float>(g) / 255.0f + 0.3f),
                        std::min(1.0f, static_cast<float>(b) / 255.0f + 0.3f),
                        1.0f);
                
                // Build and add highlight border to builder (use same model matrix)
                auto highlight_batch = DataViewerHelpers::buildIntervalHighlightBorderBatch(
                        highlighted_start, highlighted_end,
                        highlight_color, 4.0f, model_matrix);
                
                if (!highlight_batch.vertices.empty()) {
                    builder.addPolyLineBatch(std::move(highlight_batch));
                }
            }
        }
    }
}

// =============================================================================
// Layout System (Phase 4.9 Migration - Unified LayoutEngine)
// =============================================================================

bool OpenGLWidget::extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id) {
    channel_id = -1;
    group.clear();
    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        return false;
    }
    group = key.substr(0, pos);
    try {
        int const parsed = std::stoi(key.substr(pos + 1));
        channel_id = parsed > 0 ? parsed - 1 : parsed;
    } catch (...) {
        channel_id = -1;
        return false;
    }
    return true;
}

std::vector<std::string> OpenGLWidget::orderAnalogKeysByConfig(std::vector<std::string> const & visible_keys) const {
    // Group visible analog series by group_name
    struct Item { std::string key; std::string group; int channel; };
    std::vector<Item> items;
    items.reserve(visible_keys.size());
    for (auto const & key : visible_keys) {
        std::string group_name;
        int channel_id;
        extractGroupAndChannel(key, group_name, channel_id);
        Item it{key, group_name, channel_id};
        items.push_back(std::move(it));
    }

    // Sort with configuration: by group; within group, if config present, by ascending y; else by channel id
    std::stable_sort(items.begin(), items.end(), [&](Item const & a, Item const & b) {
        if (a.group != b.group) return a.group < b.group;
        auto cfg_it = _spike_sorter_configs.find(a.group);
        if (cfg_it == _spike_sorter_configs.end()) {
            return a.channel < b.channel;
        }
        auto const & cfg = cfg_it->second;
        auto find_y = [&](int ch) {
            for (auto const & p : cfg) if (p.channel_id == ch) return p.y;
            return 0.0f;
        };
        float ya = find_y(a.channel);
        float yb = find_y(b.channel);
        if (ya == yb) return a.channel < b.channel;
        return ya < yb; // ascending by y so larger y get larger index (top)
    });

    std::vector<std::string> keys;
    keys.reserve(items.size());
    for (auto const & it : items) keys.push_back(it.key);
    return keys;
}

CorePlotting::LayoutRequest OpenGLWidget::buildLayoutRequest() const {
    CorePlotting::LayoutRequest request;
    request.viewport_y_min = _view_state.y_min;
    request.viewport_y_max = _view_state.y_max;
    request.global_zoom = _view_state.global_zoom;
    request.global_vertical_scale = _view_state.global_vertical_scale;
    request.vertical_pan_offset = _view_state.vertical_pan_offset;

    // Collect visible analog series keys and order by spike sorter config
    std::vector<std::string> visible_analog_keys;
    for (auto const & [key, data] : _analog_series) {
        if (data.display_options->style.is_visible) {
            visible_analog_keys.push_back(key);
        }
    }
    
    // Apply spike sorter ordering if any configs exist
    if (!_spike_sorter_configs.empty()) {
        visible_analog_keys = orderAnalogKeysByConfig(visible_analog_keys);
    }
    
    // Add analog series in order
    for (auto const & key : visible_analog_keys) {
        request.series.emplace_back(key, CorePlotting::SeriesType::Analog, true);
    }

    // Add digital event series (stacked events after analog series, full-canvas events as non-stackable)
    for (auto const & [key, data] : _digital_event_series) {
        if (!data.display_options->style.is_visible) continue;
        
        bool is_stacked = (data.display_options->display_mode == EventDisplayMode::Stacked);
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, is_stacked);
    }

    // Add digital interval series (always full-canvas, non-stackable)
    for (auto const & [key, data] : _digital_interval_series) {
        if (!data.display_options->style.is_visible) continue;
        
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalInterval, false);
    }

    return request;
}

void OpenGLWidget::computeAndApplyLayout() {
    if (!_layout_response_dirty) {
        return;
    }

    // Build layout request from current series state
    CorePlotting::LayoutRequest request = buildLayoutRequest();
    
    // Compute layout using LayoutEngine
    _cached_layout_response = _layout_engine.compute(request);
    
    // Apply computed layout to display options
    // This updates each series' allocated_y_center and allocated_height
    for (auto const & layout : _cached_layout_response.layouts) {
        // Find and update analog series
        auto analog_it = _analog_series.find(layout.series_id);
        if (analog_it != _analog_series.end()) {
            analog_it->second.display_options->layout.allocated_y_center = layout.y_transform.offset;
            analog_it->second.display_options->layout.allocated_height = layout.y_transform.gain * 2.0f;
            continue;
        }
        
        // Find and update digital event series
        auto event_it = _digital_event_series.find(layout.series_id);
        if (event_it != _digital_event_series.end()) {
            event_it->second.display_options->layout.allocated_y_center = layout.y_transform.offset;
            event_it->second.display_options->layout.allocated_height = layout.y_transform.gain * 2.0f;
            continue;
        }
        
        // Find and update digital interval series
        auto interval_it = _digital_interval_series.find(layout.series_id);
        if (interval_it != _digital_interval_series.end()) {
            interval_it->second.display_options->layout.allocated_y_center = layout.y_transform.offset;
            interval_it->second.display_options->layout.allocated_height = layout.y_transform.gain * 2.0f;
            continue;
        }
    }
    
    // Note: _rectangle_batch_key_map is now populated by SceneBuilder in renderWithSceneRenderer()
    // This ensures the batch key map stays synchronized with the actual rendered batches

    _layout_response_dirty = false;
}

void OpenGLWidget::loadSpikeSorterConfiguration(std::string const & group_name,
                                                 std::vector<ChannelPosition> const & positions) {
    _spike_sorter_configs[group_name] = positions;
    _layout_response_dirty = true;
    updateCanvas(_time);
}

void OpenGLWidget::clearSpikeSorterConfiguration(std::string const & group_name) {
    _spike_sorter_configs.erase(group_name);
    _layout_response_dirty = true;
    updateCanvas(_time);
}