/**
 * @file ComputeShaderIntersector.test.cpp
 * @brief Integration tests comparing GPU compute shader intersection against
 *        the CPU reference implementation
 *
 * These tests upload known geometry to BatchLineStore, run both the
 * ComputeShaderIntersector (GPU) and CpuLineBatchIntersector (CPU) with
 * identical queries, and verify that they return the same results.
 *
 * Requires a headless OpenGL 4.3 context and the line_intersection.comp
 * shader accessible on the filesystem (path injected via TEST_SHADER_DIR).
 */
#include "HeadlessGLFixture.hpp"

#include "PlottingOpenGL/LineBatch/BatchLineStore.hpp"
#include "PlottingOpenGL/LineBatch/ComputeShaderIntersector.hpp"

#include "CorePlotting/LineBatch/CpuLineBatchIntersector.hpp"
#include "CorePlotting/LineBatch/ILineBatchIntersector.hpp"
#include "CorePlotting/LineBatch/LineBatchData.hpp"

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// ── Shader path (injected by CMake) ────────────────────────────────────

#ifndef TEST_SHADER_DIR
    #error "TEST_SHADER_DIR must be defined (path to directory containing line_intersection.comp)"
#endif

static std::string shaderPath()
{
    return std::string(TEST_SHADER_DIR) + "/line_intersection.comp";
}

// ── Helpers ────────────────────────────────────────────────────────────

/// Sort intersection results for order-independent comparison.
static std::vector<std::uint32_t> sorted(
    CorePlotting::LineIntersectionResult const & r)
{
    auto v = r.intersected_line_indices;
    std::sort(v.begin(), v.end());
    return v;
}

/**
 * @brief Build two diagonals crossing at the origin (NDC space).
 *
 * Line 1: (-0.5, -0.5) → (0.5, 0.5)
 * Line 2: (-0.5,  0.5) → (0.5, -0.5)
 */
static CorePlotting::LineBatchData makeCrossBatch()
{
    CorePlotting::LineBatchData batch;
    batch.canvas_width = 2.0f;
    batch.canvas_height = 2.0f;

    // Line 1: diagonal ↗
    batch.segments.insert(batch.segments.end(),
                          {-0.5f, -0.5f, 0.5f, 0.5f});
    batch.line_ids.push_back(1);
    {
        CorePlotting::LineBatchData::LineInfo info;
        info.entity_id = EntityId(1);
        info.trial_index = 0;
        info.first_segment = 0;
        info.segment_count = 1;
        batch.lines.push_back(info);
    }

    // Line 2: anti-diagonal ↘
    batch.segments.insert(batch.segments.end(),
                          {-0.5f, 0.5f, 0.5f, -0.5f});
    batch.line_ids.push_back(2);
    {
        CorePlotting::LineBatchData::LineInfo info;
        info.entity_id = EntityId(2);
        info.trial_index = 1;
        info.first_segment = 1;
        info.segment_count = 1;
        batch.lines.push_back(info);
    }

    batch.visibility_mask = {1, 1};
    batch.selection_mask = {0, 0};

    return batch;
}

/**
 * @brief Build N vertical lines evenly spaced in NDC x ∈ [-0.9, 0.9],
 *        each spanning y ∈ [-0.5, 0.5].
 */
static CorePlotting::LineBatchData makeVerticalLinesBatch(int num_lines)
{
    CorePlotting::LineBatchData batch;
    batch.canvas_width = 2.0f;
    batch.canvas_height = 2.0f;

    for (int i = 0; i < num_lines; ++i) {
        float const x = (num_lines == 1)
            ? 0.0f
            : -0.9f + 1.8f * static_cast<float>(i) /
                  static_cast<float>(num_lines - 1);

        batch.segments.insert(batch.segments.end(), {x, -0.5f, x, 0.5f});
        batch.line_ids.push_back(static_cast<std::uint32_t>(i + 1));

        CorePlotting::LineBatchData::LineInfo info;
        info.entity_id = EntityId(static_cast<std::uint64_t>(i + 1));
        info.trial_index = static_cast<std::uint32_t>(i);
        info.first_segment = static_cast<std::uint32_t>(i);
        info.segment_count = 1;
        batch.lines.push_back(info);

        batch.visibility_mask.push_back(1);
        batch.selection_mask.push_back(0);
    }

    return batch;
}

/**
 * @brief Build a multi-segment polyline in NDC space.
 *
 * Line 1: 3-segment zigzag across y = 0
 * Line 2: horizontal at y = 0.8 (away from the center)
 */
