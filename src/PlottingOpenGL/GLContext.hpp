#ifndef PLOTTINGOPENGL_GLCONTEXT_HPP
#define PLOTTINGOPENGL_GLCONTEXT_HPP

/**
 * @file GLContext.hpp
 * @brief OpenGL context and function loading utilities.
 * 
 * This header provides a Qt-based OpenGL function loader that wraps
 * QOpenGLFunctions and QOpenGLExtraFunctions. While PlottingOpenGL aims to be 
 * conceptually separate from Qt's GUI components, we use Qt's OpenGL wrappers 
 * since they handle cross-platform function loading and are already bundled 
 * with the project.
 * 
 * Alternative implementations could use GLAD, GLEW, or native platform APIs.
 */

#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

#include <memory>
#include <string>

namespace PlottingOpenGL {

/**
 * @brief RAII wrapper for a QOpenGLBuffer.
 * 
 * Ensures proper lifecycle management and provides a cleaner interface
 * for batch renderer implementations.
 */
class GLBuffer {
public:
    enum class Type {
        Vertex,// GL_ARRAY_BUFFER
        Index, // GL_ELEMENT_ARRAY_BUFFER
    };

    enum class Usage {
        StaticDraw, // Data set once, drawn many times
        DynamicDraw,// Data modified occasionally, drawn many times
        StreamDraw, // Data modified every frame
    };

    explicit GLBuffer(Type type = Type::Vertex);
    ~GLBuffer();

    GLBuffer(GLBuffer const &) = delete;
    GLBuffer & operator=(GLBuffer const &) = delete;
    GLBuffer(GLBuffer && other) noexcept;
    GLBuffer & operator=(GLBuffer && other) noexcept;

    [[nodiscard]] bool create();
    void destroy();
    [[nodiscard]] bool bind();
    void release();

    void allocate(void const * data, int size_bytes);
    void write(int offset, void const * data, int size_bytes);

    [[nodiscard]] bool isCreated() const;
    [[nodiscard]] int size() const;

private:
    std::unique_ptr<QOpenGLBuffer> m_buffer;
    Type m_type;
};

/**
 * @brief RAII wrapper for a Vertex Array Object.
 * 
 * VAOs store vertex attribute configurations, reducing state changes
 * during rendering.
 */
class GLVertexArray {
public:
    GLVertexArray();
    ~GLVertexArray();

    GLVertexArray(GLVertexArray const &) = delete;
    GLVertexArray & operator=(GLVertexArray const &) = delete;
    GLVertexArray(GLVertexArray && other) noexcept;
    GLVertexArray & operator=(GLVertexArray && other) noexcept;

    [[nodiscard]] bool create();
    void destroy();
    [[nodiscard]] bool bind();
    void release();
    [[nodiscard]] bool isCreated() const;

private:
    std::unique_ptr<QOpenGLVertexArrayObject> m_vao;
};

/**
 * @brief Wrapper for QOpenGLShaderProgram with convenience methods.
 */
class GLShaderProgram {
public:
    GLShaderProgram();
    ~GLShaderProgram();

    GLShaderProgram(GLShaderProgram const &) = delete;
    GLShaderProgram & operator=(GLShaderProgram const &) = delete;
    GLShaderProgram(GLShaderProgram && other) noexcept;
    GLShaderProgram & operator=(GLShaderProgram && other) noexcept;

    /**
     * @brief Compile and link a vertex/fragment shader pair from source strings.
     * @return true if compilation and linking succeeded
     */
    [[nodiscard]] bool createFromSource(std::string const & vertex_source,
                                        std::string const & fragment_source);

    /**
     * @brief Compile and link with an optional geometry shader.
     */
    [[nodiscard]] bool createFromSource(std::string const & vertex_source,
                                        std::string const & geometry_source,
                                        std::string const & fragment_source);

    void destroy();
    [[nodiscard]] bool bind();
    void release();

    void setUniformValue(char const * name, int value);
    void setUniformValue(char const * name, float value);
    void setUniformValue(char const * name, float x, float y);
    void setUniformValue(char const * name, float x, float y, float z, float w);

    /**
     * @brief Set a 4x4 matrix uniform from a glm::mat4.
     * 
     * GLM stores matrices in column-major order, which matches OpenGL's
     * expectation, so we can pass the data pointer directly.
     */
    void setUniformMatrix4(char const * name, float const * values);

    [[nodiscard]] int attributeLocation(char const * name) const;
    [[nodiscard]] int uniformLocation(char const * name) const;

    [[nodiscard]] bool isLinked() const;

    [[nodiscard]] QOpenGLShaderProgram * native() const { return m_program.get(); }

private:
    std::unique_ptr<QOpenGLShaderProgram> m_program;
};

/**
 * @brief Accessor for OpenGL functions in the current context.
 * 
 * Call GLFunctions::get() to obtain a pointer to the QOpenGLFunctions
 * interface for the current context. Returns nullptr if no context is current.
 * 
 * Usage:
 *   auto* f = GLFunctions::get();
 *   if (f) {
 *       f->glDrawArrays(GL_TRIANGLES, 0, 3);
 *   }
 */
class GLFunctions {
public:
    [[nodiscard]] static QOpenGLFunctions * get();

    /**
     * @brief Get extended OpenGL functions (OpenGL 3.x+).
     * 
     * Use this for instanced rendering functions like glDrawArraysInstanced
     * and glVertexAttribDivisor.
     * 
     * @return Pointer to QOpenGLExtraFunctions, or nullptr if no context
     */
    [[nodiscard]] static QOpenGLExtraFunctions * getExtra();

    /**
     * @brief Check if a valid OpenGL context is current.
     */
    [[nodiscard]] static bool hasCurrentContext();
};

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_GLCONTEXT_HPP
