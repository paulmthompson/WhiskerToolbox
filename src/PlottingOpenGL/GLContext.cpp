#include "GLContext.hpp"

#include <QOpenGLShader>

namespace PlottingOpenGL {

// ============================================================================
// GLBuffer Implementation
// ============================================================================

GLBuffer::GLBuffer(Type type)
    : m_type(type) {
}

GLBuffer::~GLBuffer() {
    destroy();
}

GLBuffer::GLBuffer(GLBuffer && other) noexcept
    : m_buffer(std::move(other.m_buffer)),
      m_type(other.m_type) {
}

GLBuffer & GLBuffer::operator=(GLBuffer && other) noexcept {
    if (this != &other) {
        destroy();
        m_buffer = std::move(other.m_buffer);
        m_type = other.m_type;
    }
    return *this;
}

bool GLBuffer::create() {
    if (m_buffer && m_buffer->isCreated()) {
        return true;// Already created
    }

    auto qt_type = (m_type == Type::Index)
                           ? QOpenGLBuffer::IndexBuffer
                           : QOpenGLBuffer::VertexBuffer;

    m_buffer = std::make_unique<QOpenGLBuffer>(qt_type);
    return m_buffer->create();
}

void GLBuffer::destroy() {
    if (m_buffer) {
        m_buffer->destroy();
        m_buffer.reset();
    }
}

bool GLBuffer::bind() {
    return m_buffer && m_buffer->bind();
}

void GLBuffer::release() {
    if (m_buffer) {
        m_buffer->release();
    }
}

void GLBuffer::allocate(void const * data, int size_bytes) {
    if (m_buffer) {
        m_buffer->allocate(data, size_bytes);
    }
}

void GLBuffer::write(int offset, void const * data, int size_bytes) {
    if (m_buffer) {
        m_buffer->write(offset, data, size_bytes);
    }
}

bool GLBuffer::isCreated() const {
    return m_buffer && m_buffer->isCreated();
}

int GLBuffer::size() const {
    return m_buffer ? m_buffer->size() : 0;
}

// ============================================================================
// GLVertexArray Implementation
// ============================================================================

GLVertexArray::GLVertexArray() = default;

GLVertexArray::~GLVertexArray() {
    destroy();
}

GLVertexArray::GLVertexArray(GLVertexArray && other) noexcept
    : m_vao(std::move(other.m_vao)) {
}

GLVertexArray & GLVertexArray::operator=(GLVertexArray && other) noexcept {
    if (this != &other) {
        destroy();
        m_vao = std::move(other.m_vao);
    }
    return *this;
}

bool GLVertexArray::create() {
    if (m_vao && m_vao->isCreated()) {
        return true;
    }
    m_vao = std::make_unique<QOpenGLVertexArrayObject>();
    return m_vao->create();
}

void GLVertexArray::destroy() {
    if (m_vao) {
        m_vao->destroy();
        m_vao.reset();
    }
}

bool GLVertexArray::bind() {
    if (m_vao && m_vao->isCreated()) {
        m_vao->bind();
        return true;
    }
    return false;
}

void GLVertexArray::release() {
    if (m_vao) {
        m_vao->release();
    }
}

bool GLVertexArray::isCreated() const {
    return m_vao && m_vao->isCreated();
}

// ============================================================================
// GLShaderProgram Implementation
// ============================================================================

GLShaderProgram::GLShaderProgram() = default;

GLShaderProgram::~GLShaderProgram() {
    destroy();
}

GLShaderProgram::GLShaderProgram(GLShaderProgram && other) noexcept
    : m_program(std::move(other.m_program)) {
}

GLShaderProgram & GLShaderProgram::operator=(GLShaderProgram && other) noexcept {
    if (this != &other) {
        destroy();
        m_program = std::move(other.m_program);
    }
    return *this;
}

bool GLShaderProgram::createFromSource(std::string const & vertex_source,
                                       std::string const & fragment_source) {
    destroy();
    m_program = std::make_unique<QOpenGLShaderProgram>();

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                            vertex_source.c_str())) {
        return false;
    }

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                            fragment_source.c_str())) {
        return false;
    }

    return m_program->link();
}

bool GLShaderProgram::createFromSource(std::string const & vertex_source,
                                       std::string const & geometry_source,
                                       std::string const & fragment_source) {
    destroy();
    m_program = std::make_unique<QOpenGLShaderProgram>();

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                            vertex_source.c_str())) {
        return false;
    }

    if (!geometry_source.empty()) {
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Geometry,
                                                geometry_source.c_str())) {
            return false;
        }
    }

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                            fragment_source.c_str())) {
        return false;
    }

    return m_program->link();
}

void GLShaderProgram::destroy() {
    if (m_program) {
        m_program.reset();
    }
}

bool GLShaderProgram::bind() {
    return m_program && m_program->bind();
}

void GLShaderProgram::release() {
    if (m_program) {
        m_program->release();
    }
}

void GLShaderProgram::setUniformValue(char const * name, int value) {
    if (m_program) {
        m_program->setUniformValue(name, value);
    }
}

void GLShaderProgram::setUniformValue(char const * name, float value) {
    if (m_program) {
        m_program->setUniformValue(name, value);
    }
}

void GLShaderProgram::setUniformValue(char const * name, float x, float y) {
    if (m_program) {
        m_program->setUniformValue(name, x, y);
    }
}

void GLShaderProgram::setUniformValue(char const * name, float x, float y, float z, float w) {
    if (m_program) {
        m_program->setUniformValue(name, x, y, z, w);
    }
}

void GLShaderProgram::setUniformMatrix4(char const * name, float const * values) {
    if (m_program) {
        // GLM is column-major, OpenGL expects column-major, no transpose needed
        m_program->setUniformValue(name, QMatrix4x4(values).transposed());
    }
}

int GLShaderProgram::attributeLocation(char const * name) const {
    return m_program ? m_program->attributeLocation(name) : -1;
}

int GLShaderProgram::uniformLocation(char const * name) const {
    return m_program ? m_program->uniformLocation(name) : -1;
}

bool GLShaderProgram::isLinked() const {
    return m_program && m_program->isLinked();
}

// ============================================================================
// GLFunctions Implementation
// ============================================================================

QOpenGLFunctions * GLFunctions::get() {
    auto * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        return nullptr;
    }
    return ctx->functions();
}

QOpenGLExtraFunctions * GLFunctions::getExtra() {
    auto * ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        return nullptr;
    }
    return ctx->extraFunctions();
}

bool GLFunctions::hasCurrentContext() {
    return QOpenGLContext::currentContext() != nullptr;
}

}// namespace PlottingOpenGL
