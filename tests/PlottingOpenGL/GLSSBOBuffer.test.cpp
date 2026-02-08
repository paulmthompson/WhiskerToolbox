/**
 * @file GLSSBOBuffer.test.cpp
 * @brief Integration tests for the GLSSBOBuffer RAII wrapper
 *
 * These tests require a headless OpenGL 4.3 context (SSBOs are a 4.3 feature).
 * They verify create / allocate / write / readback / move operations.
 */
#include "HeadlessGLFixture.hpp"

#include "PlottingOpenGL/LineBatch/GLSSBOBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

// ── Creation / Destruction ─────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — default-constructed is not created",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE_FALSE(buf.isCreated());
    REQUIRE(buf.bufferId() == 0);
    REQUIRE(buf.size() == 0);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — create allocates a GL buffer name",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());
    REQUIRE(buf.isCreated());
    REQUIRE(buf.bufferId() != 0);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — destroy releases the buffer",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());
    auto const id = buf.bufferId();
    REQUIRE(id != 0);

    buf.destroy();
    REQUIRE_FALSE(buf.isCreated());
    REQUIRE(buf.bufferId() == 0);
    REQUIRE(buf.size() == 0);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — double create is idempotent",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());
    auto const id = buf.bufferId();
    REQUIRE(buf.create());            // second call
    REQUIRE(buf.bufferId() == id);    // same buffer
}

// ── Allocate + Readback ────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — allocate and readback float data",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());

    std::vector<float> data = {1.0f, 2.5f, 3.0f, 4.5f, 5.0f};
    buf.allocate(data.data(), static_cast<int>(data.size() * sizeof(float)));

    REQUIRE(buf.size() == static_cast<int>(data.size() * sizeof(float)));

    auto const * ptr = static_cast<float const *>(buf.mapReadOnly());
    REQUIRE(ptr != nullptr);

    for (std::size_t i = 0; i < data.size(); ++i) {
        REQUIRE(ptr[i] == data[i]);
    }

    buf.unmap();
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — allocate and readback uint32 data",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());

    std::vector<std::uint32_t> data = {0, 42, 100, 0xDEADBEEF};
    buf.allocate(data.data(), static_cast<int>(data.size() * sizeof(std::uint32_t)));

    auto const * ptr = static_cast<std::uint32_t const *>(buf.mapReadOnly());
    REQUIRE(ptr != nullptr);

    for (std::size_t i = 0; i < data.size(); ++i) {
        REQUIRE(ptr[i] == data[i]);
    }

    buf.unmap();
}

// ── Partial Write ──────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — partial write updates subrange",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());

    // Initial: {10, 20, 30, 40}
    std::vector<std::uint32_t> initial = {10, 20, 30, 40};
    buf.allocate(initial.data(),
                 static_cast<int>(initial.size() * sizeof(std::uint32_t)));

    // Overwrite elements [1] and [2] with {99, 88}
    std::vector<std::uint32_t> patch = {99, 88};
    buf.write(static_cast<int>(1 * sizeof(std::uint32_t)),
              patch.data(),
              static_cast<int>(patch.size() * sizeof(std::uint32_t)));

    auto const * ptr = static_cast<std::uint32_t const *>(buf.mapReadOnly());
    REQUIRE(ptr != nullptr);

    REQUIRE(ptr[0] == 10);
    REQUIRE(ptr[1] == 99);
    REQUIRE(ptr[2] == 88);
    REQUIRE(ptr[3] == 40);

    buf.unmap();
}

// ── Move Semantics ─────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — move constructor transfers ownership",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf1;
    REQUIRE(buf1.create());
    auto const id = buf1.bufferId();

    PlottingOpenGL::GLSSBOBuffer buf2(std::move(buf1));
    REQUIRE(buf2.bufferId() == id);
    REQUIRE(buf2.isCreated());
    REQUIRE(buf1.bufferId() == 0);   // NOLINT(bugprone-use-after-move)
    REQUIRE_FALSE(buf1.isCreated()); // NOLINT(bugprone-use-after-move)
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — move assignment transfers ownership",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf1;
    REQUIRE(buf1.create());
    auto const id = buf1.bufferId();

    PlottingOpenGL::GLSSBOBuffer buf2;
    buf2 = std::move(buf1);
    REQUIRE(buf2.bufferId() == id);
    REQUIRE(buf1.bufferId() == 0);  // NOLINT(bugprone-use-after-move)
}

// ── Bind ───────────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — bindBase does not crash",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());

    std::vector<float> data = {1.0f, 2.0f};
    buf.allocate(data.data(), static_cast<int>(data.size() * sizeof(float)));

    // Binding to an arbitrary point should not crash
    buf.bindBase(7);
}

// ── Allocate with nullptr (uninitialized) ──────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "GLSSBOBuffer — allocate with nullptr reserves space",
                 "[PlottingOpenGL][GLSSBOBuffer]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::GLSSBOBuffer buf;
    REQUIRE(buf.create());

    constexpr int size_bytes = 256;
    buf.allocate(nullptr, size_bytes);
    REQUIRE(buf.size() == size_bytes);
}
