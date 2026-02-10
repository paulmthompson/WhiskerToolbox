/**
 * @file BatchLineStore.test.cpp
 * @brief Integration tests for BatchLineStore GPU buffer management
 *
 * Verifies that LineBatchData can be uploaded to GPU SSBOs via BatchLineStore
 * and that the CPU mirror, segment packing, and partial mask updates all
 * behave correctly.  Requires a headless OpenGL 4.3 context.
 */
#include "HeadlessGLFixture.hpp"

#include "PlottingOpenGL/LineBatch/BatchLineStore.hpp"

#include "CorePlotting/LineBatch/LineBatchData.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QOpenGLExtraFunctions>

#include <cstdint>
#include <cstring>
#include <vector>

// ── Test data helpers ──────────────────────────────────────────────────

/**
 * @brief Build a small, deterministic LineBatchData for testing.
 *
 * Line 1 (entity 100): two segments  (0,0)→(1,0)→(1,1)
 * Line 2 (entity 200): one segment   (2,2)→(3,3)
 */
static CorePlotting::LineBatchData makeTestBatch()
{
    CorePlotting::LineBatchData batch;
    batch.canvas_width = 640.0f;
    batch.canvas_height = 480.0f;

    // Segment 0: line 1
    batch.segments.insert(batch.segments.end(), {0.0f, 0.0f, 1.0f, 0.0f});
    batch.line_ids.push_back(1);

    // Segment 1: line 1
    batch.segments.insert(batch.segments.end(), {1.0f, 0.0f, 1.0f, 1.0f});
    batch.line_ids.push_back(1);

    // Segment 2: line 2
    batch.segments.insert(batch.segments.end(), {2.0f, 2.0f, 3.0f, 3.0f});
    batch.line_ids.push_back(2);

    CorePlotting::LineBatchData::LineInfo info1;
    info1.entity_id = EntityId(100);
    info1.trial_index = 0;
    info1.first_segment = 0;
    info1.segment_count = 2;
    batch.lines.push_back(info1);

    CorePlotting::LineBatchData::LineInfo info2;
    info2.entity_id = EntityId(200);
    info2.trial_index = 1;
    info2.first_segment = 2;
    info2.segment_count = 1;
    batch.lines.push_back(info2);

    batch.visibility_mask = {1, 1};
    batch.selection_mask = {0, 0};

    return batch;
}

// ── Initialization ─────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — initialize and cleanup",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE_FALSE(store.isInitialized());

    REQUIRE(store.initialize());
    REQUIRE(store.isInitialized());

    // Idempotent
    REQUIRE(store.initialize());

    store.cleanup();
    REQUIRE_FALSE(store.isInitialized());
}

// ── Upload — CPU mirror ────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — upload populates CPU mirror",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeTestBatch();
    store.upload(batch);

    auto const & cpu = store.cpuData();
    REQUIRE(cpu.numSegments() == 3);
    REQUIRE(cpu.numLines() == 2);
    REQUIRE(cpu.canvas_width == 640.0f);
    REQUIRE(cpu.canvas_height == 480.0f);
    REQUIRE(store.cpuData().lines[0].entity_id == EntityId(100));
    REQUIRE(store.cpuData().lines[0].segment_count == 2);
    REQUIRE(store.cpuData().lines[1].entity_id == EntityId(200));
    REQUIRE(store.cpuData().lines[1].segment_count == 1);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — convenience accessors after upload",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeTestBatch();
    store.upload(batch);

    REQUIRE(store.numSegments() == 3);
    REQUIRE(store.numLines() == 2);
}

// ── Upload — GPU buffer IDs ────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — upload creates valid GPU buffers",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeTestBatch();
    store.upload(batch);

    REQUIRE(store.segmentsBufferId() != 0);
    REQUIRE(store.visibilityBufferId() != 0);
    REQUIRE(store.selectionMaskBufferId() != 0);
    REQUIRE(store.intersectionResultsBufferId() != 0);
    REQUIRE(store.intersectionCountBufferId() != 0);
}

