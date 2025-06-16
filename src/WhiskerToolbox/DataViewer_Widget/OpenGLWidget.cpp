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
#include "TimeFrame.hpp"
#include "shaders/colored_vertex_shader.hpp"
#include "shaders/dashed_line_shader.hpp"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
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
}

OpenGLWidget::~OpenGLWidget() {
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
        return;// Don't do other mouse move processing while dragging
    }

    if (_is_creating_new_interval) {
        // Update new interval creation
        updateNewIntervalCreation(event->pos());
        return;// Don't do other mouse move processing while creating
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
        update();// Request redraw
    } else {
        // Check for cursor changes when hovering near interval edges
        auto edge_info = findIntervalEdgeAtPosition(static_cast<float>(event->pos().x()), static_cast<float>(event->pos().y()));
        if (edge_info.has_value()) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::ArrowCursor);
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
    if (m_program == nullptr)
        return;
    makeCurrent();
    delete m_program;
    delete m_dashedProgram;
    m_program = nullptr;
    m_dashedProgram = nullptr;
    doneCurrent();
}

QOpenGLShaderProgram * create_shader_program(char const * vertexShaderSource,
                                             char const * fragmentShaderSource) {
    auto prog = new QOpenGLShaderProgram;
    prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);

    prog->link();

    return prog;
}

void OpenGLWidget::initializeGL() {

    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &OpenGLWidget::cleanup);

    initializeOpenGLFunctions();

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

    // Enable blending for transparency support
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_program = create_shader_program(vertexShaderSource, fragmentShaderSource);

    auto const m_program_ID = m_program->programId();
    glUseProgram(m_program_ID);
    m_projMatrixLoc = glGetUniformLocation(m_program_ID, "projMatrix");
    m_viewMatrixLoc = glGetUniformLocation(m_program_ID, "viewMatrix");
    m_modelMatrixLoc = glGetUniformLocation(m_program_ID, "modelMatrix");

    m_dashedProgram = create_shader_program(dashedVertexShaderSource, dashedFragmentShaderSource);

    auto const m_dashedProgram_ID = m_dashedProgram->programId();
    glUseProgram(m_dashedProgram_ID);
    m_dashedProjMatrixLoc = glGetUniformLocation(m_dashedProgram_ID, "u_mvp");
    m_dashedResolutionLoc = glGetUniformLocation(m_dashedProgram_ID, "u_resolution");
    m_dashedDashSizeLoc = glGetUniformLocation(m_dashedProgram_ID, "u_dashSize");
    m_dashedGapSizeLoc = glGetUniformLocation(m_dashedProgram_ID, "u_gapSize");

    m_vao.create();
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    setupVertexAttribs();

    //m_program->release();
    //m_dashedProgram->release();
}

