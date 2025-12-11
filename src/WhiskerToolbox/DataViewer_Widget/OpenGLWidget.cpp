#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/utils/color.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/AnalogTimeSeries/MVP_AnalogTimeSeries.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/MVP_DigitalEvent.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/MVP_DigitalInterval.hpp"
#include "DataViewer/PlottingManager/PlottingManager.hpp"
#include "DataViewer_Widget.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
#include <QPointer>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <ranges>


// This was a helpful resource for making a dashed line:
//https://stackoverflow.com/questions/52928678/dashed-line-in-opengl3

/*

Currently this is pretty hacky OpenGL code that will be refactored to take advantage of
OpenGL.

I will establish a Model-View-Projection matrix framework.

1) model matrix (object to world space)
    - can be used to shift data up or down?
2) view matrix (world space to camera space)
    - Camera panning
3) projection matrix (camera space to clip space)
    - orthographic projection
    - zooming and visible area definition

I will also only select the data that is present


*/

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
                if (data.display_options->is_visible) {
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
    if (_is_dragging_interval) {
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

        // Adjust vertical offset based on movement
        _verticalPanOffset += normalizedDeltaY;

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
            if (data.display_options->is_visible) {
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
        if (_is_dragging_interval) {
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

    m_vbo.destroy();
    m_vao.destroy();

    doneCurrent();

    _gl_initialized = false;
}

QOpenGLShaderProgram * create_shader_program(char const * vertexShaderSource,
                                             char const * fragmentShaderSource) {
    auto prog = new QOpenGLShaderProgram;
    prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);

    prog->link();

    return prog;
}

// Replace the old create_shader_program with a new version that loads from resource files for the dashed shader
QOpenGLShaderProgram * create_shader_program_from_resource(QString const & vertexResource, QString const & fragmentResource) {
    auto prog = new QOpenGLShaderProgram;
    prog->addShaderFromSourceFile(QOpenGLShader::Vertex, vertexResource);
    prog->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentResource);
    prog->link();
    return prog;
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

/**
 * @brief OpenGLWidget::drawDigitalEventSeries
 *
 * Each event is specified by a single time point.
 * We can find which of the time points are within the visible time frame
 * After those are found, we will draw a vertical line at that time point
 * 
 * Now supports two display modes:
 * - Stacked: Events are positioned in separate horizontal lanes with configurable spacing
 * - Full Canvas: Events stretch from top to bottom of the canvas (original behavior)
 */
void OpenGLWidget::drawDigitalEventSeries() {
    int r, g, b;
    auto const start_time = _xAxis.getStart();
    auto const end_time = _xAxis.getEnd();
    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    auto const min_y = _yMin;
    auto const max_y = _yMax;

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

    // Count visible event series for stacked positioning
    int visible_event_count = 0;
    for (auto const & [key, event_data]: _digital_event_series) {
        if (event_data.display_options->is_visible) {
            visible_event_count++;
        }
    }

    if (visible_event_count == 0 || !_master_time_frame) {
        glUseProgram(0);
        return;
    }

    // Calculate center coordinate for stacked mode (similar to analog series)
    //float const center_coord = -0.5f * 0.1f * (static_cast<float>(visible_event_count - 1));// Use default spacing for center calculation

    int visible_series_index = 0;// counts all visible event series (for iteration)
    int stacked_series_index = 0;// index among stacked-mode event series only
    int const total_analog_visible = static_cast<int>(_plotting_manager->getVisibleAnalogSeriesKeys().size());
    // Count stacked-mode events (exclude FullCanvas from stackable count)
    int stacked_event_count = 0;
    for (auto const & [key, event_data]: _digital_event_series) {
        if (event_data.display_options->is_visible &&
            event_data.display_options->display_mode == EventDisplayMode::Stacked) {
            stacked_event_count++;
        }
    }
    int const total_stackable_series = total_analog_visible + stacked_event_count;

    for (auto const & [key, event_data]: _digital_event_series) {
        auto const & series = event_data.series;
        auto const & display_options = event_data.display_options;

        if (!display_options->is_visible) continue;

        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = display_options->alpha;

        auto visible_events = series->getEventsInRange(TimeFrameIndex(start_time),
                                                       TimeFrameIndex(end_time),
                                                       *_master_time_frame);

        // === MVP MATRIX SETUP ===

        // We need to check if we have a PlottingManager reference
        if (!_plotting_manager) {
            std::cerr << "Warning: PlottingManager not set in OpenGLWidget" << std::endl;
            continue;
        }

        // Determine plotting mode and allocate accordingly
        display_options->plotting_mode = (display_options->display_mode == EventDisplayMode::Stacked)
                                                 ? EventPlottingMode::Stacked
                                                 : EventPlottingMode::FullCanvas;

        float allocated_y_center = 0.0f;
        float allocated_height = 0.0f;
        if (display_options->plotting_mode == EventPlottingMode::Stacked) {
            // Use global stacked allocation across analog + stacked events
            _plotting_manager->calculateGlobalStackedAllocation(-1, stacked_series_index, total_stackable_series,
                                                                allocated_y_center, allocated_height);
            stacked_series_index++;
        } else {
            // Full canvas allocation
            allocated_y_center = (_plotting_manager->viewport_y_min + _plotting_manager->viewport_y_max) * 0.5f;
            allocated_height = _plotting_manager->viewport_y_max - _plotting_manager->viewport_y_min;
        }

        display_options->allocated_y_center = allocated_y_center;
        display_options->allocated_height = allocated_height;

        // Apply PlottingManager pan offset
        _plotting_manager->setPanOffset(_verticalPanOffset);

        auto Model = new_getEventModelMat(*display_options, *_plotting_manager);
        auto View = new_getEventViewMat(*display_options, *_plotting_manager);
        auto Projection = new_getEventProjectionMat(start_time, end_time, _yMin, _yMax, *_plotting_manager);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Set color and alpha uniforms
        glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
        glUniform1f(m_alphaLoc, alpha);

        // Set line thickness from display options
        glLineWidth(static_cast<float>(display_options->line_thickness));

        for (auto const & event: visible_events) {
            // Calculate X position in master time frame coordinates for consistent rendering
            float xCanvasPos;
            if (series->getTimeFrame().get() == _master_time_frame.get()) {
                // Same time frame - event is already in correct coordinates
                xCanvasPos = static_cast<float>(event.getValue());
            } else {
                // Different time frames - convert event index to time, then to master time frame
                float event_time = static_cast<float>(series->getTimeFrame()->getTimeAtIndex(event));
                xCanvasPos = event_time;// This should work if both time frames use the same time units
            }

            // Provide local [-1, 1] vertical endpoints; Model handles placement/scale
            std::array<GLfloat, 8> vertices = {
                    xCanvasPos, -1.0f, 0.0f, 1.0f,
                    xCanvasPos, 1.0f, 0.0f, 1.0f};

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 2;// number of vertices to render
            glDrawArrays(GL_LINES, first, count);
        }

        visible_series_index++;
    }

    // Reset line width to default
    glLineWidth(1.0f);
    glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::drawDigitalIntervalSeries() {
    int r, g, b;
    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());

    //auto const min_y = _yMin;
    //auto const max_y = _yMax;

    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

    if (!_master_time_frame) {
        glUseProgram(0);
        return;
    }

    for (auto const & [key, interval_data]: _digital_interval_series) {
        auto const & series = interval_data.series;
        auto const & display_options = interval_data.display_options;

        if (!display_options->is_visible) continue;

        // Get only the intervals that overlap with the visible range
        // These will be
        auto visible_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                TimeFrameIndex(static_cast<int64_t>(start_time)),
                TimeFrameIndex(static_cast<int64_t>(end_time)),
                *_master_time_frame);

        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = display_options->alpha;

        // === MVP MATRIX SETUP ===

        // We need to check if we have a PlottingManager reference
        if (!_plotting_manager) {
            std::cerr << "Warning: PlottingManager not set in OpenGLWidget" << std::endl;
            continue;
        }

        // Calculate coordinate allocation from PlottingManager
        // Digital intervals typically use full canvas allocation
        float allocated_y_center, allocated_height;
        _plotting_manager->calculateDigitalIntervalSeriesAllocation(0, allocated_y_center, allocated_height);

        display_options->allocated_y_center = allocated_y_center;
        display_options->allocated_height = allocated_height;

        // Apply PlottingManager pan offset
        _plotting_manager->setPanOffset(_verticalPanOffset);

        auto Model = new_getIntervalModelMat(*display_options, *_plotting_manager);
        auto View = new_getIntervalViewMat(*_plotting_manager);
        auto Projection = new_getIntervalProjectionMat(start_time, end_time, _yMin, _yMax, *_plotting_manager);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Set color and alpha uniforms
        glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
        glUniform1f(m_alphaLoc, alpha);

        for (auto const & interval: visible_intervals) {

            std::cout << "interval.start:" << interval.start << "interval.end:" << interval.end << std::endl;

            auto start = static_cast<float>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval.start)));
            auto end = static_cast<float>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(interval.end)));

            //Clip the interval to the visible range
            start = std::max(start, start_time);
            end = std::min(end, end_time);

            float const xStart = start;
            float const xEnd = end;

            // Use normalized coordinates for intervals
            // The Model matrix will handle positioning and scaling
            float const interval_y_min = -1.0f;// Bottom of interval in local coordinates
            float const interval_y_max = +1.0f;// Top of interval in local coordinates

            // Create 4D vertices (x, y, 0, 1) to match the shader expectations
            std::array<GLfloat, 16> vertices = {
                    xStart, interval_y_min, 0.0f, 1.0f,
                    xEnd, interval_y_min, 0.0f, 1.0f,
                    xEnd, interval_y_max, 0.0f, 1.0f,
                    xStart, interval_y_max, 0.0f, 1.0f};

            m_vbo.bind();
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));
            m_vbo.release();

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 4;// number of indexes to render
            glDrawArrays(GL_TRIANGLE_FAN, first, count);
        }

        // Draw highlighting for selected intervals
        auto selected_interval = getSelectedInterval(key);
        if (selected_interval.has_value() && !(_is_dragging_interval && _dragging_series_key == key)) {
            auto const [sel_start_time, sel_end_time] = selected_interval.value();

            // Check if the selected interval overlaps with visible range
            if (sel_end_time >= static_cast<int64_t>(start_time) && sel_start_time <= static_cast<int64_t>(end_time)) {
                // Clip the selected interval to the visible range
                float const highlighted_start = std::max(static_cast<float>(sel_start_time), start_time);
                float const highlighted_end = std::min(static_cast<float>(sel_end_time), end_time);

                // Draw a thick border around the selected interval
                // Use a brighter version of the same color for highlighting
                float const highlight_rNorm = std::min(1.0f, rNorm + 0.3f);
                float const highlight_gNorm = std::min(1.0f, gNorm + 0.3f);
                float const highlight_bNorm = std::min(1.0f, bNorm + 0.3f);

                // Set line width for highlighting
                glLineWidth(4.0f);

                // Draw the four border lines of the rectangle
                // Set highlight color uniforms
                glUniform3f(m_colorLoc, highlight_rNorm, highlight_gNorm, highlight_bNorm);
                glUniform1f(m_alphaLoc, 1.0f);

                // Bottom edge
                std::array<GLfloat, 8> bottom_edge = {
                        highlighted_start, -1.0f, 0.0f, 1.0f,
                        highlighted_end, -1.0f, 0.0f, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(bottom_edge.data(), bottom_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Top edge
                std::array<GLfloat, 8> top_edge = {
                        highlighted_start, 1.0f, 0.0f, 1.0f,
                        highlighted_end, 1.0f, 0.0f, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(top_edge.data(), top_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Left edge
                std::array<GLfloat, 8> left_edge = {
                        highlighted_start, -1.0f, 0.0f, 1.0f,
                        highlighted_start, 1.0f, 0.0f, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(left_edge.data(), left_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Right edge
                std::array<GLfloat, 8> right_edge = {
                        highlighted_end, -1.0f, 0.0f, 1.0f,
                        highlighted_end, 1.0f, 0.0f, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(right_edge.data(), right_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Reset line width
                glLineWidth(1.0f);
            }
        }
    }

    glUseProgram(0);
}

///////////////////////////////////////////////////////////////////////////////

void OpenGLWidget::drawAnalogSeries() {
    int r, g, b;

    auto const start_time = TimeFrameIndex(_xAxis.getStart());
    auto const end_time = TimeFrameIndex(_xAxis.getEnd());

    auto axesProgram = ShaderManager::instance().getProgram("axes");
    if (axesProgram) glUseProgram(axesProgram->getProgramId());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    if (!_master_time_frame) {
        glUseProgram(0);
        return;
    }

    int i = 0;

    for (auto const & [key, analog_data]: _analog_series) {
        auto const & series = analog_data.series;
        auto const & data = series->getAnalogTimeSeries();
        //if (!series->hasTimeFrameV2()) {
        //    continue;
        //}
        //auto time_frame = series->getTimeFrameV2().value();
        auto time_frame = analog_data.time_frame;

        auto const & display_options = analog_data.display_options;

        if (!display_options->is_visible) continue;

        // Calculate coordinate allocation from PlottingManager
        // For now, we'll use the analog series index to allocate coordinates
        // This is a temporary bridge until we fully migrate series management to PlottingManager
        if (!_plotting_manager) {
            std::cerr << "Warning: PlottingManager not set in OpenGLWidget" << std::endl;
            continue;
        }

        // Compute global stacked allocation so analog shares space with stacked events
        float allocated_y_center = 0.0f;
        float allocated_height = 0.0f;

        // Determine analog index among visible analogs using current analog-only allocation
        int const analog_visible_count = static_cast<int>(_plotting_manager->getVisibleAnalogSeriesKeys().size());
        int analog_index = i;
        float tmp_center = 0.0f;
        float tmp_height = 0.0f;
        if (_plotting_manager->getAnalogSeriesAllocationForKey(key, tmp_center, tmp_height) && tmp_height > 0.0f) {
            float const idxf = (tmp_center - _plotting_manager->viewport_y_min) / tmp_height - 0.5f;
            analog_index = std::clamp(static_cast<int>(std::round(idxf)), 0, std::max(0, analog_visible_count - 1));
        }

        // Count stacked-mode events (exclude FullCanvas)
        int stacked_event_count = 0;
        for (auto const & [ekey, edata]: _digital_event_series) {
            if (edata.display_options->is_visible && edata.display_options->display_mode == EventDisplayMode::Stacked) {
                stacked_event_count++;
            }
        }
        int const total_stackable_series = analog_visible_count + stacked_event_count;

        if (total_stackable_series > 0) {
            _plotting_manager->calculateGlobalStackedAllocation(analog_index, -1, total_stackable_series,
                                                                allocated_y_center, allocated_height);
        } else {
            // Fallback to analog-only allocation
            if (!_plotting_manager->getAnalogSeriesAllocationForKey(key, allocated_y_center, allocated_height)) {
                _plotting_manager->calculateAnalogSeriesAllocation(i, allocated_y_center, allocated_height);
            }
        }

        display_options->allocated_y_center = allocated_y_center;
        display_options->allocated_height = allocated_height;

        // Set the color for the current series
        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;

        m_vertices.clear();

        auto series_start_index = getTimeIndexForSeries(start_time, _master_time_frame.get(), time_frame.get());
        auto series_end_index = getTimeIndexForSeries(end_time, _master_time_frame.get(), time_frame.get());

        // === MVP MATRIX SETUP ===

        // Apply PlottingManager pan offset
        _plotting_manager->setPanOffset(_verticalPanOffset);

        auto Model = new_getAnalogModelMat(*display_options, display_options->cached_std_dev, display_options->cached_mean, *_plotting_manager);
        auto View = new_getAnalogViewMat(*_plotting_manager);
        auto Projection = new_getAnalogProjectionMat(start_time, end_time, _yMin, _yMax, *_plotting_manager);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Set color and alpha uniforms
        glUniform3f(m_colorLoc, rNorm, gNorm, bNorm);
        glUniform1f(m_alphaLoc, 1.0f);

        // Use iterator-based approach which automatically uses fast path (span/pointer)
        // for contiguous data and slow path for non-contiguous (memory-mapped with stride)
        auto analog_range = series->getTimeValueRangeInTimeFrameIndexRange(series_start_index,
                                                                           series_end_index);

        if (display_options->gap_handling == AnalogGapHandling::AlwaysConnect) {

            m_vertices.clear();
            for (auto const& [time_idx, value] : analog_range) {
                auto const xCanvasPos = time_frame->getTimeAtIndex(time_idx);
                auto const yCanvasPos = value;

                m_vertices.push_back(xCanvasPos);
                m_vertices.push_back(yCanvasPos);
                m_vertices.push_back(0.0f);// z coordinate
                m_vertices.push_back(1.0f);// w coordinate
            }
            
            if (m_vertices.empty()) {
                i++;
                continue;
            }
            
            m_vbo.bind();
            m_vbo.allocate(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));
            m_vbo.release();

            // Set line thickness from display options
            glLineWidth(static_cast<float>(display_options->line_thickness));
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(m_vertices.size() / 4));

        } else if (display_options->gap_handling == AnalogGapHandling::DetectGaps) {
            // Draw multiple line segments, breaking at gaps
            // Set line thickness before drawing segments
            glLineWidth(static_cast<float>(display_options->line_thickness));
            _drawAnalogSeriesWithGapDetection(time_frame, analog_range,
                                              display_options->gap_threshold);

        } else if (display_options->gap_handling == AnalogGapHandling::ShowMarkers) {
            // Draw individual markers instead of lines
            _drawAnalogSeriesAsMarkers(time_frame, analog_range);
        }


        i++;
    }

    // Reset line width to default
    glLineWidth(1.0f);
    glUseProgram(0);
}

void OpenGLWidget::_drawAnalogSeriesWithGapDetection(std::shared_ptr<TimeFrame> const & time_frame,
                                                     AnalogTimeSeries::TimeValueRangeView analog_range,
                                                     float gap_threshold) {

    std::vector<GLfloat> segment_vertices;
    int prev_index = -1;
    bool first_point = true;

    for (auto const& [time_idx, value] : analog_range) {
        auto const xCanvasPos = time_frame->getTimeAtIndex(time_idx);
        auto const yCanvasPos = value;

        // Check for gap if this isn't the first point
        if (!first_point) {
            int const current_index = time_idx.getValue();
            int const gap_size = current_index - prev_index;

            if (gap_size > static_cast<int>(gap_threshold)) {
                // Gap detected - draw current segment and start new one
                if (segment_vertices.size() >= 4) {// At least 1 point (4 floats)
                    m_vbo.bind();
                    m_vbo.allocate(segment_vertices.data(), static_cast<int>(segment_vertices.size() * sizeof(GLfloat)));
                    m_vbo.release();

                    if (segment_vertices.size() >= 8) {// At least 2 points (8 floats)
                        glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(segment_vertices.size() / 4));
                    } else {// Single point - draw as a small marker
                        glDrawArrays(GL_POINTS, 0, static_cast<int>(segment_vertices.size() / 4));
                    }
                }
                segment_vertices.clear();
            }
        }

        // Add current point to segment (4D coordinates: x, y, 0, 1)
        segment_vertices.push_back(xCanvasPos);
        segment_vertices.push_back(yCanvasPos);
        segment_vertices.push_back(0.0f);// z coordinate
        segment_vertices.push_back(1.0f);// w coordinate

        prev_index = time_idx.getValue();
        first_point = false;
    }

    // Draw final segment
    if (segment_vertices.size() >= 4) {// At least 1 point (4 floats)
        m_vbo.bind();
        m_vbo.allocate(segment_vertices.data(), static_cast<int>(segment_vertices.size() * sizeof(GLfloat)));
        m_vbo.release();

        if (segment_vertices.size() >= 8) {// At least 2 points (8 floats)
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(segment_vertices.size() / 4));
        } else {
            // Single point - draw as a small marker
            glDrawArrays(GL_POINTS, 0, static_cast<int>(segment_vertices.size() / 4));
        }
    }
}

void OpenGLWidget::_drawAnalogSeriesAsMarkers(std::shared_ptr<TimeFrame> const & time_frame,
                                              AnalogTimeSeries::TimeValueRangeView analog_range) {
    m_vertices.clear();

    for (auto const& [time_idx, value] : analog_range) {
        auto const xCanvasPos = time_frame->getTimeAtIndex(time_idx);
        auto const yCanvasPos = value;

        m_vertices.push_back(xCanvasPos);
        m_vertices.push_back(yCanvasPos);
        m_vertices.push_back(0.0f);// z coordinate
        m_vertices.push_back(1.0f);// w coordinate
    }

    if (!m_vertices.empty()) {
        m_vbo.bind();
        m_vbo.allocate(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));
        m_vbo.release();

        // Set point size for better visibility
        //glPointSize(3.0f);
        glDrawArrays(GL_POINTS, 0, static_cast<int>(m_vertices.size() / 4));
        //glPointSize(1.0f); // Reset to default
    }
}

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

    int64_t const zoom = _xAxis.getEnd() - _xAxis.getStart();
    _xAxis.setCenterAndZoom(currentTime, zoom);

    // Update Y boundaries based on pan and zoom
    _updateYViewBoundaries();

    // Draw the series
    drawDigitalEventSeries();
    drawDigitalIntervalSeries();
    drawAnalogSeries();

    drawAxis();

    drawGridLines();

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
            0.0f, _yMin, 0.0f, 1.0f,
            0.0f, _yMax, 0.0f, 1.0f};

    m_vbo.bind();
    m_vbo.allocate(lineVertices.data(), lineVertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_LINES, 0, 2);
    glUseProgram(0);
}

