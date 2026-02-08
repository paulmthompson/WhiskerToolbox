/**
 * @file HeadlessGLFixture.hpp
 * @brief Catch2 test fixture providing a headless OpenGL 4.3 Core context
 *
 * Uses QOffscreenSurface + QOpenGLContext to create a GPU-backed context
 * without a visible window.  Tests that inherit from this fixture can call
 * OpenGL functions and exercise SSBOs, compute shaders, etc.
 *
 * If the system cannot provide OpenGL 4.3 (e.g. macOS, software renderer),
 * the fixture calls SKIP() so CI does not fail.
 */
#ifndef HEADLESS_GL_FIXTURE_HPP
#define HEADLESS_GL_FIXTURE_HPP

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <iostream>
#include <memory>
#include <string>

/**
 * @brief Headless OpenGL 4.3 Core context fixture for PlottingOpenGL tests.
 *
 * Manages the lifetime of a QApplication (singleton), a QOpenGLContext, and a
 * QOffscreenSurface.  The GL context is made current in the constructor and
 * released in the destructor.
 *
 * Use `REQUIRE(isGLAvailable())` at the start of every TEST_CASE_METHOD to
 * guard against unavailable contexts.
 */
class HeadlessGLFixture {
protected:
    HeadlessGLFixture()
    {
        // Suppress noisy Qt Wayland logging
        qputenv("QT_LOGGING_RULES", "qt.qpa.wayland*=false");

        // Create QApplication singleton (Catch2 re-uses the process)
        static int argc = 1;
        static char * argv[] = {const_cast<char *>("test")};
        if (!QApplication::instance()) {
            m_app = std::make_unique<QApplication>(argc, argv);
        }

        // Request OpenGL 4.3 Core (needed for SSBOs / compute shaders)
        QSurfaceFormat format;
        format.setVersion(4, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        format.setSwapInterval(0);

        m_context = std::make_unique<QOpenGLContext>();
        m_context->setFormat(format);

        m_surface = std::make_unique<QOffscreenSurface>();
        m_surface->setFormat(format);
        m_surface->create();

        if (!m_context->create()) {
            SKIP("Cannot create OpenGL context — skipping GPU test");
            return;
        }

        // Verify that we actually received 4.3+
        auto const actual = m_context->format();
        if (actual.majorVersion() < 4 ||
            (actual.majorVersion() == 4 && actual.minorVersion() < 3)) {
            SKIP("OpenGL 4.3 not available (got " +
                 std::to_string(actual.majorVersion()) + "." +
                 std::to_string(actual.minorVersion()) + ") — skipping GPU test");
            return;
        }

        if (!m_context->makeCurrent(m_surface.get())) {
            SKIP("Cannot make OpenGL context current — skipping GPU test");
            return;
        }

        m_gl_available = true;

        // Print GL info once per process
        static bool printed = false;
        if (!printed) {
            auto * f = m_context->functions();
            auto const * version =
                reinterpret_cast<char const *>(f->glGetString(GL_VERSION));
            auto const * renderer =
                reinterpret_cast<char const *>(f->glGetString(GL_RENDERER));
            std::cout << "[HeadlessGL] Version : " << (version ? version : "?")
                      << "\n"
                      << "[HeadlessGL] Renderer: " << (renderer ? renderer : "?")
                      << "\n";
            printed = true;
        }
    }

    ~HeadlessGLFixture()
    {
        if (m_context) {
            m_context->doneCurrent();
        }
        if (m_surface) {
            m_surface->destroy();
        }
    }

    /// @return true if the GL 4.3 context was successfully created and made current.
    [[nodiscard]] bool isGLAvailable() const { return m_gl_available; }

    /// @return The underlying QOpenGLContext (for manual GL calls in tests).
    [[nodiscard]] QOpenGLContext * context() const { return m_context.get(); }

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QOpenGLContext> m_context;
    bool m_gl_available{false};
};

#endif // HEADLESS_GL_FIXTURE_HPP