void OpenGLWidget::setupVertexAttribs() {

    m_vbo.bind();// glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
    int const vertex_argument_num = 6;

    // Attribute 0: vertex positions (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), nullptr);

    // Attribute 1: color (r, g, b)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), reinterpret_cast<void *>(2 * sizeof(GLfloat)));

    // Attribute 2: alpha
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), reinterpret_cast<void *>(5 * sizeof(GLfloat)));

    //glDisableVertexAttribArray(0);
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
    auto const m_program_ID = m_program->programId();

    auto const min_y = _yMin;
    auto const max_y = _yMax;

    glUseProgram(m_program_ID);

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
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

    int visible_series_index = 0;

    for (auto const & [key, event_data]: _digital_event_series) {
        auto const & series = event_data.series;
        auto const & time_frame = event_data.time_frame;
        auto const & display_options = event_data.display_options;

        if (!display_options->is_visible) continue;

        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = display_options->alpha;

        auto visible_events = series->getEventsInRange(TimeFrameIndex(start_time),
                                                       TimeFrameIndex(end_time),
                                                       time_frame.get(),
                                                       _master_time_frame.get());

        // === MVP MATRIX SETUP ===

        // We need to check if we have a PlottingManager reference
        if (!_plotting_manager) {
            std::cerr << "Warning: PlottingManager not set in OpenGLWidget" << std::endl;
            continue;
        }

        // Calculate coordinate allocation from PlottingManager
        // For now, we'll use the visible series index to allocate coordinates
        float allocated_y_center, allocated_height;
        _plotting_manager->calculateGlobalStackedAllocation(-1, visible_series_index, visible_event_count, allocated_y_center, allocated_height);

        display_options->allocated_y_center = allocated_y_center;
        display_options->allocated_height = allocated_height;
        display_options->plotting_mode = (display_options->display_mode == EventDisplayMode::Stacked) ? EventPlottingMode::Stacked : EventPlottingMode::FullCanvas;

        // Apply PlottingManager pan offset
        _plotting_manager->setPanOffset(_verticalPanOffset);

        auto Model = new_getEventModelMat(*display_options, *_plotting_manager);
        auto View = new_getEventViewMat(*display_options, *_plotting_manager);
        auto Projection = new_getEventProjectionMat(start_time, end_time, _yMin, _yMax, *_plotting_manager);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Set line thickness from display options
        glLineWidth(static_cast<float>(display_options->line_thickness));

        for (auto const & event: visible_events) {
            // Calculate X position in master time frame coordinates for consistent rendering
            float xCanvasPos;
            if (time_frame.get() == _master_time_frame.get()) {
                // Same time frame - event is already in correct coordinates
                xCanvasPos = event;
            } else {
                // Different time frames - convert event index to time, then to master time frame
                float event_time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(static_cast<int>(event))));
                xCanvasPos = event_time;// This should work if both time frames use the same time units
            }

            std::array<GLfloat, 12> vertices = {
                    xCanvasPos, min_y, rNorm, gNorm, bNorm, alpha,
                    xCanvasPos, max_y, rNorm, gNorm, bNorm, alpha};

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 2;// number of indexes to render
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

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

    if (!_master_time_frame) {
        glUseProgram(0);
        return;
    }

    for (auto const & [key, interval_data]: _digital_interval_series) {
        auto const & series = interval_data.series;
        auto const & time_frame = interval_data.time_frame;
        auto const & display_options = interval_data.display_options;

        if (!display_options->is_visible) continue;

        // Get only the intervals that overlap with the visible range
        auto visible_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                static_cast<int64_t>(start_time),
                static_cast<int64_t>(end_time),
                [&time_frame](int64_t idx) {
                    return static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(idx)));
                });

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

        for (auto const & interval: visible_intervals) {

            auto start = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(interval.start)));
            auto end = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(interval.end)));

            //Clip the interval to the visible range
            start = std::max(start, start_time);
            end = std::min(end, end_time);

            float const xStart = start;
            float const xEnd = end;

            // Use normalized coordinates for intervals
            // The Model matrix will handle positioning and scaling
            float const interval_y_min = -1.0f;// Bottom of interval in local coordinates
            float const interval_y_max = +1.0f;// Top of interval in local coordinates

            std::array<GLfloat, 24> vertices = {
                    xStart, interval_y_min, rNorm, gNorm, bNorm, alpha,
                    xEnd, interval_y_min, rNorm, gNorm, bNorm, alpha,
                    xEnd, interval_y_max, rNorm, gNorm, bNorm, alpha,
                    xStart, interval_y_max, rNorm, gNorm, bNorm, alpha};

            //glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
            m_vbo.bind();
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));
            m_vbo.release();

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 4;// number of indexes to render
            glDrawArrays(GL_QUADS, first, count);
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
                // Bottom edge
                std::array<GLfloat, 12> bottom_edge = {
                        highlighted_start, -1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f,
                        highlighted_end, -1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(bottom_edge.data(), bottom_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Top edge
                std::array<GLfloat, 12> top_edge = {
                        highlighted_start, 1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f,
                        highlighted_end, 1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(top_edge.data(), top_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Left edge
                std::array<GLfloat, 12> left_edge = {
                        highlighted_start, -1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f,
                        highlighted_start, 1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f};
                m_vbo.bind();
                m_vbo.allocate(left_edge.data(), left_edge.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);

                // Right edge
                std::array<GLfloat, 12> right_edge = {
                        highlighted_end, -1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f,
                        highlighted_end, 1.0f, highlight_rNorm, highlight_gNorm, highlight_bNorm, 1.0f};
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

    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());

    //auto const min_y = _yMin;
    //auto const max_y = _yMax;

    //float const center_coord = -0.5f * _ySpacing * (static_cast<float>(_analog_series.size() - 1));

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

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
        auto const & data_time = series->getTimeSeries();
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

        float allocated_y_center, allocated_height;
        _plotting_manager->calculateAnalogSeriesAllocation(i, allocated_y_center, allocated_height);

        display_options->allocated_y_center = allocated_y_center;
        display_options->allocated_height = allocated_height;

        // Set the color for the current series
        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;

        m_vertices.clear();

        // Define iterators for the data points in the visible range
        auto start_it = data_time.begin();
        auto end_it = data_time.end();

        // If the time frame is the master time frame, use the time coordinates directly
        // Otherwise, convert the master time coordinates to data time frame indices
        if (time_frame.get() == _master_time_frame.get()) {
            start_it = std::lower_bound(data_time.begin(), data_time.end(), start_time,
                                        [&time_frame](auto const & time, auto const & value) { return time_frame->getTimeAtIndex(TimeFrameIndex(time)) < value; });
            end_it = std::upper_bound(data_time.begin(), data_time.end(), end_time,
                                      [&time_frame](auto const & value, auto const & time) { return value < time_frame->getTimeAtIndex(TimeFrameIndex(time)); });
        } else {
            auto start_idx = time_frame->getIndexAtTime(start_time);                   // index in time vector
            auto end_idx = time_frame->getIndexAtTime(end_time);                       // index in time vector
            start_it = std::lower_bound(data_time.begin(), data_time.end(), TimeFrameIndex(start_idx));// index in data vector
            end_it = std::upper_bound(data_time.begin(), data_time.end(), TimeFrameIndex(end_idx));    // index in data vector

            std::cout << "start_it: " << (*start_it).getValue() << std::endl;
            std::cout << "end_it: " << (*end_it).getValue() << std::endl;

            std::cout << "start_idx: " << start_idx << std::endl;
            std::cout << "end_idx: " << end_idx << std::endl;
        }

        // === MVP MATRIX SETUP ===

        // Apply PlottingManager pan offset
        _plotting_manager->setPanOffset(_verticalPanOffset);

        auto Model = new_getAnalogModelMat(*display_options, display_options->cached_std_dev, display_options->cached_mean, *_plotting_manager);
        auto View = new_getAnalogViewMat(*_plotting_manager);
        auto Projection = new_getAnalogProjectionMat(TimeFrameIndex(start_time), TimeFrameIndex(end_time), _yMin, _yMax, *_plotting_manager);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Handle different gap handling modes

        if (display_options->gap_handling == AnalogGapHandling::AlwaysConnect) {
            // Original behavior: connect all points
            for (auto it = start_it; it != end_it; ++it) {
                size_t const index = std::distance(data_time.begin(), it);
                auto const time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(data_time[index])));
                float const xCanvasPos = time;
                float const yCanvasPos = data[index];
                m_vertices.push_back(xCanvasPos);
                m_vertices.push_back(yCanvasPos);
                m_vertices.push_back(rNorm);
                m_vertices.push_back(gNorm);
                m_vertices.push_back(bNorm);
                m_vertices.push_back(1.0f);// alpha
            }

            m_vbo.bind();
            m_vbo.allocate(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));
            m_vbo.release();

            // Set line thickness from display options
            glLineWidth(static_cast<float>(display_options->line_thickness));
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(m_vertices.size() / 6));

        } else if (display_options->gap_handling == AnalogGapHandling::DetectGaps) {
            // Draw multiple line segments, breaking at gaps
            // Set line thickness before drawing segments
            glLineWidth(static_cast<float>(display_options->line_thickness));
            _drawAnalogSeriesWithGapDetection(start_it, end_it, data, data_time, time_frame,
                                              display_options->gap_threshold, rNorm, gNorm, bNorm);

        } else if (display_options->gap_handling == AnalogGapHandling::ShowMarkers) {
            // Draw individual markers instead of lines
            _drawAnalogSeriesAsMarkers(start_it, end_it, data, data_time, time_frame,
                                       rNorm, gNorm, bNorm);
        }

        i++;
    }

    // Reset line width to default
    glLineWidth(1.0f);
    glUseProgram(0);
}

