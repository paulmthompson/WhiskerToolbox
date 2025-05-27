#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataViewer_Widget.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"
#include "TimeFrame.hpp"
#include "shaders/colored_vertex_shader.hpp"
#include "shaders/dashed_line_shader.hpp"
#include "utils/color.hpp"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtx/transform.hpp>

#include <cstdlib>
#include <ctime>
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
    setMouseTracking(true); // Enable mouse tracking for hover events
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
        _isPanning = true;
        _lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent * event) {
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
        for (auto const & [key, data] : _analog_series) {
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
        _isPanning = false;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLWidget::setBackgroundColor(std::string const & hexColor) {
    m_background_color = hexColor;
    update();
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

/**
 * @brief OpenGLWidget::drawDigitalEventSeries
 *
 * Each event is specified by a single time point.
 * We can find which of the time points are within the visible time frame
 * After those are found, we will draw a vertical line at that time point
 */
void OpenGLWidget::drawDigitalEventSeries() {
    int r, g, b;
    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());
    auto const m_program_ID = m_program->programId();

    auto const min_y = _yMin;
    auto const max_y = _yMax;

    glUseProgram(m_program_ID);

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

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

        // Get events in the visible range using the time transform
        auto visible_events = series->getEventsInRange(
                start_time, end_time,
                [&time_frame](float idx) {
                    return static_cast<float>(time_frame->getTimeAtIndex(static_cast<int>(idx)));
                });

        // Model Matrix. Scale series. Vertical Offset based on display order and offset increment
        auto Model = glm::mat4(1.0f);

        // View Matrix. Panning (all lines moved together).
        auto View = glm::mat4(1.0f);
        View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0));

        // Projection Matrix. Orthographic. Horizontal Zoom. Vertical Zoom.
        auto Projection = glm::ortho(start_time, end_time,
                                     min_y, max_y);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        for (auto const & event: visible_events) {
            auto const xCanvasPos = static_cast<float>(time_frame->getTimeAtIndex(static_cast<int>(event)));

            std::array<GLfloat, 12> vertices = {
                    xCanvasPos, min_y, rNorm, gNorm, bNorm, alpha,
                    xCanvasPos, max_y, rNorm, gNorm, bNorm, alpha};

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 2;// number of indexes to render
            glDrawArrays(GL_LINES, first, count);
        }
    }

    glUseProgram(0);
}

void OpenGLWidget::drawDigitalIntervalSeries() {
    int r, g, b;
    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());

    auto const min_y = _yMin;
    auto const max_y = _yMax;

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

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
                    return static_cast<float>(time_frame->getTimeAtIndex(idx));
                });

        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = display_options->alpha;

        // Model Matrix. Scale series. Vertical Offset based on display order and offset increment
        auto Model = glm::mat4(1.0f);

        // View Matrix. Panning (all lines moved together).
        auto View = glm::mat4(1.0f);
        //View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0));

        // Projection Matrix. Orthographic. Horizontal Zoom. Vertical Zoom.
        auto Projection = glm::ortho(start_time, end_time,
                                     min_y, max_y);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        for (auto const & interval: visible_intervals) {

            auto start = static_cast<float>(time_frame->getTimeAtIndex(interval.start));
            auto end = static_cast<float>(time_frame->getTimeAtIndex(interval.end));

            //Clip the interval to the visible range
            start = std::max(start, start_time);
            end = std::min(end, end_time);

            float const xStart = start;
            float const xEnd = end;

            std::array<GLfloat, 24> vertices = {
                    xStart, min_y, rNorm, gNorm, bNorm, alpha,
                    xEnd, min_y, rNorm, gNorm, bNorm, alpha,
                    xEnd, max_y, rNorm, gNorm, bNorm, alpha,
                    xStart, max_y, rNorm, gNorm, bNorm, alpha};

            //glBindBuffer(GL_ARRAY_BUFFER, m_vbo.bufferId());
            m_vbo.bind();
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));// What is this?
            m_vbo.release();

            GLint const first = 0;  // Starting index of enabled array
            GLsizei const count = 4;// number of indexes to render
            glDrawArrays(GL_QUADS, first, count);
        }
    }

    glUseProgram(0);
}

