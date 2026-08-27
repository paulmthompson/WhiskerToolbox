/**
 * @file PolyLineRenderer.test.cpp
 * @brief Unit tests for PolyLineRenderer and StreamingPolyLineRenderer
 */

#include "HeadlessGLFixture.hpp"

#include "PlottingOpenGL/Renderers/PolyLineRenderer.hpp"
#include "PlottingOpenGL/Renderers/StreamingPolyLineRenderer.hpp"
#include "PlottingOpenGL/SceneRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>

#include <bit>
#include <vector>

using namespace PlottingOpenGL;

namespace {

CorePlotting::RenderablePolyLineBatch makeIntegerBatch(int32_t start_tick, size_t count, float y_val = 1.0f) {
    CorePlotting::RenderablePolyLineBatch batch;
    batch.vertices.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) {
        int32_t const t = start_tick + static_cast<int32_t>(i);
        batch.vertices.push_back(std::bit_cast<float>(t));
        batch.vertices.push_back(y_val);
    }
    batch.line_start_indices.push_back(0);
    batch.line_vertex_counts.push_back(static_cast<int32_t>(count));
    batch.global_color = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
    batch.thickness = 1.0f;
    batch.model_matrix = glm::mat4(1.0f);
    batch.is_integer_time = true;
    batch.view_start_time = start_tick;
    return batch;
}

}// anonymous namespace

TEST_CASE_METHOD(HeadlessGLFixture, "PolyLineRenderer: uploadBatches and multi-batch persistent VBO", "[PlottingOpenGL][PolyLineRenderer]") {
    REQUIRE(isGLAvailable());

    PolyLineRenderer renderer;
    REQUIRE(renderer.initialize());

    SECTION("uploadBatches combines multiple batches in a single pass") {
        std::vector<CorePlotting::RenderablePolyLineBatch> batches;
        batches.push_back(makeIntegerBatch(0, 100, 1.0f));
        batches.push_back(makeIntegerBatch(0, 200, 2.0f));
        batches.push_back(makeIntegerBatch(0, 300, 3.0f));

        renderer.uploadBatches(batches);
        REQUIRE(renderer.hasData());
        CHECK_FALSE(renderer.wasLastUploadIncremental());

        // Render smoke test
        glm::mat4 const view{1.0f};
        glm::mat4 const proj{1.0f};
        renderer.render(view, proj);

        // Upload new frame with same size — should reuse persistent buffer
        batches[0] = makeIntegerBatch(10, 100, 1.5f);
        batches[1] = makeIntegerBatch(10, 200, 2.5f);
        batches[2] = makeIntegerBatch(10, 300, 3.5f);

        renderer.uploadBatches(batches);
        REQUIRE(renderer.hasData());
        renderer.render(view, proj);

        renderer.clearData();
        REQUIRE_FALSE(renderer.hasData());
    }

    renderer.cleanup();
}

TEST_CASE_METHOD(HeadlessGLFixture, "SceneRenderer: uploadScene uses uploadBatches", "[PlottingOpenGL][SceneRenderer]") {
    REQUIRE(isGLAvailable());

    SceneRenderer scene_renderer;
    REQUIRE(scene_renderer.initialize());

    CorePlotting::RenderableScene scene;
    scene.poly_line_batches.push_back(makeIntegerBatch(0, 50, 1.0f));
    scene.poly_line_batches.push_back(makeIntegerBatch(0, 75, 2.0f));

    scene_renderer.uploadScene(scene);
    scene_renderer.render();

    scene_renderer.clearScene();
    scene_renderer.cleanup();
}

TEST_CASE_METHOD(HeadlessGLFixture, "StreamingPolyLineRenderer: incremental scrolling and sliding window", "[PlottingOpenGL][StreamingPolyLineRenderer]") {
    REQUIRE(isGLAvailable());

    StreamingPolyLineRenderer renderer("", 3.0f);
    REQUIRE(renderer.initialize());
    renderer.setTimingEnabled(true);

    SECTION("Initial upload triggers full upload") {
        auto batch = makeIntegerBatch(1000, 500, 0.5f);
        renderer.updateData(batch);
        REQUIRE(renderer.hasData());

        auto const & stats = renderer.getTimingStats();
        CHECK(stats.was_full_reupload);
        CHECK(stats.bytes_uploaded == 500 * 2 * sizeof(float));

        glm::mat4 const view{1.0f};
        glm::mat4 const proj{1.0f};
        renderer.render(view, proj);
    }

    SECTION("Forward scrolling performs incremental sub-buffer upload") {
        auto batch1 = makeIntegerBatch(1000, 500, 0.5f);
        renderer.updateData(batch1);

        // Scroll forward by 10 samples
        auto batch2 = makeIntegerBatch(1010, 500, 0.5f);
        renderer.updateData(batch2);

        auto const & stats = renderer.getTimingStats();
        CHECK_FALSE(stats.was_full_reupload);
        CHECK(stats.bytes_uploaded == 10 * 2 * sizeof(float));
        CHECK(renderer.getCacheHitRatio() > 0.0f);

        glm::mat4 const view{1.0f};
        glm::mat4 const proj{1.0f};
        renderer.render(view, proj);
    }

    SECTION("Backward scrolling performs incremental sub-buffer upload") {
        // Start with offset so backward scroll fits
        auto batch1 = makeIntegerBatch(1000, 500, 0.5f);
        renderer.updateData(batch1);

        // Scroll forward first by 50 to create head room
        auto batch2 = makeIntegerBatch(1050, 500, 0.5f);
        renderer.updateData(batch2);

        // Scroll backward by 10 samples
        auto batch3 = makeIntegerBatch(1040, 500, 0.5f);
        renderer.updateData(batch3);

        auto const & stats = renderer.getTimingStats();
        CHECK_FALSE(stats.was_full_reupload);
        CHECK(stats.bytes_uploaded == 10 * 2 * sizeof(float));

        glm::mat4 const view{1.0f};
        glm::mat4 const proj{1.0f};
        renderer.render(view, proj);
    }

    SECTION("Large seek jump triggers full re-upload") {
        auto batch1 = makeIntegerBatch(1000, 500, 0.5f);
        renderer.updateData(batch1);

        // Jump far away (seek to sample 500,000)
        auto batch2 = makeIntegerBatch(500000, 500, 0.5f);
        renderer.updateData(batch2);

        auto const & stats = renderer.getTimingStats();
        CHECK(stats.was_full_reupload);
        CHECK(stats.bytes_uploaded == 500 * 2 * sizeof(float));
    }

    renderer.cleanup();
}