template<typename Iterator>
void OpenGLWidget::_drawAnalogSeriesWithGapDetection(Iterator start_it, Iterator end_it,
                                                     std::vector<float> const & data,
                                                     std::vector<TimeFrameIndex> const & data_time,
                                                     std::shared_ptr<TimeFrame> const & time_frame,
                                                     float gap_threshold,
                                                     float rNorm, float gNorm, float bNorm) {
    if (start_it == end_it) return;

    std::vector<GLfloat> segment_vertices;
    auto prev_it = start_it;

    for (auto it = start_it; it != end_it; ++it) {
        size_t const index = std::distance(data_time.begin(), it);
        auto const time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(data_time[index])));
        float const xCanvasPos = time;
        float const yCanvasPos = data[index];

        // Check for gap if this isn't the first point
        if (it != start_it) {
            size_t const prev_index = std::distance(data_time.begin(), prev_it);
            auto const prev_time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(data_time[prev_index])));
            //float const time_gap = time - prev_time;
            float const time_gap = index - prev_index;

            if (time_gap > gap_threshold) {
                // Draw current segment if it has points
                if (segment_vertices.size() >= 12) {// At least 2 points (6 floats each)
                    m_vbo.bind();
                    m_vbo.allocate(segment_vertices.data(), static_cast<int>(segment_vertices.size() * sizeof(GLfloat)));
                    m_vbo.release();
                    glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(segment_vertices.size() / 6));
                }

                // Start new segment
                segment_vertices.clear();
            }
        }

        // Add current point to segment
        segment_vertices.push_back(xCanvasPos);
        segment_vertices.push_back(yCanvasPos);
        segment_vertices.push_back(rNorm);
        segment_vertices.push_back(gNorm);
        segment_vertices.push_back(bNorm);
        segment_vertices.push_back(1.0f);// alpha

        prev_it = it;
    }

    // Draw final segment
    if (segment_vertices.size() >= 6) {// At least 1 point
        m_vbo.bind();
        m_vbo.allocate(segment_vertices.data(), static_cast<int>(segment_vertices.size() * sizeof(GLfloat)));
        m_vbo.release();

        if (segment_vertices.size() >= 12) {
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(segment_vertices.size() / 6));
        } else {
            // Single point - draw as a small marker
            glDrawArrays(GL_POINTS, 0, static_cast<int>(segment_vertices.size() / 6));
        }
    }
}

