#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataViewer_Widget.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame.hpp"
#include "utils/color.hpp"

#include <QOpenGLShader>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>

#include <cstdlib>
#include <ctime>


OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    std::srand(std::time(nullptr));
}

OpenGLWidget::~OpenGLWidget() {
    cleanup();
}

void OpenGLWidget::updateCanvas(int time)
{
    _time = time;
    //std::cout << "Redrawing at " << _time << std::endl;
    update();
}

void OpenGLWidget::setBackgroundColor(const std::string &hexColor)
{
    m_background_color = hexColor;
    update();
}

static const char *vertexShaderSource =
    "#version 150\n"
    "in vec4 vertex;\n"
    "in vec3 color;\n"
    "in float alpha;\n"
    "out vec3 fragColor;\n"
    "out float fragAlpha;\n"
    "uniform mat4 projMatrix;\n"
    "uniform mat4 viewMatrix;\n"
    "uniform mat4 modelMatrix;\n"
    "void main() {\n"
    "   gl_Position = projMatrix * viewMatrix * modelMatrix * vertex;\n"
    "   fragColor = color;\n"
    "   fragAlpha = alpha;\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 150\n"
    "in vec3 fragColor;\n"
    "in float fragAlpha;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "   outColor = vec4(fragColor, fragAlpha);\n"
    "}\n";


void OpenGLWidget::cleanup()
{
    if (m_program == nullptr)
        return;
    makeCurrent();
    delete m_program;
    m_program = 0;
    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &OpenGLWidget::cleanup);

    initializeOpenGLFunctions();
    int r, g, b;
    hexToRGB(m_background_color, r, g, b);
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->link();
    m_program->bind();

    m_projMatrixLoc = m_program->uniformLocation("projMatrix");
    m_viewMatrixLoc = m_program->uniformLocation("viewMatrix");
    m_modelMatrixLoc = m_program->uniformLocation("modelMatrix");

    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    /*
    GLfloat vertices[] = {
        -0.5f, 0.0f,
        0.5f, 0.0f
    };
    m_vbo.allocate(vertices, sizeof(vertices));
    */
    //generateRandomValues(100);
    //generateAndAddFakeData(100000);

    setupVertexAttribs();

    m_program->release();

}

void OpenGLWidget::setupVertexAttribs() {
    m_vbo.bind();
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glEnableVertexAttribArray(0);
    const int vertex_argument_num = 6;
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), nullptr);

    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), reinterpret_cast<void*>(2 * sizeof(GLfloat)));

    f->glEnableVertexAttribArray(2);
    f->glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, vertex_argument_num * sizeof(GLfloat), reinterpret_cast<void*>(5 * sizeof(GLfloat)));

    m_vbo.release();
}

void OpenGLWidget::drawDigitalEventSeries()
{
    int r, g, b;
    const auto start_time = _xAxis.getStart();
    const auto end_time = _xAxis.getEnd();

    for (const auto& [key, event_data] : _digital_event_series) {
        const auto& series = event_data.series;
        const auto& events = series->getEventSeries();
        const auto& time_frame = event_data.time_frame;
        hexToRGB(event_data.color, r, g, b);
        float rNorm = r / 255.0f;
        float gNorm = g / 255.0f;
        float bNorm = b / 255.0f;
        float alpha = 1.0f;

        for (const auto& event : events) {
            float time = time_frame->getTimeAtIndex(event);
            if (time >= start_time && time <= end_time) {
                float xCanvasPos = static_cast<GLfloat>(time - start_time) / (end_time - start_time) * 2.0f - 1.0f;

                std::array<GLfloat, 6*2> vertices = {
                        xCanvasPos, -1.0f, rNorm, gNorm, bNorm, alpha,
                        xCanvasPos, 1.0f, rNorm, gNorm, bNorm, alpha
                };
                m_vbo.bind();
                m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));
                m_vbo.release();
                glDrawArrays(GL_LINES, 0, 2);
            }
        }
    }
}

