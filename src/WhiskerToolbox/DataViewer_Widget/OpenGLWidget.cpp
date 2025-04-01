#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataViewer_Widget.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame.hpp"
#include "shaders/colored_vertex_shader.hpp"
#include "shaders/dashed_line_shader.hpp"
#include "utils/color.hpp"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShader>
#include <QPainter>
#include <glm/glm.hpp>

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
}

OpenGLWidget::~OpenGLWidget() {
    cleanup();
}

void OpenGLWidget::updateCanvas(int time) {
    _time = time;
    //std::cout << "Redrawing at " << _time << std::endl;
    update();
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

    setupVertexAttribs();

    glUseProgram(m_program_ID);

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, m_proj.constData());
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, m_view.constData());
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, m_model.constData());

    for (auto const & [key, event_data]: _digital_event_series) {
        auto const & series = event_data.series;
        auto const & time_frame = event_data.time_frame;
        hexToRGB(event_data.color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = 1.0f;

        // Get events in the visible range using the time transform
        auto visible_events = series->getEventsInRange(
                start_time, end_time,
                [&time_frame](float idx) {
                    return static_cast<float>(time_frame->getTimeAtIndex(static_cast<int>(idx)));
                });

        for (auto const & event: visible_events) {
            auto const time = static_cast<float>(time_frame->getTimeAtIndex(static_cast<int>(event)));
            float const xCanvasPos = static_cast<GLfloat>(time - start_time) / (end_time - start_time) * 2.0f - 1.0f;

            std::array<GLfloat, 12> vertices = {
                    xCanvasPos, -1.0f, rNorm, gNorm, bNorm, alpha,
                    xCanvasPos, 1.0f, rNorm, gNorm, bNorm, alpha};

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

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, m_proj.constData());
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, m_view.constData());
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, m_model.constData());

    //QOpenGLFunctions_4_1_Core::glBindVertexArray(m_vao.objectId());
    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);// glBindVertexArray
    setupVertexAttribs();

    for (auto const & [key, interval_data]: _digital_interval_series) {
        auto const & series = interval_data.series;
        auto const & time_frame = interval_data.time_frame;

        // Get only the intervals that overlap with the visible range
        auto visible_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
                static_cast<int64_t>(start_time),
                static_cast<int64_t>(end_time),
                [&time_frame](int64_t idx) {
                    return static_cast<float>(time_frame->getTimeAtIndex(idx));
                });

        hexToRGB(interval_data.color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;
        float const alpha = 0.5f;// Set alpha for shading

        for (auto const & interval: visible_intervals) {

            auto start = static_cast<float>(time_frame->getTimeAtIndex(interval.start));
            auto end = static_cast<float>(time_frame->getTimeAtIndex(interval.end));

            //Clip the interval to the visible range
            start = std::max(start, start_time);
            end = std::min(end, end_time);

            float const xStart = static_cast<GLfloat>(start - start_time) / (end_time - start_time) * 2.0f - 1.0f;
            float const xEnd = static_cast<GLfloat>(end - start_time) / (end_time - start_time) * 2.0f - 1.0f;

            std::array<GLfloat, 24> vertices = {
                    xStart, -1.0f, rNorm, gNorm, bNorm, alpha,
                    xEnd, -1.0f, rNorm, gNorm, bNorm, alpha,
                    xEnd, 1.0f, rNorm, gNorm, bNorm, alpha,
                    xStart, 1.0f, rNorm, gNorm, bNorm, alpha};

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

    auto const m_program_ID = m_program->programId();

    glUseProgram(m_program_ID);

    glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, m_proj.constData());
    glUniformMatrix4fv(m_viewMatrixLoc, 1, GL_FALSE, m_view.constData());
    glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, m_model.constData());

    QOpenGLVertexArrayObject::Binder const vaoBinder(&m_vao);
    setupVertexAttribs();

    for (auto const & [key, analog_data]: _analog_series) {
        auto const & series = analog_data.series;
        auto const & data = series->getAnalogTimeSeries();
        auto const & data_time = series->getTimeSeries();
        auto const & time_frame = analog_data.time_frame;
        auto const stdDev = analog_data.scaleFactor;

        // Set the color for the current series
        hexToRGB(analog_data.color, r, g, b);
        float const rNorm = static_cast<float>(r) / 255.0f;
        float const gNorm = static_cast<float>(g) / 255.0f;
        float const bNorm = static_cast<float>(b) / 255.0f;

        m_vertices.clear();

        auto start_it = std::lower_bound(data_time.begin(), data_time.end(), start_time,
                                         [&time_frame](auto const & time, auto const & value) { return time_frame->getTimeAtIndex(time) < value; });
        auto end_it = std::upper_bound(data_time.begin(), data_time.end(), end_time,
                                       [&time_frame](auto const & value, auto const & time) { return value < time_frame->getTimeAtIndex(time); });

        for (auto it = start_it; it != end_it; ++it) {
            size_t const index = std::distance(data_time.begin(), it);
            auto const time = static_cast<float>(time_frame->getTimeAtIndex(data_time[index]));
            float const xCanvasPos = static_cast<GLfloat>(time - start_time) / (end_time - start_time) * 2.0f - 1.0f;
            float const yCanvasPos = data[index] / stdDev;
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
    }

    glUseProgram(0);
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

    // Draw the series
    drawDigitalEventSeries();
    drawDigitalIntervalSeries();
    drawAnalogSeries();

    drawAxis();

    drawGridLines();
}

