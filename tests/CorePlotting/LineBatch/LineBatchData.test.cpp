/**
 * @file LineBatchData.test.cpp
 * @brief Unit tests for LineBatchData construction, queries, and clear.
 */
#include <catch2/catch_test_macros.hpp>

#include "CorePlotting/LineBatch/LineBatchData.hpp"

using namespace CorePlotting;

namespace {

/// Build a minimal batch with the given number of single-segment lines.
LineBatchData makeBatch(std::uint32_t num_lines)
{
    LineBatchData batch;
    for (std::uint32_t i = 0; i < num_lines; ++i) {
        float const x = static_cast<float>(i);
        // segment: (x,0) → (x,1)
        batch.segments.push_back(x);
        batch.segments.push_back(0.0f);
        batch.segments.push_back(x);
        batch.segments.push_back(1.0f);

        batch.line_ids.push_back(i + 1); // 1-based
        batch.lines.push_back(LineBatchData::LineInfo{
            .entity_id = EntityId{i},
            .trial_index = 0,
            .first_segment = i,
            .segment_count = 1});
    }
    batch.visibility_mask.assign(num_lines, 1);
    batch.selection_mask.assign(num_lines, 0);
    return batch;
}

} // namespace

TEST_CASE("LineBatchData — default state", "[CorePlotting][LineBatch]")
{
    LineBatchData batch;
    REQUIRE(batch.numSegments() == 0);
    REQUIRE(batch.numLines() == 0);
    REQUIRE(batch.empty());
    REQUIRE(batch.canvas_width == 1.0f);
    REQUIRE(batch.canvas_height == 1.0f);
}

TEST_CASE("LineBatchData — counts after population", "[CorePlotting][LineBatch]")
{
    auto batch = makeBatch(5);
    REQUIRE(batch.numSegments() == 5);
    REQUIRE(batch.numLines() == 5);
    REQUIRE_FALSE(batch.empty());
}

TEST_CASE("LineBatchData — clear resets everything", "[CorePlotting][LineBatch]")
{
    auto batch = makeBatch(10);
    batch.canvas_width = 800.0f;
    batch.canvas_height = 600.0f;

    batch.clear();

    REQUIRE(batch.numSegments() == 0);
    REQUIRE(batch.numLines() == 0);
    REQUIRE(batch.empty());
    REQUIRE(batch.canvas_width == 1.0f);
    REQUIRE(batch.canvas_height == 1.0f);
    REQUIRE(batch.visibility_mask.empty());
    REQUIRE(batch.selection_mask.empty());
}

TEST_CASE("LineBatchData — segment data layout", "[CorePlotting][LineBatch]")
{
    auto batch = makeBatch(1);
    REQUIRE(batch.segments.size() == 4);

    // Segment: (0,0) → (0,1)
    CHECK(batch.segments[0] == 0.0f);
    CHECK(batch.segments[1] == 0.0f);
    CHECK(batch.segments[2] == 0.0f);
    CHECK(batch.segments[3] == 1.0f);

    CHECK(batch.line_ids[0] == 1); // 1-based
}

TEST_CASE("LineBatchData — multi-segment line", "[CorePlotting][LineBatch]")
{
    LineBatchData batch;

    // A three-point polyline produces 2 segments
    // Points: (0,0) → (1,1) → (2,0)
    batch.segments = {0, 0, 1, 1, 1, 1, 2, 0};
    batch.line_ids = {1, 1};
    batch.lines.push_back(LineBatchData::LineInfo{
        .entity_id = EntityId{42},
        .trial_index = 0,
        .first_segment = 0,
        .segment_count = 2});
    batch.visibility_mask = {1};
    batch.selection_mask = {0};

    REQUIRE(batch.numSegments() == 2);
    REQUIRE(batch.numLines() == 1);
    CHECK(batch.lines[0].entity_id == EntityId{42});
    CHECK(batch.lines[0].segment_count == 2);
}
