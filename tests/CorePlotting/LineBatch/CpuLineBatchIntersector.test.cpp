/**
 * @file CpuLineBatchIntersector.test.cpp
 * @brief Unit tests for the CPU brute-force line batch intersector.
 *
 * Covers the test cases outlined in the roadmap:
 *  1. Perpendicular cross
 *  2. Parallel lines
 *  3. Near miss
 *  4. Tolerance boundary
 *  5. Visibility filtering
 *  6. Empty batch
 *  7. Single-segment lines
 *  8. Large batch performance
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/LineBatch/CpuLineBatchIntersector.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

using namespace CorePlotting;

// ── Helpers ────────────────────────────────────────────────────────────

namespace {

/**
 * Build a LineBatchData from a vector of line definitions.
 * Each line is specified as a flat sequence of (x,y) pairs.
 * The MVP is identity (world == NDC) for simplicity.
 */
struct LineDef {
    std::vector<glm::vec2> points;
};

LineBatchData makeBatch(std::vector<LineDef> const & defs, bool all_visible = true)
{
    LineBatchData batch;
    std::uint32_t line_id = 0;

    for (auto const & def : defs) {
        if (def.points.size() < 2) continue;
        ++line_id;
        auto const first_seg = batch.numSegments();
        std::uint32_t seg_count = 0;

        for (std::size_t i = 0; i + 1 < def.points.size(); ++i) {
            batch.segments.push_back(def.points[i].x);
            batch.segments.push_back(def.points[i].y);
            batch.segments.push_back(def.points[i + 1].x);
            batch.segments.push_back(def.points[i + 1].y);
            batch.line_ids.push_back(line_id);
            ++seg_count;
        }

        batch.lines.push_back(LineBatchData::LineInfo{
            .entity_id = EntityId{line_id},
            .trial_index = 0,
            .first_segment = first_seg,
            .segment_count = seg_count});
    }

    batch.visibility_mask.assign(batch.numLines(), all_visible ? 1 : 0);
    batch.selection_mask.assign(batch.numLines(), 0);
    return batch;
}

/// Default query: identity MVP, reasonable tolerance.
LineIntersectionQuery makeQuery(glm::vec2 start, glm::vec2 end,
                                float tolerance = 0.05f)
{
    return LineIntersectionQuery{
        .start_ndc = start,
        .end_ndc = end,
        .tolerance = tolerance,
        .mvp = glm::mat4{1.0f}};
}

bool resultContains(LineIntersectionResult const & r, LineBatchIndex idx)
{
    return std::ranges::find(r.intersected_line_indices, idx) !=
           r.intersected_line_indices.end();
}

} // namespace

// ── Tests ──────────────────────────────────────────────────────────────

TEST_CASE("CpuLineBatchIntersector — empty batch", "[CorePlotting][LineBatch]")
{
    CpuLineBatchIntersector cpu;
    LineBatchData batch; // empty
    auto result = cpu.intersect(batch, makeQuery({-1, 0}, {1, 0}));
    REQUIRE(result.intersected_line_indices.empty());
}

TEST_CASE("CpuLineBatchIntersector — perpendicular cross", "[CorePlotting][LineBatch]")
{
    // Two lines forming an X:
    //   Line 0: horizontal from (-0.5,0) to (0.5,0)
    //   Line 1: vertical from (0,-0.5) to (0,0.5)
    // Query: horizontal through center from (-1,0) to (1,0) — should hit both.
    auto batch = makeBatch({
        {.points = {{-0.5f, 0.0f}, {0.5f, 0.0f}}},
        {.points = {{0.0f, -0.5f}, {0.0f, 0.5f}}}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.0f}, {1.0f, 0.0f}));

    REQUIRE(result.intersected_line_indices.size() == 2);
    CHECK(resultContains(result, 0));
    CHECK(resultContains(result, 1));
}

TEST_CASE("CpuLineBatchIntersector — parallel lines", "[CorePlotting][LineBatch]")
{
    // 5 vertical lines at x = -0.4, -0.2, 0.0, 0.2, 0.4
    // Query: horizontal line from (-1,0) to (1,0) → should hit all 5.
    std::vector<LineDef> defs;
    for (int i = -2; i <= 2; ++i) {
        float const x = static_cast<float>(i) * 0.2f;
        defs.push_back({.points = {{x, -0.5f}, {x, 0.5f}}});
    }

    auto batch = makeBatch(defs);
    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.0f}, {1.0f, 0.0f}));

    REQUIRE(result.intersected_line_indices.size() == 5);
}