void OpenGLWidget::resizeGL(int w, int h) {
    m_proj.setToIdentity();
    m_proj.perspective(45.0f, GLfloat(w) / GLfloat(h), 0.01f, 100.0f);
    //m_proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Use orthographic projection for 2D plotting
    m_view.setToIdentity();
    m_view.translate(0, 0, -2);
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
            0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

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

    std::string const seriesColor = color.empty() ? "#FFFFFF" : color;

    float const stdDev = series->getStdDevValue();

    _analog_series[key] =
            AnalogSeriesData{std::move(series),
                             seriesColor,
                             std::move(time_frame),
                             stdDev * 5.0f};
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

    std::string const seriesColor = color.empty() ? generateRandomColor() : color;
    _digital_event_series[key] = DigitalEventSeriesData{
            std::move(series),
            seriesColor,
            std::move(time_frame)};
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

    std::string const seriesColor = color.empty() ? generateRandomColor() : color;
    _digital_interval_series[key] = DigitalIntervalSeriesData{
            std::move(series),
            seriesColor,
            std::move(time_frame)};
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

/*
void OpenGLWidget::generateRandomValues(int count) {
    m_vertices.clear();
    for (int i = 0; i < count; ++i) {
        m_vertices.push_back(static_cast<GLfloat>(i) / (count - 1) * 2.0f - 1.0f);       // X coordinate
        m_vertices.push_back(static_cast<GLfloat>(std::rand()) / RAND_MAX * 2.0f - 1.0f);// Y coordinate
    }

    m_vbo.bind();
    m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(GLfloat));
    m_vbo.release();
}

void OpenGLWidget::generateAndAddFakeData(int count) {
    std::vector<float> data(count);
    for (int i = 0; i < count; ++i) {
        data[i] = static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f;
    }
    auto series = std::make_shared<AnalogTimeSeries>(data);
    //addAnalogTimeSeries(series);
}

void OpenGLWidget::adjustFakeData() {
    std::vector<float> new_series;
    for (size_t i = 0; i < _analog_series[0].series->getAnalogTimeSeries().size(); i++) {
        new_series.push_back(static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
    }
    _analog_series[0].series->setData(new_series);
}
*/
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
    startLine.yStart = -1.0f;
    startLine.yEnd = 1.0f;
    drawDashedLine(startLine);

    LineParameters endLine;
    endLine.xStart = xEndCanvasPos;
    endLine.xEnd = xEndCanvasPos;
    endLine.yStart = -1.0f;
    endLine.yEnd = 1.0f;
    drawDashedLine(endLine);
}
