/**
 * @file GLSSBOBuffer.cpp
 * @brief Implementation of the GLSSBOBuffer RAII wrapper
 */
#include "GLSSBOBuffer.hpp"

#include <QOpenGLExtraFunctions>

// Define GL 4.3 constants if not available (macOS only supports GL 4.1)
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

namespace PlottingOpenGL {

GLSSBOBuffer::~GLSSBOBuffer()
{
    destroy();
}

GLSSBOBuffer::GLSSBOBuffer(GLSSBOBuffer && other) noexcept
    : m_buffer_id(other.m_buffer_id),
      m_size_bytes(other.m_size_bytes)
{
    other.m_buffer_id = 0;
    other.m_size_bytes = 0;
}

GLSSBOBuffer & GLSSBOBuffer::operator=(GLSSBOBuffer && other) noexcept
{
    if (this != &other) {
        destroy();
        m_buffer_id = other.m_buffer_id;
        m_size_bytes = other.m_size_bytes;
        other.m_buffer_id = 0;
        other.m_size_bytes = 0;
    }
    return *this;
}

bool GLSSBOBuffer::create()
{
    if (m_buffer_id != 0) {
        return true; // Already created
    }

    auto * f = GLFunctions::get();
    if (!f) {
        return false;
    }

    f->glGenBuffers(1, &m_buffer_id);
    return m_buffer_id != 0;
}

void GLSSBOBuffer::destroy()
{
    if (m_buffer_id != 0) {
        auto * f = GLFunctions::get();
        if (f) {
            f->glDeleteBuffers(1, &m_buffer_id);
        }
        m_buffer_id = 0;
        m_size_bytes = 0;
    }
}

void GLSSBOBuffer::allocate(void const * data, int size_bytes)
{
    if (m_buffer_id == 0) {
        return;
    }
    auto * f = GLFunctions::get();
    if (!f) {
        return;
    }

    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);
    f->glBufferData(GL_SHADER_STORAGE_BUFFER, size_bytes, data, GL_DYNAMIC_DRAW);
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_size_bytes = size_bytes;
}

void GLSSBOBuffer::write(int offset_bytes, void const * data, int size_bytes)
{
    if (m_buffer_id == 0) {
        return;
    }
    auto * f = GLFunctions::get();
    if (!f) {
        return;
    }

    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);
    f->glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset_bytes, size_bytes, data);
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GLSSBOBuffer::bindBase(std::uint32_t binding) const
{
    if (m_buffer_id == 0) {
        return;
    }
    auto * ef = GLFunctions::getExtra();
    if (!ef) {
        return;
    }

    ef->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_buffer_id);
}

void * GLSSBOBuffer::mapReadOnly() const
{
    if (m_buffer_id == 0) {
        return nullptr;
    }
    auto * f = GLFunctions::get();
    if (!f) {
        return nullptr;
    }

    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);

    // glMapBufferRange is available via QOpenGLExtraFunctions
    auto * ef = GLFunctions::getExtra();
    if (!ef) {
        f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return nullptr;
    }

    void * ptr = ef->glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, m_size_bytes, GL_MAP_READ_BIT);
    // Note: buffer stays bound — caller must call unmap()
    return ptr;
}

void GLSSBOBuffer::unmap() const
{
    if (m_buffer_id == 0) {
        return;
    }
    auto * ef = GLFunctions::getExtra();
    if (!ef) {
        return;
    }

    ef->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    auto * f = GLFunctions::get();
    if (f) {
        f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

} // namespace PlottingOpenGL