void OpenGLWidget::addAnalogTimeSeries(
        std::string const & key,
        std::shared_ptr<AnalogTimeSeries> series,
        std::shared_ptr<TimeFrame> time_frame,
        std::string const & color) {

    auto display_options = std::make_unique<NewAnalogTimeSeriesDisplayOptions>();

    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_analog_series.size()) : color;
    display_options->is_visible = true;

    // Calculate scale factor based on standard deviation
    auto start_time = std::chrono::high_resolution_clock::now();
    setAnalogIntrinsicProperties(series.get(), *display_options);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Standard deviation calculation took " << duration.count() << " milliseconds" << std::endl;
    display_options->scale_factor = display_options->cached_std_dev * 5.0f;
    display_options->user_scale_factor = 1.0f;// Default user scale

    if (time_frame->getTotalFrameCount() / 5 > series->getNumSamples()) {
        display_options->gap_handling = AnalogGapHandling::AlwaysConnect;
        display_options->enable_gap_detection = false;

    } else {
        display_options->enable_gap_detection = true;
        display_options->gap_handling = AnalogGapHandling::DetectGaps;
        // Set gap threshold to 0.1% of total frames, with a minimum floor of 2 to work with integer time frames
        float const calculated_threshold = static_cast<float>(time_frame->getTotalFrameCount()) / 1000.0f;
        display_options->gap_threshold = std::max(2.0f, calculated_threshold);
    }

    _analog_series[key] = AnalogSeriesData{
            std::move(series),
            std::move(time_frame),
            std::move(display_options)};

    updateCanvas(_time);
}

