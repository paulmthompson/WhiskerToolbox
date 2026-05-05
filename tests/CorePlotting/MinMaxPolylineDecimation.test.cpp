/**
 * @file MinMaxPolylineDecimation.test.cpp
 * @brief Unit tests for `CorePlotting::decimatePolyLineBatchMinMax`.
 */

#include "CorePlotting/LineDecimation/MinMaxPolylineDecimation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>

namespace {

[[nodiscard]] CorePlotting::RenderablePolyLineBatch makeStrip(
        std::vector<float> verts,
        glm::vec4 const color = {1.0f, 0.0f, 0.0f, 1.0f}) {
    CorePlotting::RenderablePolyLineBatch b;
    b.vertices = std::move(verts);
    b.line_start_indices.push_back(0);
    b.line_vertex_counts.push_back(static_cast<int32_t>(b.vertices.size() / 2U));
    b.global_color = color;
    return b;
}

}// namespace

TEST_CASE("decimatePolyLineBatchMinMax is identity when bucket_count is zero",
          "[CorePlotting][MinMaxPolylineDecimation]") {
    std::vector<float> v;
    for (int i = 0; i < 200; ++i) {
        float const x = static_cast<float>(i);
        float const y = std::sin(x * 0.1f);
        v.push_back(x);
        v.push_back(y);
    }
    auto const in = makeStrip(std::move(v));
    auto const out = CorePlotting::decimatePolyLineBatchMinMax(in, CorePlotting::MinMaxDecimationParams{0});
    REQUIRE(out.vertices == in.vertices);
    REQUIRE(out.line_start_indices == in.line_start_indices);
    REQUIRE(out.line_vertex_counts == in.line_vertex_counts);
}

TEST_CASE("decimatePolyLineBatchMinMax preserves first and last vertices",
          "[CorePlotting][MinMaxPolylineDecimation]") {
    std::vector<float> v;
    for (int i = 0; i < 500; ++i) {
        float const x = static_cast<float>(i);
        float const y = static_cast<float>(i % 17) * 0.1f;
        v.push_back(x);
        v.push_back(y);
    }
    auto const in = makeStrip(std::move(v));
    auto const out = CorePlotting::decimatePolyLineBatchMinMax(in, CorePlotting::MinMaxDecimationParams{64});
    REQUIRE(!out.vertices.empty());
    REQUIRE(out.vertices[0] == in.vertices[0]);
    REQUIRE(out.vertices[1] == in.vertices[1]);
    size_t const n = in.vertices.size();
    REQUIRE(out.vertices[out.vertices.size() - 2U] == in.vertices[n - 2U]);
    REQUIRE(out.vertices[out.vertices.size() - 1U] == in.vertices[n - 1U]);
}

TEST_CASE("decimatePolyLineBatchMinMax reduces vertex count for dense strips",
          "[CorePlotting][MinMaxPolylineDecimation]") {
    std::vector<float> v;
    for (int i = 0; i < 10'000; ++i) {
        float const x = static_cast<float>(i);
        v.push_back(x);
        v.push_back(std::sin(x * 0.02f));
    }
    auto const in = makeStrip(std::move(v));
    auto const out = CorePlotting::decimatePolyLineBatchMinMax(in, CorePlotting::MinMaxDecimationParams{256});
    REQUIRE(out.vertices.size() < in.vertices.size());
    REQUIRE(out.vertices.size() % 2U == 0U);
}

TEST_CASE("decimatePolyLineBatchMinMax handles two independent lines",
          "[CorePlotting][MinMaxPolylineDecimation]") {
    CorePlotting::RenderablePolyLineBatch in;
    // Line 0: 0..99
    for (int i = 0; i < 100; ++i) {
        in.vertices.push_back(static_cast<float>(i));
        in.vertices.push_back(static_cast<float>(i % 5));
    }
    // Line 1: 200..399 in x
    for (int i = 0; i < 100; ++i) {
        in.vertices.push_back(200.0f + static_cast<float>(i));
        in.vertices.push_back(static_cast<float>((i * 3) % 7));
    }
    in.line_start_indices.push_back(0);
    in.line_vertex_counts.push_back(100);
    in.line_start_indices.push_back(100);
    in.line_vertex_counts.push_back(100);

    auto const out = CorePlotting::decimatePolyLineBatchMinMax(in, CorePlotting::MinMaxDecimationParams{16});
    REQUIRE(out.line_start_indices.size() == 2U);
    REQUIRE(out.line_vertex_counts.size() == 2U);
    REQUIRE(out.line_start_indices[0] == 0);
    REQUIRE(out.line_start_indices[1] == out.line_vertex_counts[0]);
    int const total_v = out.line_vertex_counts[0] + out.line_vertex_counts[1];
    REQUIRE(static_cast<int>(out.vertices.size()) == total_v * 2);
}

TEST_CASE("decimatePolyLineBatchMinMax returns input on topology mismatch",
          "[CorePlotting][MinMaxPolylineDecimation]") {
    CorePlotting::RenderablePolyLineBatch in;
    in.vertices = {0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 2.0f};
    in.line_start_indices.push_back(0);
    // missing line_vertex_counts on purpose
    auto const out = CorePlotting::decimatePolyLineBatchMinMax(in, CorePlotting::MinMaxDecimationParams{8});
    REQUIRE(out.vertices == in.vertices);
}