static CorePlotting::LineBatchData makePolylineBatch()
{
    CorePlotting::LineBatchData batch;
    batch.canvas_width = 2.0f;
    batch.canvas_height = 2.0f;

    // Line 1: zigzag  (-0.6,-0.3)→(-0.2,0.3)→(0.2,-0.3)→(0.6,0.3)
    batch.segments.insert(batch.segments.end(),
                          {-0.6f, -0.3f, -0.2f, 0.3f});
    batch.line_ids.push_back(1);
    batch.segments.insert(batch.segments.end(),
                          {-0.2f, 0.3f, 0.2f, -0.3f});
    batch.line_ids.push_back(1);
    batch.segments.insert(batch.segments.end(),
                          {0.2f, -0.3f, 0.6f, 0.3f});
    batch.line_ids.push_back(1);
    {
        CorePlotting::LineBatchData::LineInfo info;
        info.entity_id = EntityId(10);
        info.trial_index = 0;
        info.first_segment = 0;
        info.segment_count = 3;
        batch.lines.push_back(info);
    }

    // Line 2: short horizontal at y = 0.8
    batch.segments.insert(batch.segments.end(),
                          {-0.3f, 0.8f, 0.3f, 0.8f});
    batch.line_ids.push_back(2);
    {
        CorePlotting::LineBatchData::LineInfo info;
        info.entity_id = EntityId(20);
        info.trial_index = 1;
        info.first_segment = 3;
        info.segment_count = 1;
        batch.lines.push_back(info);
    }

    batch.visibility_mask = {1, 1};
    batch.selection_mask = {0, 0};

    return batch;
}

// ── Initialization ─────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — initialize from filesystem shader",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));
    REQUIRE(gpu.isAvailable());
}

// ── Cross pattern ──────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — cross pattern matches CPU",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeCrossBatch();
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Horizontal query through the center — should hit both diagonals
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.05f;
    query.mvp = glm::mat4(1.0f); // identity

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    REQUIRE(gpu_result.intersected_line_indices.size() == 2);
}

// ── Vertical lines with horizontal sweep ───────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — 10 vertical lines all hit",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    constexpr int N = 10;
    auto const batch = makeVerticalLinesBatch(N);
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Horizontal query across the middle — should hit all N lines
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.05f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    REQUIRE(gpu_result.intersected_line_indices.size() ==
            static_cast<std::size_t>(N));
}

// ── No intersection (near miss) ────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — query far from data returns empty",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeCrossBatch();
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Query well above the data with tiny tolerance
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.95f};
    query.end_ndc = {1.0f, 0.95f};
    query.tolerance = 0.001f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(cpu_result.intersected_line_indices.empty());
    REQUIRE(gpu_result.intersected_line_indices.empty());
}

// ── Visibility filtering ───────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — hidden lines not returned",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto batch = makeCrossBatch();
    batch.visibility_mask = {0, 1}; // hide line 1
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Horizontal query that would hit both lines if both were visible
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.05f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    // Only line 2 (index 1) should be hit
    REQUIRE(gpu_result.intersected_line_indices.size() == 1);
    REQUIRE(gpu_result.intersected_line_indices[0] == 1);
}

// ── Empty batch ────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — empty batch returns empty result",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    CorePlotting::LineBatchData empty;
    store.upload(empty);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.05f;
    query.mvp = glm::mat4(1.0f);

    auto const result = gpu.intersect(empty, query);
    REQUIRE(result.intersected_line_indices.empty());
}

// ── Multi-segment polyline ─────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — multi-segment polyline matches CPU",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makePolylineBatch();
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Horizontal query through y = 0 — should hit the zigzag (line 1)
    // but miss the horizontal at y = 0.8 (line 2)
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.02f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    // The zigzag crosses y = 0 multiple times — line 1 should be hit
    REQUIRE_FALSE(gpu_result.intersected_line_indices.empty());
}

// ── Larger batch (100 lines) ───────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — 100 vertical lines matches CPU",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    constexpr int N = 100;
    auto const batch = makeVerticalLinesBatch(N);
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.02f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    REQUIRE(gpu_result.intersected_line_indices.size() ==
            static_cast<std::size_t>(N));
}

// ── Partial selection: only left half ──────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "ComputeShaderIntersector — partial sweep selects subset",
                 "[PlottingOpenGL][ComputeShaderIntersector]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    constexpr int N = 10;
    auto const batch = makeVerticalLinesBatch(N);
    store.upload(batch);

    PlottingOpenGL::ComputeShaderIntersector gpu(store);
    REQUIRE(gpu.initialize(shaderPath()));

    CorePlotting::CpuLineBatchIntersector cpu;

    // Query only the left half: x ∈ [-1, 0]
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {0.0f, 0.0f};
    query.tolerance = 0.02f;
    query.mvp = glm::mat4(1.0f);

    auto const cpu_result = cpu.intersect(batch, query);
    auto const gpu_result = gpu.intersect(batch, query);

    REQUIRE(sorted(cpu_result) == sorted(gpu_result));
    // Should hit roughly half the lines (those at x ≤ 0)
    REQUIRE(gpu_result.intersected_line_indices.size() < static_cast<std::size_t>(N));
    REQUIRE_FALSE(gpu_result.intersected_line_indices.empty());
}