void OpenGLWidget::drawDigitalIntervalSeries()
{
    int r, g, b;
    const auto start_time = _xAxis.getStart();
    const auto end_time = _xAxis.getEnd();

    for (const auto& [key, interval_data] : _digital_interval_series) {
        const auto& series = interval_data.series;
        const auto& intervals = series->getDigitalIntervalSeries();
        const auto& time_frame = interval_data.time_frame;
        hexToRGB(interval_data.color, r, g, b);
        float rNorm = r / 255.0f;
        float gNorm = g / 255.0f;
        float bNorm = b / 255.0f;
        float alpha = 0.5f; // Set alpha for shading

        for (const auto& interval : intervals) {

            float start = time_frame->getTimeAtIndex(interval.first);
            float end = time_frame->getTimeAtIndex(interval.second);
            if (end < start_time || start > end_time) {
                continue;
            }

            start = std::max(start, static_cast<float>(start_time));
            end = std::min(end, static_cast<float>(end_time));

            float xStart = static_cast<GLfloat>(start - start_time) / (end_time - start_time) * 2.0f - 1.0f;
            float xEnd = static_cast<GLfloat>(end - start_time) / (end_time - start_time) * 2.0f - 1.0f;

            std::array<GLfloat, 6*4> vertices = {
                    xStart, -1.0f, rNorm, gNorm, bNorm, alpha,
                    xEnd, -1.0f, rNorm, gNorm, bNorm, alpha,
                    xEnd, 1.0f, rNorm, gNorm, bNorm, alpha,
                    xStart, 1.0f, rNorm, gNorm, bNorm, alpha
            };

            m_vbo.bind();
            m_vbo.allocate(vertices.data(), vertices.size() * sizeof(GLfloat));
            m_vbo.release();
            glDrawArrays(GL_QUADS, 0, 4);
        }
    }
}

void OpenGLWidget::drawAnalogSeries()
{
    int r, g, b;

    const auto start_time = _xAxis.getStart();
    const auto end_time = _xAxis.getEnd();

    for (const auto&  [key, analog_data] : _analog_series) {
        const auto &series = analog_data.series;
        const auto &data = series->getAnalogTimeSeries();
        const auto &data_time = series->getTimeSeries();
        const auto &time_frame = analog_data.time_frame;

        // Set the color for the current series
        hexToRGB(analog_data.color, r, g, b);
        float rNorm = r / 255.0f;
        float gNorm = g / 255.0f;
        float bNorm = b / 255.0f;

        m_vertices.clear();

        auto start_it = std::lower_bound(data_time.begin(), data_time.end(), start_time,
                                         [&time_frame](const auto& time, const auto& value) { return time_frame->getTimeAtIndex(time) < value; });
        auto end_it = std::upper_bound(data_time.begin(), data_time.end(), end_time,
                                       [&time_frame](const auto& value, const auto& time) { return value < time_frame->getTimeAtIndex(time); });

        float maxY = series->getMaxValue(start_it - data_time.begin(), end_it - data_time.begin());
        float minY = series->getMinValue(start_it - data_time.begin(), end_it - data_time.begin());

        float absMaxY = std::max(std::abs(maxY), std::abs(minY));
        if (absMaxY == 0) {
            absMaxY = 1.0f;
        }

        for (auto it = start_it; it != end_it; ++it) {
            size_t index = std::distance(data_time.begin(), it);
            float time = time_frame->getTimeAtIndex(data_time[index]);
            float xCanvasPos = static_cast<GLfloat>(time - start_time) / (end_time - start_time) * 2.0f - 1.0f;
            float yCanvasPos = data[index] / absMaxY;
            m_vertices.push_back(xCanvasPos);
            m_vertices.push_back(yCanvasPos);
            m_vertices.push_back(rNorm);
            m_vertices.push_back(gNorm);
            m_vertices.push_back(bNorm);
            m_vertices.push_back(1.0f); // alpha
        }

        m_vbo.bind();
        m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(GLfloat));
        m_vbo.release();
        glDrawArrays(GL_LINE_STRIP, 0, m_vertices.size() / 6);
    }
}