void OpenGLWidget::drawAnalogSeries() {
    int r, g, b;

    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());

    auto const min_y = _yMin;
    auto const max_y = _yMax;

    float const center_coord = -0.5f * _ySpacing * (static_cast<float>(_analog_series.size() - 1));

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    int i = 0;

    for (auto const & [key, analog_data]: _analog_series) {
        auto const & series = analog_data.series;
        auto const & data = series->getAnalogTimeSeries();
        auto const & data_time = series->getTimeSeries();
        auto const & time_frame = analog_data.time_frame;
        auto const & display_options = analog_data.display_options;
        
        if (!display_options->is_visible) continue;
        
        auto const stdDev = display_options->scale_factor * display_options->user_scale_factor / _global_zoom;

        // Set the color for the current series
        hexToRGB(display_options->hex_color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;

        m_vertices.clear();

        auto start_it = std::lower_bound(data_time.begin(), data_time.end(), start_time,
                                         [&time_frame](auto const & time, auto const & value) { return time_frame->getTimeAtIndex(time) < value; });
        auto end_it = std::upper_bound(data_time.begin(), data_time.end(), end_time,
                                       [&time_frame](auto const & value, auto const & time) { return value < time_frame->getTimeAtIndex(time); });

        // Model Matrix. Scale series. Vertical Offset based on display order and offset increment
        float const y_offset = static_cast<float>(i) * _ySpacing;
        auto Model = glm::mat4(1.0f);
        Model = glm::translate(Model, glm::vec3(0, y_offset, 0));

        //Now move so that center of all series is at zero
        Model = glm::translate(Model, glm::vec3(0, center_coord, 0));
        Model = glm::scale(Model, glm::vec3(1, 1 / stdDev, 1));

        // View Matrix. Panning (all lines moved together).
        auto View = glm::mat4(1.0f);
        View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0));

        // Projection Matrix. Orthographic. Horizontal Zoom. Vertical Zoom.
        auto Projection = glm::ortho(start_time, end_time,
                                     min_y, max_y);

        glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, &Projection[0][0]);
        glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, &View[0][0]);
        glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, &Model[0][0]);

        // Handle different gap handling modes
        if (display_options->gap_handling == AnalogGapHandling::AlwaysConnect) {
            // Original behavior: connect all points
            for (auto it = start_it; it != end_it; ++it) {
                size_t const index = std::distance(data_time.begin(), it);
                auto const time = static_cast<float>(time_frame->getTimeAtIndex(data_time[index]));
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

            glDrawArrays(GL_LINE_STRIP, 0, static_cast<int>(m_vertices.size() / 6));
            
        } else if (display_options->gap_handling == AnalogGapHandling::DetectGaps) {
            // Draw multiple line segments, breaking at gaps
            _drawAnalogSeriesWithGapDetection(start_it, end_it, data, data_time, time_frame, 
                                            display_options->gap_threshold, rNorm, gNorm, bNorm);
            
        } else if (display_options->gap_handling == AnalogGapHandling::ShowMarkers) {
            // Draw individual markers instead of lines
            _drawAnalogSeriesAsMarkers(start_it, end_it, data, data_time, time_frame, 
                                     rNorm, gNorm, bNorm);
        }

        i++;
    }

    glUseProgram(0);
}