template<typename Iterator>
void OpenGLWidget::_drawAnalogSeriesAsMarkers(Iterator start_it, Iterator end_it,
                                              std::vector<float> const & data,
                                              std::vector<TimeFrameIndex> const & data_time,
                                              std::shared_ptr<TimeFrame> const & time_frame,
                                              float rNorm, float gNorm, float bNorm) {
    m_vertices.clear();

    for (auto it = start_it; it != end_it; ++it) {
        size_t const index = std::distance(data_time.begin(), it);

        // Calculate X position with proper time frame handling
        float xCanvasPos;
        if (time_frame.get() == _master_time_frame.get()) {
            // Same time frame - convert data index to time coordinate
            auto const time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(data_time[index])));
            xCanvasPos = time;
        } else {
            // Different time frames - data_time[index] is already an index in the data's time frame
            // Convert to actual time, which should be compatible with master time frame
            auto const time = static_cast<float>(time_frame->getTimeAtIndex(TimeFrameIndex(data_time[index])));
            xCanvasPos = time;
        }

        float const yCanvasPos = data[index];

        m_vertices.push_back(xCanvasPos);
        m_vertices.push_back(yCanvasPos);
        m_vertices.push_back(rNorm);
        m_vertices.push_back(gNorm);
        m_vertices.push_back(bNorm);
        m_vertices.push_back(1.0f);// alpha
    }

    if (!m_vertices.empty()) {
        m_vbo.bind();
        m_vbo.allocate(m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));
        m_vbo.release();

        // Set point size for better visibility
        //glPointSize(3.0f);
        glDrawArrays(GL_POINTS, 0, static_cast<int>(m_vertices.size() / 6));
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

    static_cast<void>(w);
    static_cast<void>(h);

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

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, m_proj.constData());
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, m_view.constData());
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, m_model.constData());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    // Get axis color from theme
    float r, g, b;
    hexToRGB(m_axis_color, r, g, b);

    // Draw horizontal line at x=0
    std::array<GLfloat, 12> lineVertices = {
            0.0f, _yMin, r, g, b, 1.0f,
            0.0f, _yMax, r, g, b, 1.0f};

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

    // Automatic gap detection and display mode selection
    start_time = std::chrono::high_resolution_clock::now();
    auto gap_analysis = _analyzeDataGaps(*series);
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Gap analysis took " << duration.count() << " milliseconds" << std::endl;
    if (gap_analysis.has_gaps) {
        std::cout << "Gaps detected in " << key << ": " << gap_analysis.gap_count
                  << " gaps, max gap: " << gap_analysis.max_gap_size
                  << ", mean gap: " << gap_analysis.mean_gap_size
                  << " time units. Using gap-aware display." << std::endl;

        display_options->gap_handling = AnalogGapHandling::DetectGaps;
        display_options->enable_gap_detection = true;
        display_options->gap_threshold = gap_analysis.recommended_threshold;
    } else {
        std::cout << "No significant gaps detected in " << key
                  << " (max/mean ratio: " << (gap_analysis.max_gap_size / gap_analysis.mean_gap_size)
                  << "). Using continuous display." << std::endl;

        display_options->gap_handling = AnalogGapHandling::AlwaysConnect;
        display_options->enable_gap_detection = false;
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
        std::shared_ptr<TimeFrame> time_frame,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalEventSeriesDisplayOptions>();

    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_event_series.size()) : color;
    display_options->is_visible = true;

    _digital_event_series[key] = DigitalEventSeriesData{
            std::move(series),
            std::move(time_frame),
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
        std::shared_ptr<TimeFrame> time_frame,
        std::string const & color) {

    auto display_options = std::make_unique<NewDigitalIntervalSeriesDisplayOptions>();

    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_interval_series.size()) : color;
    display_options->is_visible = true;

    _digital_interval_series[key] = DigitalIntervalSeriesData{
            std::move(series),
            std::move(time_frame),
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

    auto const m_dashedProgram_ID = m_dashedProgram->programId();

    glUseProgram(m_dashedProgram_ID);

    glUniformMatrix4fv(m_dashedProjMatrixLoc, 1, GL_FALSE, (m_proj * m_view * m_model).constData());
    std::array<GLfloat, 2> hw = {static_cast<float>(width()), static_cast<float>(height())};
    glUniform2fv(m_dashedResolutionLoc, 1, hw.data());
    glUniform1f(m_dashedDashSizeLoc, params.dashLength);
    glUniform1f(m_dashedGapSizeLoc, params.gapLength);

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);

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
    auto const & time_frame = interval_data.time_frame;

    // Convert time coordinate from master time frame to series time frame
    int64_t query_time_index;
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame - use time coordinate directly
        query_time_index = static_cast<int64_t>(std::round(time_coord));
    } else {
        // Different time frame - convert master time to series time frame index
        query_time_index = static_cast<int64_t>(time_frame->getIndexAtTime(time_coord));
    }

    // Find all intervals that contain this time point in the series' time frame
    auto intervals = series->getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(query_time_index, query_time_index);

    if (!intervals.empty()) {
        // Return the first interval found, converted to master time frame coordinates
        auto const & interval = intervals[0];
        int64_t interval_start_master, interval_end_master;

        if (time_frame.get() == _master_time_frame.get()) {
            // Same time frame - use indices directly as time coordinates
            interval_start_master = interval.start;
            interval_end_master = interval.end;
        } else {
            // Convert series indices to master time frame coordinates
            interval_start_master = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(interval.start)));
            interval_end_master = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(interval.end)));
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
    auto const & time_frame = it->second.time_frame;

    // Convert master time coordinate to series time frame index
    int64_t current_time_series_index;
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame - use time coordinate directly
        current_time_series_index = static_cast<int64_t>(std::round(current_time_master));
    } else {
        // Different time frame - convert master time to series time frame index
        current_time_series_index = static_cast<int64_t>(time_frame->getIndexAtTime(current_time_master));
    }

    // Convert original interval bounds to series time frame for constraints
    int64_t original_start_series, original_end_series;
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame
        original_start_series = _original_start_time;
        original_end_series = _original_end_time;
    } else {
        // Convert master time coordinates to series time frame indices
        original_start_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_original_start_time)));
        original_end_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_original_end_time)));
    }

    // Perform dragging logic in series time frame
    int64_t new_start_series, new_end_series;

    if (_dragging_left_edge) {
        // Dragging left edge - constrain to not pass right edge
        new_start_series = std::min(current_time_series_index, original_end_series - 1);
        new_end_series = original_end_series;

        // Check for collision with other intervals in series time frame
        auto overlapping_intervals = series->getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
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
        auto overlapping_intervals = series->getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
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
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame
        _dragged_start_time = new_start_series;
        _dragged_end_time = new_end_series;
    } else {
        // Convert series indices back to master time coordinates
        try {
            _dragged_start_time = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(new_start_series)));
            _dragged_end_time = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(new_end_series)));
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
    auto const & time_frame = it->second.time_frame;

    try {
        // Convert all coordinates to series time frame for data operations
        int64_t original_start_series, original_end_series, new_start_series, new_end_series;

        if (time_frame.get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            original_start_series = _original_start_time;
            original_end_series = _original_end_time;
            new_start_series = _dragged_start_time;
            new_end_series = _dragged_end_time;
        } else {
            // Convert master time coordinates to series time frame indices
            original_start_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_original_start_time)));
            original_end_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_original_end_time)));
            new_start_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_dragged_start_time)));
            new_end_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_dragged_end_time)));
        }

        // Validate converted coordinates
        if (new_start_series >= new_end_series ||
            new_start_series < 0 || new_end_series < 0) {
            throw std::runtime_error("Invalid interval bounds after conversion");
        }

        // Update the interval data in the series' native time frame
        // First, remove the original interval completely
        for (int64_t time = original_start_series; time <= original_end_series; ++time) {
            series->setEventAtTime(static_cast<int>(time), false);
        }

        // Add the new interval
        series->addEvent(static_cast<int>(new_start_series), static_cast<int>(new_end_series));

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

    auto const m_program_ID = m_program->programId();
    glUseProgram(m_program_ID);

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

    std::array<GLfloat, 24> original_vertices = {
            original_start, min_y, rNorm, gNorm, bNorm, 0.2f,
            original_end, min_y, rNorm, gNorm, bNorm, 0.2f,
            original_end, max_y, rNorm, gNorm, bNorm, 0.2f,
            original_start, max_y, rNorm, gNorm, bNorm, 0.2f};

    m_vbo.bind();
    m_vbo.allocate(original_vertices.data(), original_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_QUADS, 0, 4);

    // Draw the dragged interval semi-transparent (alpha = 0.8)
    std::array<GLfloat, 24> dragged_vertices = {
            dragged_start, min_y, rNorm, gNorm, bNorm, 0.8f,
            dragged_end, min_y, rNorm, gNorm, bNorm, 0.8f,
            dragged_end, max_y, rNorm, gNorm, bNorm, 0.8f,
            dragged_start, max_y, rNorm, gNorm, bNorm, 0.8f};

    m_vbo.bind();
    m_vbo.allocate(dragged_vertices.data(), dragged_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_QUADS, 0, 4);

    glUseProgram(0);
}

