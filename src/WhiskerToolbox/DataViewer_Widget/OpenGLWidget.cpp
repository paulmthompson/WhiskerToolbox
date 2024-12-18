#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <QOpenGLShader>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QWheelEvent>

#include <iostream>
#include <sstream>
#include <iomanip>
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
    std::cout << "Redrawing at " << _time << std::endl;
    update();
}

void hexToRGB(const std::string &hexColor, int &r, int &g, int &b) {
    if (hexColor[0] != '#' || hexColor.length() != 7) {
        throw std::invalid_argument("Invalid hex color format");
    }

    std::stringstream ss;
    ss << std::hex << hexColor.substr(1, 2);
    ss >> r;
    ss.clear();
    ss << std::hex << hexColor.substr(3, 2);
    ss >> g;
    ss.clear();
    ss << std::hex << hexColor.substr(5, 2);
    ss >> b;
}

void OpenGLWidget::setBackgroundColor(const std::string &hexColor)
{
    m_background_color = hexColor;
    update();
}

static const char *vertexShaderSource =
    "#version 150\n"
    "in vec4 vertex;\n"
    "uniform mat4 projMatrix;\n"
    "uniform mat4 viewMatrix;\n"
    "uniform mat4 modelMatrix;\n"
    "void main() {\n"
    "   gl_Position = projMatrix * viewMatrix * modelMatrix * vertex;\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 150\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "   fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
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
    glClearColor(r, g, b, 1);

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
    generateAndAddFakeData(100000);

    setupVertexAttribs();

    m_program->release();

}

void OpenGLWidget::setupVertexAttribs() {
    m_vbo.bind();
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);
    m_vbo.release();
}

void OpenGLWidget::paintGL() {
    int r, g, b;
    hexToRGB(m_background_color, r, g, b);
    glClearColor(r, g, b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_program->setUniformValue(m_projMatrixLoc, m_proj);
    m_program->setUniformValue(m_viewMatrixLoc, m_view);
    m_program->setUniformValue(m_modelMatrixLoc, m_model);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    //adjustFakeData();
    int currentTime = _time;
    int zoom = _xAxis.getEnd() - _xAxis.getStart();
    _xAxis.setCenterAndZoom(currentTime, zoom);

    std::cout << "XAxis: " << _xAxis.getStart() << " - " << _xAxis.getEnd() << std::endl;

    for (size_t i = 0; i < _analog_series.size(); ++i) {
        const auto &series = _analog_series[i];
        const auto &data = series->getAnalogTimeSeries();
        float minY = _series_min_max[i].first;
        float maxY = _series_min_max[i].second;

        m_vertices.clear();
        for (int j = _xAxis.getStart(); j <= _xAxis.getEnd(); ++j) {
            auto it = data.find(j);
            if (it != data.end()) {
                float xCanvasPos = static_cast<GLfloat>(j - _xAxis.getStart()) / (_xAxis.getEnd() - _xAxis.getStart()) * 2.0f - 1.0f; // X coordinate normalized to [-1, 1]
                float yCanvasPos = (it->second - minY) / (maxY - minY) * 2.0f - 1.0f; // Y coordinate, scaled to [-1, 1]
                m_vertices.push_back(xCanvasPos);
                m_vertices.push_back(yCanvasPos);
            }
        }
        m_vbo.bind();
        m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(GLfloat));
        m_vbo.release();
        glDrawArrays(GL_LINE_STRIP, 0, m_vertices.size() / 2);
    }
    //glDrawArrays(GL_LINES, 0, 2);
    //generateRandomValues(100);
    //glDrawArrays(GL_LINE_STRIP, 0, m_vertices.size() / 2);

    m_program->release();
}

void OpenGLWidget::resizeGL(int w, int h) {
    m_proj.setToIdentity();
    m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
    //m_proj.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Use orthographic projection for 2D plotting
    m_view.setToIdentity();
    m_view.translate(0, 0, -2);
}

void OpenGLWidget::addAnalogTimeSeries(std::shared_ptr<AnalogTimeSeries> series) {
    _analog_series.push_back(series);
    _series_min_max.emplace_back(series->getMinValue(), series->getMaxValue());
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
    addAnalogTimeSeries(series);
}

void OpenGLWidget::adjustFakeData()
{
    std::vector<float> new_series;
    for (int i = 0; i < _analog_series[0]->getAnalogTimeSeries().size(); i ++)
    {
        new_series.push_back(static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f);
    }
    _analog_series[0]->setData(new_series);
}

void OpenGLWidget::wheelEvent(QWheelEvent *event) {
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;
    int zoomFactor = 10; // Adjust this value to control zoom sensitivity

    int center = (_xAxis.getStart() + _xAxis.getEnd()) / 2;
    int zoom = (_xAxis.getEnd() - _xAxis.getStart()) - numSteps * zoomFactor;

    _xAxis.setCenterAndZoom(center, zoom);
    updateCanvas(_time);
}