void OpenGLWidget::paintGL() {
    int r, g, b;
    hexToRGB(m_background_color, r, g, b);
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_program->setUniformValue(m_projMatrixLoc, m_proj);
    m_program->setUniformValue(m_viewMatrixLoc, m_view);
    m_program->setUniformValue(m_modelMatrixLoc, m_model);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    //This has been converted to master coordinates
    int currentTime = _time;

    int zoom = _xAxis.getEnd() - _xAxis.getStart();
    _xAxis.setCenterAndZoom(currentTime, zoom);

    // Draw the series
    drawDigitalEventSeries();
    drawDigitalIntervalSeries();
    drawAnalogSeries();

    // Draw horizontal line at x=0
    std::array<GLfloat, 12> lineVertices = {
        0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
    };

    m_vbo.bind();
    m_vbo.allocate(lineVertices.data(), lineVertices.size() * sizeof(GLfloat));
    m_vbo.release();
    glDrawArrays(GL_LINES, 0, 2);

    m_program->release();

    //drawAxis();
}

void OpenGLWidget::resizeGL(int w, int h) {
    m_proj.setToIdentity();
    m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
    //m_proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Use orthographic projection for 2D plotting
    m_view.setToIdentity();
    m_view.translate(0, 0, -2);
}

void OpenGLWidget::drawAxis()
{
    const auto start_time = _xAxis.getStart();
    const auto end_time = _xAxis.getEnd();

}

void OpenGLWidget::addAnalogTimeSeries(
        std::string key,
        std::shared_ptr<AnalogTimeSeries> series,
        std::shared_ptr<TimeFrame> time_frame,
        std::string color) {

    std::string seriesColor = color.empty() ? "#FFFFFF" : color;

    _analog_series[key] =
            AnalogSeriesData{series,
                             seriesColor,
                             time_frame};
    updateCanvas(_time);
}

void OpenGLWidget::removeAnalogTimeSeries(const std::string &key) {
    auto item = _analog_series.find(key);
    if (item != _analog_series.end()) {
        _analog_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalEventSeries(
        std::string key,
        std::shared_ptr <DigitalEventSeries> series,
        std::shared_ptr <TimeFrame> time_frame,
        std::string color) {
    std::string seriesColor = color.empty() ? generateRandomColor() : color;
    _digital_event_series[key] = DigitalEventSeriesData{series, seriesColor, time_frame};
    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalEventSeries(const std::string &key) {
    auto item = _digital_event_series.find(key);
    if (item != _digital_event_series.end()) {
        _digital_event_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::addDigitalIntervalSeries(
        std::string key,
        std::shared_ptr <DigitalIntervalSeries> series,
        std::shared_ptr <TimeFrame> time_frame,
        std::string color) {

    std::string seriesColor = color.empty() ? generateRandomColor() : color;
    _digital_interval_series[key] = DigitalIntervalSeriesData{series, seriesColor, time_frame};
    updateCanvas(_time);
}

void OpenGLWidget::removeDigitalIntervalSeries(const std::string &key) {
    auto item = _digital_interval_series.find(key);
    if (item != _digital_interval_series.end()) {
        _digital_interval_series.erase(item);
    }
    updateCanvas(_time);
}

void OpenGLWidget::clearSeries() {
    _analog_series.clear();
    updateCanvas(_time);
}

void OpenGLWidget::generateRandomValues(int count) {
    m_vertices.clear();
    for (int i = 0; i < count; ++i) {
        m_vertices.push_back(static_cast<GLfloat>(i) / (count - 1) * 2.0f - 1.0f); // X coordinate
        m_vertices.push_back(static_cast<GLfloat>(std::rand()) / RAND_MAX * 2.0f - 1.0f); // Y coordinate
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

void OpenGLWidget::adjustFakeData()
{
    std::vector<float> new_series;
    for (int i = 0; i < _analog_series[0].series->getAnalogTimeSeries().size(); i ++)
    {
        new_series.push_back(static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
    }
    _analog_series[0].series->setData(new_series);
}