OpenGLWidget::GapAnalysis OpenGLWidget::_analyzeDataGaps(AnalogTimeSeries const & series) {
    GapAnalysis analysis;

    auto const & time_indices = series.getTimeSeries();
    if (time_indices.size() < 2) {
        // Not enough data points to analyze gaps
        return analysis;
    }

    // Fast gap analysis using running statistics - no storage or sorting required
    size_t max_gap = 0;
    size_t sum_gaps = 0;
    int gap_count = 0;

    // Calculate gaps between consecutive time indices directly
    for (size_t i = 1; i < time_indices.size(); ++i) {

        auto const gap_size = time_indices[i] - time_indices[i - 1];

        sum_gaps += gap_size.getValue();
        gap_count++;

        if (gap_size.getValue() > max_gap) {
            max_gap = gap_size.getValue();
        }
    }

    size_t const mean_gap = sum_gaps / gap_count;// rounded to nearest int

    // Simple heuristic: if the largest gap is significantly bigger than the mean gap,
    // we likely have real gaps in the data (not just regular sampling intervals)
    constexpr size_t GAP_MULTIPLIER = 3;// Largest gap must be 3x mean to be considered significant
    bool const has_significant_gaps = (max_gap > mean_gap * GAP_MULTIPLIER);

    // Count gaps that are larger than the threshold
    int significant_gap_count = 0;
    if (has_significant_gaps) {
        size_t const gap_threshold = mean_gap * GAP_MULTIPLIER;

        // Single pass to count significant gaps
        for (size_t i = 1; i < time_indices.size(); ++i) {
            auto const gap_size = time_indices[i] - time_indices[i - 1];
            if (gap_size.getValue() > gap_threshold) {
                significant_gap_count++;
            }
        }
    }

    // Set analysis results
    analysis.has_gaps = has_significant_gaps;
    analysis.gap_count = significant_gap_count;
    analysis.max_gap_size = max_gap;
    analysis.mean_gap_size = mean_gap;

    // Set recommended threshold slightly below the detection threshold
    if (has_significant_gaps) {
        analysis.recommended_threshold = mean_gap * GAP_MULTIPLIER * 0.8f;// 80% of threshold
    } else {
        analysis.recommended_threshold = mean_gap * 2.0f;// Conservative default
    }

    return analysis;
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
        for (auto const & [series_key, data] : _digital_interval_series) {
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
    auto const & time_frame = it->second.time_frame;

    // Convert coordinates to series time frame for collision detection
    int64_t click_time_series, current_time_series;
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame - use coordinates directly
        click_time_series = _new_interval_click_time;
        current_time_series = current_time_coord;
    } else {
        // Convert master time coordinates to series time frame indices
        click_time_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_new_interval_click_time)));
        current_time_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(current_time_coord)));
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
    auto overlapping_intervals = series->getIntervalsAsVector<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
            new_start_series, new_end_series);

    // If there are overlapping intervals, constrain the new interval
    for (auto const & interval : overlapping_intervals) {
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
    if (time_frame.get() == _master_time_frame.get()) {
        // Same time frame
        _new_interval_start_time = new_start_series;
        _new_interval_end_time = new_end_series;
    } else {
        // Convert series indices back to master time coordinates
        try {
            _new_interval_start_time = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(new_start_series)));
            _new_interval_end_time = static_cast<int64_t>(time_frame->getTimeAtIndex(TimeFrameIndex(new_end_series)));
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
    auto const & time_frame = it->second.time_frame;

    try {
        // Convert coordinates to series time frame for data operations
        int64_t new_start_series, new_end_series;

        if (time_frame.get() == _master_time_frame.get()) {
            // Same time frame - use coordinates directly
            new_start_series = _new_interval_start_time;
            new_end_series = _new_interval_end_time;
        } else {
            // Convert master time coordinates to series time frame indices
            new_start_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_new_interval_start_time)));
            new_end_series = static_cast<int64_t>(time_frame->getIndexAtTime(static_cast<float>(_new_interval_end_time)));
        }

        // Validate converted coordinates
        if (new_start_series >= new_end_series || new_start_series < 0 || new_end_series < 0) {
            throw std::runtime_error("Invalid interval bounds after conversion");
        }

        // Add the new interval to the series
        series->addEvent(static_cast<int>(new_start_series), static_cast<int>(new_end_series));

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

    auto const m_program_ID = m_program->programId();
    glUseProgram(m_program_ID);

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

    // Clip the new interval to visible range
    float const new_start = std::max(static_cast<float>(_new_interval_start_time), start_time);
    float const new_end = std::min(static_cast<float>(_new_interval_end_time), end_time);

    // Draw the new interval being created with 50% transparency
    std::array<GLfloat, 24> new_interval_vertices = {
            new_start, min_y, rNorm, gNorm, bNorm, 0.5f,
            new_end, min_y, rNorm, gNorm, bNorm, 0.5f,
            new_end, max_y, rNorm, gNorm, bNorm, 0.5f,
            new_start, max_y, rNorm, gNorm, bNorm, 0.5f};

    m_vbo.bind();
    m_vbo.allocate(new_interval_vertices.data(), new_interval_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_QUADS, 0, 4);

    glUseProgram(0);
}