void OpenGLWidget::removeAnalogTimeSeries(std::string const & key) {
    auto item = _analog_series.find(key);
    if (item != _analog_series.end()) {
        _analog_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalEventSeries(
        std::string const & key,
        std::shared_ptr<DigitalEventSeries> series,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalEventSeriesDisplayOptions>();

    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_event_series.size()) : color;
    display_options->is_visible = true;

    _digital_event_series[key] = DigitalEventSeriesData{
            std::move(series),
            std::move(display_options)};

    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalEventSeries(std::string const & key) {
    auto item = _digital_event_series.find(key);
    if (item != _digital_event_series.end()) {
        _digital_event_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalIntervalSeries(
        std::string const & key,
        std::shared_ptr<DigitalIntervalSeries> series,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalIntervalSeriesDisplayOptions>();

    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_interval_series.size()) : color;
    display_options->is_visible = true;

    _digital_interval_series[key] = DigitalIntervalSeriesData{
            std::move(series),
            std::move(display_options)};

    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalIntervalSeries(std::string const & key) {
    auto item = _digital_interval_series.find(key);
    if (item != _digital_interval_series.end()) {
        _digital_interval_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::_addSeries(std::string const & key) {
    static_cast<void>(key);
    //auto item = _series_y_position.find(key);
}

void OpenGLWidget::_removeSeries(std::string const & key) {
    auto item = _series_y_position.find(key);
    if (item != _series_y_position.end()) {
        _series_y_position.erase(item);
    }
}

void OpenGLWidget::clearSeries() {
    _analog_series.clear();
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

    auto const start_time = _xAxis.getStart();
    auto const end_time = _xAxis.getEnd();

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
        gridLine.yStart = _yMin;
        gridLine.yEnd = _yMax;
        gridLine.dashLength = 3.0f;// Shorter dashes for grid lines
        gridLine.gapLength = 3.0f; // Shorter gaps for grid lines

        drawDashedLine(gridLine);
    }
}

void OpenGLWidget::_updateYViewBoundaries() {
    /*
    float viewHeight = 2.0f;

    // Calculate center point (adjusted by vertical pan)
    float centerY = _verticalPanOffset;
    
    // Calculate min and max values
    _yMin = centerY - (viewHeight / 2.0f);
    _yMax = centerY + (viewHeight / 2.0f);
     */
}

float OpenGLWidget::canvasXToTime(float canvas_x) const {
    // Convert canvas pixel coordinate to time coordinate
    float const canvas_width = static_cast<float>(width());
    float const normalized_x = canvas_x / canvas_width;// 0.0 to 1.0

    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());

    return start_time + normalized_x * (end_time - start_time);
}

float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const & series_key) const {
    // Get canvas dimensions
    auto [canvas_width, canvas_height] = getCanvasSize();

    // Convert canvas Y to normalized coordinates [0, 1] where 0 is bottom, 1 is top
    float const normalized_y = 1.0f - (canvas_y / static_cast<float>(canvas_height));

    // Convert to view coordinates using current viewport bounds (accounting for pan offset)
    float const dynamic_min_y = _yMin + _verticalPanOffset;
    float const dynamic_max_y = _yMax + _verticalPanOffset;
    float const view_y = dynamic_min_y + normalized_y * (dynamic_max_y - dynamic_min_y);

    // Find the series configuration to get positioning info
    auto const analog_it = _analog_series.find(series_key);
    if (analog_it == _analog_series.end()) {
        return 0.0f;// Series not found
    }

    auto const & display_options = analog_it->second.display_options;

    // Check if this series uses VerticalSpaceManager positioning
    if (display_options->y_offset != 0.0f && display_options->allocated_height > 0.0f) {
        // VerticalSpaceManager mode: series is positioned at y_offset with its own scaling
        float const series_local_y = view_y - display_options->y_offset;

        // Apply inverse of the scaling used in rendering to get actual analog value
        auto const series = analog_it->second.series;
        auto const stdDev = getCachedStdDev(*series, *display_options);
        auto const user_scale_combined = display_options->user_scale_factor * _global_zoom;

        // Calculate the same scaling factors used in rendering
        float const usable_height = display_options->allocated_height * 0.8f;
        float const base_amplitude_scale = 1.0f / stdDev;
        float const height_scale = usable_height / 1.0f;
        float const amplitude_scale = base_amplitude_scale * height_scale * user_scale_combined;

        // Apply inverse scaling to get actual data value
        return series_local_y / amplitude_scale;
    } else {
        // Legacy mode: use corrected calculation
        float const adjusted_y = view_y;// No pan offset adjustment needed since it's in projection

        // Calculate series center and scaling for legacy mode
        auto series_pos_it = _series_y_position.find(series_key);
        if (series_pos_it == _series_y_position.end()) {
            // Series not found in position map, return 0 as fallback
            return 0.0f;
        }
        int const series_index = series_pos_it->second;
        float const center_coord = -0.5f * _ySpacing * (static_cast<float>(_analog_series.size() - 1));
        float const series_y_offset = static_cast<float>(series_index) * _ySpacing + center_coord;

        float const relative_y = adjusted_y - series_y_offset;

        // Use corrected scaling calculation
        auto const series = analog_it->second.series;
        auto const stdDev = getCachedStdDev(*series, *display_options);
        auto const user_scale_combined = display_options->user_scale_factor * _global_zoom;
        float const legacy_amplitude_scale = (1.0f / stdDev) * user_scale_combined;

        return relative_y / legacy_amplitude_scale;
    }
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

    static_cast<void>(canvas_y);

    float const time_coord = canvasXToTime(canvas_x);
    constexpr float EDGE_TOLERANCE_PX = 10.0f;

    // Only check selected intervals
    for (auto const & [series_key, interval_bounds]: _selected_intervals) {
        auto const [start_time, end_time] = interval_bounds;

        // Convert interval bounds to canvas coordinates
        auto const start_time_f = static_cast<float>(start_time);
        auto const end_time_f = static_cast<float>(end_time);

        // Check if we're within the interval's time range (with some tolerance)
        if (time_coord >= start_time_f - EDGE_TOLERANCE_PX && time_coord <= end_time_f + EDGE_TOLERANCE_PX) {
            // Convert time coordinates to canvas X positions for pixel-based tolerance
            float const canvas_width = static_cast<float>(width());
            auto const start_time_canvas = static_cast<float>(_xAxis.getStart());
            auto const end_time_canvas = static_cast<float>(_xAxis.getEnd());

            float const start_canvas_x = (start_time_f - start_time_canvas) / (end_time_canvas - start_time_canvas) * canvas_width;
            float const end_canvas_x = (end_time_f - start_time_canvas) / (end_time_canvas - start_time_canvas) * canvas_width;

            // Check if we're close to the left edge
            if (std::abs(canvas_x - start_canvas_x) <= EDGE_TOLERANCE_PX) {
                return std::make_pair(series_key, true);// true = left edge
            }

            // Check if we're close to the right edge
            if (std::abs(canvas_x - end_canvas_x) <= EDGE_TOLERANCE_PX) {
                return std::make_pair(series_key, false);// false = right edge
            }
        }
    }

    return std::nullopt;
}

void OpenGLWidget::startIntervalDrag(std::string const & series_key, bool is_left_edge, QPoint const & start_pos) {
    auto selected_interval = getSelectedInterval(series_key);
    if (!selected_interval.has_value()) {
        return;
    }

    auto const [start_time, end_time] = selected_interval.value();

    _is_dragging_interval = true;
    _dragging_series_key = series_key;
    _dragging_left_edge = is_left_edge;
    _original_start_time = start_time;
    _original_end_time = end_time;
    _dragged_start_time = start_time;
    _dragged_end_time = end_time;
    _drag_start_pos = start_pos;

    // Disable normal mouse interactions during drag
    setCursor(Qt::SizeHorCursor);

    std::cout << "Started dragging " << (is_left_edge ? "left" : "right")
              << " edge of interval [" << start_time << ", " << end_time << "]" << std::endl;
}

void OpenGLWidget::updateIntervalDrag(QPoint const & current_pos) {
    if (!_is_dragging_interval) {
        return;
    }

    // Convert mouse position to time coordinate (in master time frame)
    float const current_time_master = canvasXToTime(static_cast<float>(current_pos.x()));

    // Get the series data
    auto it = _digital_interval_series.find(_dragging_series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - abort drag
        cancelIntervalDrag();
        return;
    }

    auto const & series = it->second.series;

    // Convert master time coordinate to series time frame index
    int64_t current_time_series_index;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame - use time coordinate directly
        current_time_series_index = static_cast<int64_t>(std::round(current_time_master));
    } else {
        // Different time frame - convert master time to series time frame index
        current_time_series_index = series->getTimeFrame()->getIndexAtTime(current_time_master).getValue();
    }

    // Convert original interval bounds to series time frame for constraints
    int64_t original_start_series, original_end_series;
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame
        original_start_series = _original_start_time;
        original_end_series = _original_end_time;
    } else {
        // Convert master time coordinates to series time frame indices
        original_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_original_start_time)).getValue();
        original_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_original_end_time)).getValue();
    }

    // Perform dragging logic in series time frame
    int64_t new_start_series, new_end_series;

    if (_dragging_left_edge) {
        // Dragging left edge - constrain to not pass right edge
        new_start_series = std::min(current_time_series_index, original_end_series - 1);
        new_end_series = original_end_series;

        // Check for collision with other intervals in series time frame
        auto overlapping_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                new_start_series, new_start_series);

        for (auto const & interval: overlapping_intervals) {
            // Skip the interval we're currently editing
            if (interval.start == original_start_series && interval.end == original_end_series) {
                continue;
            }

            // If we would overlap with another interval, stop 1 index after it
            if (new_start_series <= interval.end) {
                new_start_series = interval.end + 1;
                break;
            }
        }
    } else {
        // Dragging right edge - constrain to not pass left edge
        new_start_series = original_start_series;
        new_end_series = std::max(current_time_series_index, original_start_series + 1);

        // Check for collision with other intervals in series time frame
        auto overlapping_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                new_end_series, new_end_series);

        for (auto const & interval: overlapping_intervals) {
            // Skip the interval we're currently editing
            if (interval.start == original_start_series && interval.end == original_end_series) {
                continue;
            }

            // If we would overlap with another interval, stop 1 index before it
            if (new_end_series >= interval.start) {
                new_end_series = interval.start - 1;
                break;
            }
        }
    }

    // Validate the new interval bounds
    if (new_start_series >= new_end_series) {
        // Invalid bounds - keep current drag state
        return;
    }

    // Convert back to master time frame for display
    if (series->getTimeFrame().get() == _master_time_frame.get()) {
        // Same time frame
        _dragged_start_time = new_start_series;
        _dragged_end_time = new_end_series;
    } else {
        // Convert series indices back to master time coordinates
        try {
            _dragged_start_time = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(new_start_series)));
            _dragged_end_time = static_cast<int64_t>(series->getTimeFrame()->getTimeAtIndex(TimeFrameIndex(new_end_series)));
        } catch (...) {
            // Conversion failed - abort drag
            cancelIntervalDrag();
            return;
        }
    }

    // Trigger redraw to show the dragged interval
    updateCanvas(_time);
}

