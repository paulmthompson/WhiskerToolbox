/**
 * @file GLSSBOBuffer.hpp
 * @brief RAII wrapper for OpenGL Shader Storage Buffer Objects (SSBOs)
 *
 * QOpenGLBuffer does not support GL_SHADER_STORAGE_BUFFER, so we manage
 * SSBOs directly via raw GL calls wrapped in an RAII class.
 *
 * Requires OpenGL 4.3+.
 */
#ifndef PLOTTINGOPENGL_LINEBATCH_GLSSBOBUFFER_HPP
#define PLOTTINGOPENGL_LINEBATCH_GLSSBOBUFFER_HPP

#include "PlottingOpenGL/GLContext.hpp"

#include <cstdint>

namespace PlottingOpenGL {

/**
 * @brief RAII wrapper for a GL_SHADER_STORAGE_BUFFER.
 *
 * Manages a single SSBO with create / upload / partial-write / bind-base
 * operations.  Destruction releases the GL resource.
 *
 * All methods require a current OpenGL context.
 */
class GLSSBOBuffer {
public:
    GLSSBOBuffer() = default;
    ~GLSSBOBuffer();

    GLSSBOBuffer(GLSSBOBuffer const &) = delete;
    GLSSBOBuffer & operator=(GLSSBOBuffer const &) = delete;
    GLSSBOBuffer(GLSSBOBuffer && other) noexcept;
    GLSSBOBuffer & operator=(GLSSBOBuffer && other) noexcept;

    /// Create the underlying GL buffer object.  Returns true on success.
    [[nodiscard]] bool create();

    /// Destroy the GL buffer object (safe to call even if not created).
    void destroy();

    /// @return true if the buffer has been created and is valid.
    [[nodiscard]] bool isCreated() const { return m_buffer_id != 0; }

    /// @return The raw GL buffer name (0 if not created).
    [[nodiscard]] std::uint32_t bufferId() const { return m_buffer_id; }

    /**
     * @brief Allocate and optionally fill the buffer.
     *
     * @param data       Pointer to source data, or nullptr for uninitialized allocation.
     * @param size_bytes Total size in bytes to allocate.
     *
     * Uses GL_DYNAMIC_DRAW since these buffers are updated periodically.
     */
    void allocate(void const * data, int size_bytes);

    /**
     * @brief Write into an existing allocation without reallocating.
     *
     * @param offset_bytes Byte offset from the start of the buffer.
     * @param data         Pointer to source data.
     * @param size_bytes   Number of bytes to write.
     */
    void write(int offset_bytes, void const * data, int size_bytes);

    /**
     * @brief Bind this buffer to a numbered SSBO binding point.
     *
     * Equivalent to glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_buffer_id).
     */
    void bindBase(std::uint32_t binding) const;

    /// @return Current allocated size in bytes (as reported by GL).
    [[nodiscard]] int size() const { return m_size_bytes; }

    /**
     * @brief Map the buffer for CPU read-back (GL_READ_ONLY).
     *
     * @return Pointer to mapped memory, or nullptr on failure.
     *         Caller must call unmap() when done.
     */
    [[nodiscard]] void * mapReadOnly() const;

    /// Unmap a previously mapped buffer.
    void unmap() const;

private:
    std::uint32_t m_buffer_id{0};
    int m_size_bytes{0};
};

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_LINEBATCH_GLSSBOBUFFER_HPP