// ── Upload — segment packing roundtrip ─────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — segments SSBO contains packed 5-float data",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    auto const batch = makeTestBatch();
    store.upload(batch);

    // The segments SSBO is packed as 5 floats per segment:
    //   {x1, y1, x2, y2, bitcast(line_id)}
    // 3 segments × 5 floats = 15 floats
    constexpr int expected_bytes = 3 * 5 * static_cast<int>(sizeof(float));

    auto * f = context()->functions();
    auto * ef = context()->extraFunctions();
    REQUIRE(ef != nullptr);

    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, store.segmentsBufferId());
    void * ptr = ef->glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, expected_bytes, GL_MAP_READ_BIT);
    REQUIRE(ptr != nullptr);

    auto const * data = static_cast<float const *>(ptr);

    // Segment 0: (0, 0) → (1, 0), line_id = 1
    REQUIRE(data[0] == 0.0f);
    REQUIRE(data[1] == 0.0f);
    REQUIRE(data[2] == 1.0f);
    REQUIRE(data[3] == 0.0f);
    std::uint32_t id0{};
    std::memcpy(&id0, &data[4], sizeof(float));
    REQUIRE(id0 == 1);

    // Segment 1: (1, 0) → (1, 1), line_id = 1
    REQUIRE(data[5] == 1.0f);
    REQUIRE(data[6] == 0.0f);
    REQUIRE(data[7] == 1.0f);
    REQUIRE(data[8] == 1.0f);
    std::uint32_t id1{};
    std::memcpy(&id1, &data[9], sizeof(float));
    REQUIRE(id1 == 1);

    // Segment 2: (2, 2) → (3, 3), line_id = 2
    REQUIRE(data[10] == 2.0f);
    REQUIRE(data[11] == 2.0f);
    REQUIRE(data[12] == 3.0f);
    REQUIRE(data[13] == 3.0f);
    std::uint32_t id2{};
    std::memcpy(&id2, &data[14], sizeof(float));
    REQUIRE(id2 == 2);

    ef->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// ── Partial mask updates ───────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — updateVisibilityMask updates CPU mirror",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeTestBatch());

    std::vector<std::uint32_t> new_vis = {0, 1};
    store.updateVisibilityMask(new_vis);

    REQUIRE(store.cpuData().visibility_mask[0] == 0);
    REQUIRE(store.cpuData().visibility_mask[1] == 1);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — updateSelectionMask updates CPU mirror",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeTestBatch());

    std::vector<std::uint32_t> new_sel = {0, 1};
    store.updateSelectionMask(new_sel);

    REQUIRE(store.cpuData().selection_mask[0] == 0);
    REQUIRE(store.cpuData().selection_mask[1] == 1);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — wrong-sized mask update is rejected",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeTestBatch());

    // Wrong size (3 instead of 2) — should be silently rejected
    std::vector<std::uint32_t> bad = {1, 1, 1};
    store.updateVisibilityMask(bad);

    // Original mask unchanged
    REQUIRE(store.cpuData().visibility_mask == std::vector<std::uint32_t>{1, 1});
}

// ── Edge cases ─────────────────────────────────────────────────────────

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — empty batch upload",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    CorePlotting::LineBatchData empty;
    store.upload(empty);

    REQUIRE(store.cpuData().empty());
    REQUIRE(store.numSegments() == 0);
    REQUIRE(store.numLines() == 0);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — re-upload replaces previous data",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());

    store.upload(makeTestBatch());
    REQUIRE(store.numLines() == 2);

    // Upload a smaller batch
    CorePlotting::LineBatchData batch2;
    batch2.segments = {0.0f, 0.0f, 5.0f, 5.0f};
    batch2.line_ids = {1};
    CorePlotting::LineBatchData::LineInfo info;
    info.entity_id = EntityId(42);
    info.trial_index = 0;
    info.first_segment = 0;
    info.segment_count = 1;
    batch2.lines.push_back(info);
    batch2.visibility_mask = {1};
    batch2.selection_mask = {0};
    batch2.canvas_width = 100.0f;

    store.upload(batch2);

    REQUIRE(store.numLines() == 1);
    REQUIRE(store.numSegments() == 1);
    REQUIRE(store.cpuData().lines[0].entity_id == EntityId(42));
    REQUIRE(store.cpuData().canvas_width == 100.0f);
}

TEST_CASE_METHOD(HeadlessGLFixture,
                 "BatchLineStore — resetIntersectionCount zeroes the counter",
                 "[PlottingOpenGL][BatchLineStore]")
{
    REQUIRE(isGLAvailable());

    PlottingOpenGL::BatchLineStore store;
    REQUIRE(store.initialize());
    store.upload(makeTestBatch());

    store.resetIntersectionCount();

    // Read back the counter SSBO — should be 0
    auto * f = context()->functions();
    auto * ef = context()->extraFunctions();
    REQUIRE(ef != nullptr);

    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, store.intersectionCountBufferId());
    void * ptr = ef->glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0,
        static_cast<int>(sizeof(std::uint32_t)), GL_MAP_READ_BIT);
    REQUIRE(ptr != nullptr);

    std::uint32_t count{};
    std::memcpy(&count, ptr, sizeof(count));
    REQUIRE(count == 0);

    ef->glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