void OpenGLWidget::finishIntervalDrag() {
    if (!_is_dragging_interval) {
        return;
    }

    // Get the series data
    auto it = _digital_interval_series.find(_dragging_series_key);
    if (it == _digital_interval_series.end()) {
        // Series not found - abort drag
        cancelIntervalDrag();
        return;
    }

    auto const & series = it->second.series;
    
    try {
        // Convert all coordinates to series time frame for data operations
        int64_t original_start_series, original_end_series, new_start_series, new_end_series;

        if (series->getTimeFrame().get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            original_start_series = _original_start_time;
            original_end_series = _original_end_time;
            new_start_series = _dragged_start_time;
            new_end_series = _dragged_end_time;
        } else {
            // Convert master time coordinates to series time frame indices
            original_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_original_start_time)).getValue();
            original_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_original_end_time)).getValue();
            new_start_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_dragged_start_time)).getValue();
            new_end_series = series->getTimeFrame()->getIndexAtTime(static_cast<float>(_dragged_end_time)).getValue();
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
        setSelectedInterval(_dragging_series_key, _dragged_start_time, _dragged_end_time);

        std::cout << "Finished dragging interval. Original: ["
                  << _original_start_time << ", " << _original_end_time
                  << "] -> New: [" << _dragged_start_time << ", " << _dragged_end_time << "]" << std::endl;

    } catch (...) {
        // Error occurred during conversion or data update - abort drag
        std::cout << "Error during interval drag completion - keeping original interval" << std::endl;
        cancelIntervalDrag();
        return;
    }

    // Reset drag state
    _is_dragging_interval = false;
    _dragging_series_key.clear();
    setCursor(Qt::ArrowCursor);

    // Trigger final redraw
    updateCanvas(_time);
}

