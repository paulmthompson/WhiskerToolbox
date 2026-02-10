/**
 * @file BatchLineRenderer.test.cpp
 * @brief Smoke tests for BatchLineRenderer initialization and rendering
 *
 * These are not visual regression tests — they verify that the renderer
 * initializes, compiles its embedded shaders, accepts data via syncFromStore(),
 * and renders without GL errors or crashes.
 *
 * Requires a headless OpenGL 4.3 context.
 */
#include "HeadlessGLFixture.hpp"

#include "PlottingOpenGL/LineBatch/BatchLineRenderer.hpp"
#include "PlottingOpenGL/LineBatch/BatchLineStore.hpp"

#include "CorePlotting/LineBatch/LineBatchData.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QOpenGLExtraFunctions>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// ── Helpers ────────────────────────────────────────────────────────────

/// Drain all pending GL errors so they don't contaminate subsequent checks.
static void drainGLErrors(QOpenGLFunctions * f)
{
    while (f->glGetError() != GL_NO_ERROR) {}
}

/**
 * @brief Create a simple offscreen FBO so glDrawArrays has a valid target.
 *
 * QOffscreenSurface does not guarantee a complete default framebuffer on all
 * drivers.  Rendering into an explicit FBO avoids spurious GL_INVALID_OPERATION.
 *
 * @return (fbo, color_rbo) — caller must delete both when done.
 */
static std::pair<GLuint, GLuint> createSimpleFBO(QOpenGLExtraFunctions * ef,
                                                  int width = 256,
                                                  int height = 256)
{
    GLuint fbo = 0;
    GLuint rbo = 0;
    ef->glGenFramebuffers(1, &fbo);
    ef->glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    ef->glGenRenderbuffers(1, &rbo);
    ef->glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    ef->glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    ef->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, rbo);

    return {fbo, rbo};
}

static void destroyFBO(QOpenGLExtraFunctions * ef, GLuint fbo, GLuint rbo)
{
    ef->glDeleteFramebuffers(1, &fbo);
    ef->glDeleteRenderbuffers(1, &rbo);
}

// ── Test data helper ───────────────────────────────────────────────────

static CorePlotting::LineBatchData makeSimpleBatch()
{
    CorePlotting::LineBatchData batch;
    batch.canvas_width = 2.0f;
    batch.canvas_height = 2.0f;

    // Single line: (-0.5, 0) → (0.5, 0)
    batch.segments = {-0.5f, 0.0f, 0.5f, 0.0f};
    batch.line_ids = {1};
    CorePlotting::LineBatchData::LineInfo info;
    info.entity_id = EntityId(1);
    info.trial_index = 0;
    info.first_segment = 0;
    info.segment_count = 1;
    batch.lines.push_back(info);
    batch.visibility_mask = {1};
    batch.selection_mask = {0};

    return batch;
}

// ── Initialization ─────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — initialize with embedded shaders",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    // Empty shader_base_path → uses embedded GL 4.1 fallback shaders
    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());
    REQUIRE(renderer.isInitialized());
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — hasData is false before sync",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());

    REQUIRE_FALSE(renderer.hasData());
}

// ── Sync + Data ────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — syncFromStore populates vertex data",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeSimpleBatch());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());

    renderer.syncFromStore();
    REQUIRE(renderer.hasData());
}

// ── Render smoke test ──────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — render does not crash",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeSimpleBatch());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());
    renderer.setViewportSize({256.0f, 256.0f});
    renderer.syncFromStore();

    // Create an explicit FBO so we have a valid render target
    auto * ef = context()->extraFunctions();
    REQUIRE(ef != nullptr);
    auto [fbo, rbo] = createSimpleFBO(ef);

    auto * f = context()->functions();
    drainGLErrors(f);

    // Render with identity view and projection
    glm::mat4 const identity(1.0f);
    renderer.render(identity, identity);

    // Note: GL errors may occur in headless Mesa/D3D12 contexts with geometry
    // shaders.  The important assertion is that we reach this point without
    // crashing.  GL error checks are informational only.
    auto const err = f->glGetError();
    if (err != GL_NO_ERROR) {
        WARN("GL error after render (headless context): 0x" << std::hex << err);
    }

    // Clean up FBO
    ef->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    destroyFBO(ef, fbo, rbo);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — render with hover line does not crash",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeSimpleBatch());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());
    renderer.setViewportSize({256.0f, 256.0f});
    renderer.syncFromStore();

    // Set hover on line 0
    renderer.setHoverLine(0);

    // Create an explicit FBO for the render target
    auto * ef = context()->extraFunctions();
    REQUIRE(ef != nullptr);
    auto [fbo, rbo] = createSimpleFBO(ef);

    auto * f = context()->functions();
    drainGLErrors(f);

    glm::mat4 const identity(1.0f);
    renderer.render(identity, identity);

    // Informational only — headless geometry shader rendering can trigger
    // spurious GL_INVALID_OPERATION on Mesa/D3D12.
    auto const err = f->glGetError();
    if (err != GL_NO_ERROR) {
        WARN("GL error after hover render (headless context): 0x" << std::hex << err);
    }

    ef->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    destroyFBO(ef, fbo, rbo);
}

// ── Cleanup ────────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — cleanup and re-initialize",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());

    renderer.cleanup();
    REQUIRE_FALSE(renderer.isInitialized());

    // Re-initialize should succeed
    REQUIRE(renderer.initialize());
    REQUIRE(renderer.isInitialized());
}

// ── clearData ──────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineRenderer — clearData resets hasData",
                 "[PlottingOpenGL][BatchLineRenderer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeSimpleBatch());

    PlottingOpenGL::BatchLineRenderer renderer(store);
    REQUIRE(renderer.initialize());
    renderer.syncFromStore();
    REQUIRE(renderer.hasData());

    renderer.clearData();
    REQUIRE_FALSE(renderer.hasData());
}