template<typename Iterator>
void OpenGLWidget::_drawAnalogSeriesWithGapDetection(Iterator start_it, Iterator end_it,
                                                     std::vector<float> const & data,
                                                     std::vector<size_t> const & data_time,
                                                     std::shared_ptr<TimeFrame> const & time_frame,
                                                     float gap_threshold,
                                                     float rNorm, float gNorm, float bNorm) {
    if (start_it == end_it) return;
    
    std::vector<GLfloat> segment_vertices;
    auto prev_it = start_it;
    
    for (auto it = start_it; it != end_it; ++it) {
        size_t const index = std::distance(data_time.begin(), it);
        auto const time = static_cast<float>(time_frame->getTimeAtIndex(data_time[index]));
        float const xCanvasPos = time;
        float const yCanvasPos = data[index];
        
        // Check for gap if this isn't the first point
        if (it != start_it) {
            size_t const prev_index = std::distance(data_time.begin(), prev_it);
            auto const prev_time = static_cast<float>(time_frame->getTimeAtIndex(data_time[prev_index]));
            float const time_gap = time - prev_time;
            
            if (time_gap > gap_threshold) {
                // Draw current segment if it has points
                if (segment_vertices.size() >= 12) { // At least 2 points (6 floats each)
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
    if (segment_vertices.size() >= 6) { // At least 1 point
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
                                              std::vector<size_t> const & data_time,
                                              std::shared_ptr<TimeFrame> const & time_frame,
                                              float rNorm, float gNorm, float bNorm) {
    m_vertices.clear();
    
    for (auto it = start_it; it != end_it; ++it) {
        size_t const index = std::distance(data_time.begin(), it);
        auto const time = static_cast<float>(time_frame->getTimeAtIndex(data_time[index]));
        float const xCanvasPos = time;
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
        
        // Enable point size for better visibility
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDrawArrays(GL_POINTS, 0, static_cast<int>(m_vertices.size() / 6));
        glDisable(GL_PROGRAM_POINT_SIZE);
    }
}

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
}

void OpenGLWidget::resizeGL(int w, int h) {
    // Store the new dimensions
    // Note: width() and height() will return the new values after this call
    
    // For 2D plotting, we should use orthographic projection
    m_proj.setToIdentity();
    m_proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Use orthographic projection for 2D plotting
    
    m_view.setToIdentity();
    m_view.translate(0, 0, -1); // Move slightly back for orthographic view
    
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

    // Draw horizontal line at x=0
    std::array<GLfloat, 12> lineVertices = {
            0.0f, _yMin, 1.0f, 1.0f, 1.0f, 1.0f,
            0.0f, _yMax, 1.0f, 1.0f, 1.0f, 1.0f};

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

    auto display_options = std::make_unique<AnalogTimeSeriesDisplayOptions>();
    
    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_analog_series.size()) : color;
    display_options->is_visible = true;
    
    // Calculate scale factor based on standard deviation
    float const stdDev = calculate_std_dev(*series.get());
    display_options->scale_factor = stdDev * 5.0f;
    display_options->user_scale_factor = 1.0f; // Default user scale
    
    std::cout << "Standard deviation for "
              << key
              << " calculated as "
              << stdDev
              << std::endl;
              
    _analog_series[key] = AnalogSeriesData{
        std::move(series),
        std::move(time_frame),
        std::move(display_options)
    };
    
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

    auto display_options = std::make_unique<DigitalEventSeriesDisplayOptions>();
    
    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_event_series.size()) : color;
    display_options->is_visible = true;
    
    _digital_event_series[key] = DigitalEventSeriesData{
        std::move(series),
        std::move(time_frame),
        std::move(display_options)
    };
    
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

    auto display_options = std::make_unique<DigitalIntervalSeriesDisplayOptions>();
    
    // Set color
    display_options->hex_color = color.empty() ? TimeSeriesDefaultValues::getColorForIndex(_digital_interval_series.size()) : color;
    display_options->is_visible = true;
    
    _digital_interval_series[key] = DigitalIntervalSeriesData{
        std::move(series),
        std::move(time_frame),
        std::move(display_options)
    };
    
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
    auto item = _series_y_position.find(key);
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
    // Draw dashed vertical lines at edge of canvas
    auto const start_time = _xAxis.getStart();
    auto const end_time = _xAxis.getEnd();

    auto const xCanvasWidth = static_cast<float>(end_time - start_time);
    auto const xCanvasStart = static_cast<float>(start_time - start_time);

    float const xStartCanvasPos = xCanvasStart / xCanvasWidth * 2.0f - 1.0f;
    float const xEndCanvasPos = xCanvasWidth / xCanvasWidth * 2.0f - 1.0f;

    LineParameters startLine;
    startLine.xStart = xStartCanvasPos;
    startLine.xEnd = xStartCanvasPos;
    startLine.yStart = _yMin;
    startLine.yEnd = _yMax;
    drawDashedLine(startLine);

    LineParameters endLine;
    endLine.xStart = xEndCanvasPos;
    endLine.xEnd = xEndCanvasPos;
    endLine.yStart = _yMin;
    endLine.yEnd = _yMax;
    drawDashedLine(endLine);
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
    float const normalized_x = canvas_x / canvas_width; // 0.0 to 1.0
    
    auto const start_time = static_cast<float>(_xAxis.getStart());
    auto const end_time = static_cast<float>(_xAxis.getEnd());
    
    return start_time + normalized_x * (end_time - start_time);
}

float OpenGLWidget::canvasYToAnalogValue(float canvas_y, std::string const & series_key) const {
    auto it = _analog_series.find(series_key);
    if (it == _analog_series.end()) {
        return 0.0f;
    }
    
    auto const & analog_data = it->second;
    auto const & display_options = analog_data.display_options;
    
    // Convert canvas Y coordinate to normalized device coordinate
    float const canvas_height = static_cast<float>(height());
    float const normalized_y = 1.0f - (canvas_y / canvas_height); // Flip Y axis (0.0 at bottom, 1.0 at top)
    
    // Convert from normalized device coordinates to view coordinates
    float const view_y = _yMin + normalized_y * (_yMax - _yMin);
    
    // Account for vertical panning
    float const adjusted_y = view_y - _verticalPanOffset;
    
    // Account for series-specific transformations
    // Find the series index for Y offset calculation
    int series_index = 0;
    for (auto const & [key, data] : _analog_series) {
        if (key == series_key) break;
        if (data.display_options->is_visible) series_index++;
    }
    
    // Calculate the center coordinate for all series
    float const center_coord = -0.5f * _ySpacing * (static_cast<float>(_analog_series.size() - 1));
    float const series_y_offset = static_cast<float>(series_index) * _ySpacing + center_coord;
    
    // Remove the series offset
    float const series_relative_y = adjusted_y - series_y_offset;
    
    // Convert from scaled coordinates back to original analog values
    auto const scale_factor = display_options->scale_factor * display_options->user_scale_factor / _global_zoom;
    float const analog_value = series_relative_y * scale_factor;
    
    return analog_value;
}