TEST_CASE("CpuLineBatchIntersector — near miss", "[CorePlotting][LineBatch]")
{
    // Line at (0, 0.15) → (1, 0.15)
    // Query line at y=0 from (0,0) → (1,0)
    // Tolerance = 0.05 → distance 0.15 > 0.05 → no hit
    auto batch = makeBatch({
        {.points = {{0.0f, 0.15f}, {1.0f, 0.15f}}}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({0.0f, 0.0f}, {1.0f, 0.0f}, 0.05f));

    REQUIRE(result.intersected_line_indices.empty());
}

TEST_CASE("CpuLineBatchIntersector — tolerance boundary", "[CorePlotting][LineBatch]")
{
    // Line at (0, 0.05) → (1, 0.05)
    // Query line at y=0 from (0,0) → (1,0)
    // Tolerance = 0.05 → distance exactly 0.05 → should be selected
    auto batch = makeBatch({
        {.points = {{0.0f, 0.05f}, {1.0f, 0.05f}}}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({0.0f, 0.0f}, {1.0f, 0.0f}, 0.05f));

    REQUIRE(result.intersected_line_indices.size() == 1);
    CHECK(resultContains(result, 0));
}

TEST_CASE("CpuLineBatchIntersector — visibility filtering", "[CorePlotting][LineBatch]")
{
    // Two crossing lines, but one is hidden.
    auto batch = makeBatch({
        {.points = {{-0.5f, 0.0f}, {0.5f, 0.0f}}},
        {.points = {{0.0f, -0.5f}, {0.0f, 0.5f}}}
    });

    // Hide line 1 (index 1 in visibility_mask)
    batch.visibility_mask[1] = 0;

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.0f}, {1.0f, 0.0f}));

    REQUIRE(result.intersected_line_indices.size() == 1);
    CHECK(resultContains(result, 0));
    CHECK_FALSE(resultContains(result, 1));
}

TEST_CASE("CpuLineBatchIntersector — single-segment lines", "[CorePlotting][LineBatch]")
{
    // A single-segment line crossing the query
    auto batch = makeBatch({
        {.points = {{0.0f, -0.5f}, {0.0f, 0.5f}}}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.0f}, {1.0f, 0.0f}));

    REQUIRE(result.intersected_line_indices.size() == 1);
    CHECK(resultContains(result, 0));
}

TEST_CASE("CpuLineBatchIntersector — multi-segment polyline", "[CorePlotting][LineBatch]")
{
    // A zigzag line with 3 segments:
    // (0,0)→(0.5,0.5)→(1.0,0)→(1.5,0.5)
    // Query: horizontal at y=0.25 from (-1,0.25) to (2,0.25)
    // Should hit the single logical line (the zigzag).
    auto batch = makeBatch({
        {.points = {
            {0.0f, 0.0f},
            {0.5f, 0.5f},
            {1.0f, 0.0f},
            {1.5f, 0.5f}
        }}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.25f}, {2.0f, 0.25f}));

    REQUIRE(result.intersected_line_indices.size() == 1);
    CHECK(resultContains(result, 0));
}

TEST_CASE("CpuLineBatchIntersector — de-duplicates line hits", "[CorePlotting][LineBatch]")
{
    // A multi-segment zigzag that the query crosses multiple times.
    // Should still only appear once in results.
    auto batch = makeBatch({
        {.points = {
            {0.0f, -1.0f},
            {0.5f, 1.0f},
            {1.0f, -1.0f},
            {1.5f, 1.0f}
        }}
    });

    CpuLineBatchIntersector cpu;
    auto result = cpu.intersect(batch, makeQuery({-0.5f, 0.0f}, {2.0f, 0.0f}));

    REQUIRE(result.intersected_line_indices.size() == 1);
}