void OpenGLWidget::cancelIntervalDrag() {
    if (!_is_dragging_interval) {
        return;
    }

    std::cout << "Cancelled interval drag" << std::endl;

    // Reset drag state without applying changes
    _is_dragging_interval = false;
    _dragging_series_key.clear();
    setCursor(Qt::ArrowCursor);

    // Trigger redraw to remove the dragged interval visualization
    updateCanvas(_time);
}

void OpenGLWidget::drawDraggedInterval() {
    if (!_is_dragging_interval) {
        return;
    }

    // Get the series data for rendering
    auto it = _digital_interval_series.find(_dragging_series_key);
    if (it == _digital_interval_series.end()) {
        return;
    }

    auto const & display_options = it->second.display_options;

    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());
    auto const min_y = _yMin;
    auto const max_y = _yMax;

    // Check if the dragged interval is visible
    if (_dragged_end_time < static_cast<int64_t>(start_time) || _dragged_start_time > static_cast<int64_t>(end_time)) {
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
    hexToRGB(display_options->hex_color, r, g, b);
    float const rNorm = static_cast<float>(r) / 255.0f;
    float const gNorm = static_cast<float>(g) / 255.0f;
    float const bNorm = static_cast<float>(b) / 255.0f;

    // Clip the dragged interval to visible range
    float const dragged_start = std::max(static_cast<float>(_dragged_start_time), start_time);
    float const dragged_end = std::min(static_cast<float>(_dragged_end_time), end_time);

    // Draw the original interval dimmed (alpha = 0.2)
    float const original_start = std::max(static_cast<float>(_original_start_time), start_time);
    float const original_end = std::min(static_cast<float>(_original_end_time), end_time);

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

namespace TimeSeriesDefaultValues {
std::string getColorForIndex(size_t index) {
    if (index < DEFAULT_COLORS.size()) {
        return DEFAULT_COLORS[index];
    } else {
        return generateRandomColor();
    }
}
}// namespace TimeSeriesDefaultValues

void OpenGLWidget::mouseDoubleClickEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        // Check if we're double-clicking over a digital interval series
        //float const canvas_x = static_cast<float>(event->pos().x());
        //float const canvas_y = static_cast<float>(event->pos().y());

        // Find which digital interval series (if any) is at this Y position
        // For now, use the first visible digital interval series
        // TODO: Improve this to detect which series based on Y coordinate
        for (auto const & [series_key, data]: _digital_interval_series) {
            if (data.display_options->is_visible) {
                startNewIntervalCreation(series_key, event->pos());
                return;
            }
        }
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OpenGLWidget::startNewIntervalCreation(std::string const & series_key, QPoint const & start_pos) {
    // Don't start if we're already dragging an interval
    if (_is_dragging_interval || _is_creating_new_interval) {
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

    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());
    auto const min_y = _yMin;
    auto const max_y = _yMax;

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
    hexToRGB(display_options->hex_color, r, g, b);
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
    if (!_plotting_manager) {
        return std::nullopt;
    }

    // Convert canvas Y to normalized device coordinates (NDC)
    // In OpenGL, Y is inverted: top of window is -1, bottom is +1 in our view
    float const ndc_y = -1.0f + 2.0f * (static_cast<float>(height()) - canvas_y) / static_cast<float>(height());

    // Apply vertical pan offset to get the actual Y position in the coordinate system
    float const adjusted_y = ndc_y - _verticalPanOffset;

    // Check analog series first (in stacked mode)
    int analog_index = 0;
    for (auto const & [key, analog_data]: _analog_series) {
        if (!analog_data.display_options->is_visible) {
            continue;
        }

        // Get the allocated space for this series
        float allocated_center = analog_data.display_options->allocated_y_center;
        float allocated_height = analog_data.display_options->allocated_height;

        // Calculate bounds for this series
        float const series_y_min = allocated_center - allocated_height * 0.5f;
        float const series_y_max = allocated_center + allocated_height * 0.5f;

        // Check if mouse Y is within this series' allocated space
        if (adjusted_y >= series_y_min && adjusted_y <= series_y_max) {
            return std::make_pair("Analog", key);
        }

        analog_index++;
    }

    // Check digital event series (only those in stacked mode)
    int event_index = 0;
    for (auto const & [key, event_data]: _digital_event_series) {
        if (!event_data.display_options->is_visible) {
            continue;
        }

        // Only check stacked events
        if (event_data.display_options->display_mode != EventDisplayMode::Stacked) {
            continue;
        }

        // Get the allocated space for this series
        float allocated_center = event_data.display_options->allocated_y_center;
        float allocated_height = event_data.display_options->allocated_height;

        // Calculate bounds for this series
        float const series_y_min = allocated_center - allocated_height * 0.5f;
        float const series_y_max = allocated_center + allocated_height * 0.5f;

        // Check if mouse Y is within this series' allocated space
        if (adjusted_y >= series_y_min && adjusted_y <= series_y_max) {
            return std::make_pair("Event", key);
        }

        event_index++;
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
