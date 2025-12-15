#include "ShaderProgram.hpp"

#include <QDebug>
#include <QFile>
#include <QOpenGLContext>
#include <QOpenGLShader>
#include <QTextStream>

#include <iostream>

ShaderProgram::ShaderProgram(std::string const & vertexPath,
                             std::string const & fragmentPath,
                             std::string const & geometryPath,
                             ShaderSourceType sourceType)
    : m_vertexPath(vertexPath),
      m_fragmentPath(fragmentPath),
      m_geometryPath(geometryPath),
      m_sourceType(sourceType),
      m_isComputeShader(false) {
    m_program = std::make_unique<QOpenGLShaderProgram>();
}

ShaderProgram::ShaderProgram(std::string const & computePath,
                             ShaderSourceType sourceType)
    : m_computePath(computePath),
      m_sourceType(sourceType),
      m_isComputeShader(true) {
    m_program = std::make_unique<QOpenGLShaderProgram>();
}

ShaderProgram::~ShaderProgram() {
    // Don't explicitly call removeAllShaders() in destructor
    // QOpenGLShaderProgram's destructor will handle OpenGL resource cleanup
    // when the context is still valid, and skip it when the context is gone
}

bool ShaderProgram::reload() {
    // Save old program in case reload fails
    auto oldProgram = std::move(m_program);
    m_program = std::make_unique<QOpenGLShaderProgram>();
    bool const success = compileAndLink();
    if (!success) {
        m_program = std::move(oldProgram);
        return false;
    }
    m_uniformLocations.clear();
    return true;
}

void ShaderProgram::use() {
    if (m_program) {
        m_program->bind();
    }
}

void ShaderProgram::setUniform(std::string const & name, int value) {
    if (m_program) {
        m_program->setUniformValue(name.c_str(), value);
    }
}

void ShaderProgram::setUniform(std::string const & name, float value) {
    if (m_program) {
        m_program->setUniformValue(name.c_str(), value);
    }
}

void ShaderProgram::setUniform(std::string const & name, glm::mat4 const & matrix) {
    if (m_program) {
        m_program->setUniformValue(name.c_str(), QMatrix4x4(&matrix[0][0]));
    }
}

bool ShaderProgram::compileAndLink() {
    bool ok = true;
    QString errorLog;

    // Handle compute shaders differently
    if (m_isComputeShader) {
        if (!m_computePath.empty()) {
            QString src;
            if (m_sourceType == ShaderSourceType::FileSystem) {
                std::string source;
                if (!loadShaderSource(m_computePath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load compute shader: " << m_computePath << std::endl;
                    return false;
                } else {
                    src = QString::fromStdString(source);
                }
            } else {
                std::string source;
                if (!loadShaderSourceResource(m_computePath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load compute shader resource: " << m_computePath << std::endl;
                    return false;
                } else {
                    src = QString::fromStdString(source);
                }
            }
            if (!m_program->addShaderFromSourceCode(QOpenGLShader::Compute, src)) {
                errorLog += m_program->log();
                std::cerr << "[ShaderProgram] Compute shader compile error: " << errorLog.toStdString() << std::endl;
                return false;
            }
        }
    } else {
        // Traditional vertex/fragment/geometry shaders
        // Vertex shader
        if (!m_vertexPath.empty()) {
            QString src;
            if (m_sourceType == ShaderSourceType::FileSystem) {
                std::string source;
                if (!loadShaderSource(m_vertexPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load vertex shader: " << m_vertexPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            } else {
                std::string source;
                if (!loadShaderSourceResource(m_vertexPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load vertex shader resource: " << m_vertexPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            }
            if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, src)) {
                errorLog += m_program->log();
                ok = false;
            }
        }
        // Fragment shader
        if (!m_fragmentPath.empty()) {
            QString src;
            if (m_sourceType == ShaderSourceType::FileSystem) {
                std::string source;
                if (!loadShaderSource(m_fragmentPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load fragment shader: " << m_fragmentPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            } else {
                std::string source;
                if (!loadShaderSourceResource(m_fragmentPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load fragment shader resource: " << m_fragmentPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            }
            if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, src)) {
                errorLog += m_program->log();
                ok = false;
            }
        }
        // Geometry shader (optional)
        if (!m_geometryPath.empty()) {
            QString src;
            if (m_sourceType == ShaderSourceType::FileSystem) {
                std::string source;
                if (!loadShaderSource(m_geometryPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load geometry shader: " << m_geometryPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            } else {
                std::string source;
                if (!loadShaderSourceResource(m_geometryPath, source)) {
                    std::cerr << "[ShaderProgram] Failed to load geometry shader resource: " << m_geometryPath << std::endl;
                    ok = false;
                } else {
                    src = QString::fromStdString(source);
                }
            }
            if (!m_program->addShaderFromSourceCode(QOpenGLShader::Geometry, src)) {
                errorLog += m_program->log();
                ok = false;
            }
        }
    }// End of else block for traditional shaders
    if (!ok) {
        std::cerr << "[ShaderProgram] Shader compile/link error: " << errorLog.toStdString() << std::endl;
        return false;
    }
    if (!m_program->link()) {
        std::cerr << "[ShaderProgram] Program link error: " << m_program->log().toStdString() << std::endl;
        return false;
    }
    return true;
}

bool ShaderProgram::loadShaderSource(std::string const & path, std::string & outSource) const {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    outSource = in.readAll().toStdString();
    return true;
}

bool ShaderProgram::loadShaderSourceResource(std::string const & resourcePath, std::string & outSource) const {
    QFile file(QString::fromStdString(resourcePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    outSource = in.readAll().toStdString();
    return true;
}