TEST_CASE("CpuLineBatchIntersector — MVP transform", "[CorePlotting][LineBatch]")
{
    // Line in world space: (100, 200) → (300, 200)
    // MVP that scales by 1/200 and translates so center maps to NDC origin.
    // After transform: ~(-0.5, 0) → (0.5, 0)
    //
    // Query in NDC space: vertical line at x=0 from (-1,-1) to (-1,1)... wait,
    // let's keep it simple: identity MVP, world==NDC, just verify it works.
    // Non-identity MVP: scale world coords [0,100] → NDC [-1,1]
    glm::mat4 mvp{1.0f};
    mvp[0][0] = 2.0f / 100.0f;  // scale x: [0,100] → [0,2] → shift -1
    mvp[1][1] = 2.0f / 100.0f;
    mvp[3][0] = -1.0f;          // translate so 0→-1, 100→1
    mvp[3][1] = -1.0f;

    LineBatchData batch;
    // Vertical line in world space at x=50, y=[0,100]
    batch.segments = {50.0f, 0.0f, 50.0f, 100.0f};
    batch.line_ids = {1};
    batch.lines.push_back(LineBatchData::LineInfo{
        .entity_id = EntityId{1},
        .trial_index = 0,
        .first_segment = 0,
        .segment_count = 1});
    batch.visibility_mask = {1};
    batch.selection_mask = {0};

    CpuLineBatchIntersector cpu;

    // Query: horizontal across NDC center
    LineIntersectionQuery query;
    query.start_ndc = {-1.0f, 0.0f};
    query.end_ndc = {1.0f, 0.0f};
    query.tolerance = 0.05f;
    query.mvp = mvp;

    auto result = cpu.intersect(batch, query);

    // After MVP, world (50,0)→(0,-1), world (50,100)→(0,1) — vertical centerline.
    // Query horizontal at y=0 → intersects.
    REQUIRE(result.intersected_line_indices.size() == 1);
    CHECK(resultContains(result, 0));
}

TEST_CASE("CpuLineBatchIntersector — large batch performance", "[CorePlotting][LineBatch][.performance]")
{
    // 50,000 single-segment vertical lines spread across [-1,1]
    constexpr std::uint32_t N = 50'000;
    std::vector<LineDef> defs;
    defs.reserve(N);

    for (std::uint32_t i = 0; i < N; ++i) {
        float const x = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(N - 1);
        defs.push_back({.points = {{x, -1.0f}, {x, 1.0f}}});
    }

    auto batch = makeBatch(defs);
    CpuLineBatchIntersector cpu;

    auto const start = std::chrono::high_resolution_clock::now();
    auto result = cpu.intersect(batch, makeQuery({-1.0f, 0.0f}, {1.0f, 0.0f}));
    auto const end = std::chrono::high_resolution_clock::now();

    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // All 50k lines should be hit
    REQUIRE(result.intersected_line_indices.size() == N);

    // Should complete in under 100ms on modern hardware (roadmap says <10ms target)
    WARN("CPU intersector: " << N << " lines in " << ms << " ms");
    CHECK(ms < 100);
}

// ── Low-level helper tests ─────────────────────────────────────────────

TEST_CASE("distancePointToSegment — basics", "[CorePlotting][LineBatch]")
{
    SECTION("Point on segment") {
        float d = CpuLineBatchIntersector::distancePointToSegment(
            {0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f});
        REQUIRE_THAT(d, Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    }

    SECTION("Point perpendicular to segment midpoint") {
        float d = CpuLineBatchIntersector::distancePointToSegment(
            {0.5f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f});
        REQUIRE_THAT(d, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }

    SECTION("Point beyond segment end") {
        float d = CpuLineBatchIntersector::distancePointToSegment(
            {2.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f});
        REQUIRE_THAT(d, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }

    SECTION("Degenerate segment (zero length)") {
        float d = CpuLineBatchIntersector::distancePointToSegment(
            {1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f});
        REQUIRE_THAT(d, Catch::Matchers::WithinAbs(1.0f, 1e-5f));
    }
}

TEST_CASE("segmentsIntersect — basics", "[CorePlotting][LineBatch]")
{
    SECTION("Crossing segments") {
        bool hit = CpuLineBatchIntersector::segmentsIntersect(
            {-1.0f, 0.0f}, {1.0f, 0.0f},
            {0.0f, -1.0f}, {0.0f, 1.0f},
            0.0f);
        REQUIRE(hit);
    }

    SECTION("Parallel non-touching") {
        bool hit = CpuLineBatchIntersector::segmentsIntersect(
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 1.0f},
            0.0f);
        REQUIRE_FALSE(hit);
    }

    SECTION("Parallel within tolerance") {
        bool hit = CpuLineBatchIntersector::segmentsIntersect(
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {0.0f, 0.04f}, {1.0f, 0.04f},
            0.05f);
        REQUIRE(hit);
    }

    SECTION("T-junction") {
        bool hit = CpuLineBatchIntersector::segmentsIntersect(
            {0.0f, 0.0f}, {1.0f, 0.0f},
            {0.5f, 0.0f}, {0.5f, 1.0f},
            0.0f);
        REQUIRE(hit);
    }
}
