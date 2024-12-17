#include "OpenGLWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <QOpenGLShader>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <iostream>
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

void OpenGLWidget::updateCanvas()
{
    std::cout << "Redrawing" << std::endl;
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
    glClearColor(0, 0, 0, 1);

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_program->bind();
    m_program->setUniformValue(m_projMatrixLoc, m_proj);
    m_program->setUniformValue(m_viewMatrixLoc, m_view);
    m_program->setUniformValue(m_modelMatrixLoc, m_model);

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    //glDrawArrays(GL_LINES, 0, 2);
    generateRandomValues(100);
    glDrawArrays(GL_LINE_STRIP, 0, m_vertices.size() / 2);

    m_program->release();
}

void OpenGLWidget::resizeGL(int w, int h) {
    m_proj.setToIdentity();
    m_proj.perspective(45.0f, GLfloat(w) / h, 0.01f, 100.0f);
    m_view.setToIdentity();
    m_view.translate(0, 0, -2);
}

void OpenGLWidget::addAnalogTimeSeries(std::shared_ptr<AnalogTimeSeries> series) {
    _analog_series.push_back(series);
    updateCanvas();
}

void OpenGLWidget::clearSeries() {
    _analog_series.clear();
    updateCanvas();
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
