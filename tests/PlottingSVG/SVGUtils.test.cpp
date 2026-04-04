/**
 * @file SVGUtils.test.cpp
 * @brief Tests for PlottingSVG coordinate transform and color conversion utilities.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/SVGUtils.hpp"

#include <glm/glm.hpp>

using Catch::Matchers::Equals;
using Catch::Matchers::WithinAbs;

// ============================================================================
// transformVertexToSVG
// ============================================================================

TEST_CASE("transformVertexToSVG identity MVP maps NDC origin to canvas center",
          "[PlottingSVG][SVGUtils]") {
    glm::mat4 const identity{1.0f};
    glm::vec4 const origin{0.0f, 0.0f, 0.0f, 1.0f};

    auto const result = PlottingSVG::transformVertexToSVG(origin, identity, 800, 600);

    REQUIRE_THAT(result.x, WithinAbs(400.0, 0.01));
    REQUIRE_THAT(result.y, WithinAbs(300.0, 0.01));
}

TEST_CASE("transformVertexToSVG identity MVP maps NDC corners to canvas corners",
          "[PlottingSVG][SVGUtils]") {
    glm::mat4 const identity{1.0f};
    int const W = 1920;
    int const H = 1080;

    SECTION("bottom-left NDC (-1, -1) maps to canvas (0, H)") {
        auto const result = PlottingSVG::transformVertexToSVG(
                glm::vec4{-1.0f, -1.0f, 0.0f, 1.0f}, identity, W, H);
        REQUIRE_THAT(result.x, WithinAbs(0.0, 0.01));
        REQUIRE_THAT(result.y, WithinAbs(static_cast<double>(H), 0.01));
    }

    SECTION("top-right NDC (+1, +1) maps to canvas (W, 0)") {
        auto const result = PlottingSVG::transformVertexToSVG(
                glm::vec4{1.0f, 1.0f, 0.0f, 1.0f}, identity, W, H);
        REQUIRE_THAT(result.x, WithinAbs(static_cast<double>(W), 0.01));
        REQUIRE_THAT(result.y, WithinAbs(0.0, 0.01));
    }

    SECTION("top-left NDC (-1, +1) maps to canvas (0, 0)") {
        auto const result = PlottingSVG::transformVertexToSVG(
                glm::vec4{-1.0f, 1.0f, 0.0f, 1.0f}, identity, W, H);
        REQUIRE_THAT(result.x, WithinAbs(0.0, 0.01));
        REQUIRE_THAT(result.y, WithinAbs(0.0, 0.01));
    }
}

TEST_CASE("transformVertexToSVG applies MVP before mapping",
          "[PlottingSVG][SVGUtils]") {
    // Scale x by 2: vertex (0.25, 0, 0, 1) should map like NDC (0.5, 0)
    glm::mat4 mvp{1.0f};
    mvp[0][0] = 2.0f;

    auto const result = PlottingSVG::transformVertexToSVG(
            glm::vec4{0.25f, 0.0f, 0.0f, 1.0f}, mvp, 100, 100);

    // NDC x = 0.5 → svg_x = 100 * (0.5 + 1) / 2 = 75
    // NDC y = 0   → svg_y = 100 * (1 - 0) / 2 = 50
    REQUIRE_THAT(result.x, WithinAbs(75.0, 0.01));
    REQUIRE_THAT(result.y, WithinAbs(50.0, 0.01));
}

TEST_CASE("transformVertexToSVG perspective divide with non-unit w",
          "[PlottingSVG][SVGUtils]") {
    glm::mat4 const identity{1.0f};
    // Vertex with w=2: after divide, NDC = (0.5, 0.5, 0)
    glm::vec4 const vertex{1.0f, 1.0f, 0.0f, 2.0f};

    auto const result = PlottingSVG::transformVertexToSVG(vertex, identity, 200, 200);

    // NDC x = 0.5 → svg_x = 200*(0.5+1)/2 = 150
    // NDC y = 0.5 → svg_y = 200*(1-0.5)/2 = 50
    REQUIRE_THAT(result.x, WithinAbs(150.0, 0.01));
    REQUIRE_THAT(result.y, WithinAbs(50.0, 0.01));
}

TEST_CASE("transformVertexToSVG near-zero w skips divide",
          "[PlottingSVG][SVGUtils]") {
    glm::mat4 const identity{1.0f};
    // w ~0: divide is skipped, raw ndc.x/y used
    glm::vec4 const vertex{0.5f, -0.5f, 0.0f, 1e-8f};

    auto const result = PlottingSVG::transformVertexToSVG(vertex, identity, 100, 100);

    // No divide: ndc.x = 0.5, ndc.y = -0.5 (from identity * vertex without w-divide)
    // Actually: identity * (0.5, -0.5, 0, 1e-8) = (0.5, -0.5, 0, 1e-8)
    // |w| < 1e-6 → no divide
    // svg_x = 100*(0.5+1)/2 = 75, svg_y = 100*(1+0.5)/2 = 75
    REQUIRE_THAT(result.x, WithinAbs(75.0, 0.01));
    REQUIRE_THAT(result.y, WithinAbs(75.0, 0.01));
}

// ============================================================================
// colorToSVGHex
// ============================================================================

TEST_CASE("colorToSVGHex converts pure colors correctly",
          "[PlottingSVG][SVGUtils]") {
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{1.0f, 0.0f, 0.0f, 1.0f}) == "#FF0000");
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{0.0f, 1.0f, 0.0f, 1.0f}) == "#00FF00");
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}) == "#0000FF");
}

TEST_CASE("colorToSVGHex converts black and white",
          "[PlottingSVG][SVGUtils]") {
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}) == "#000000");
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}) == "#FFFFFF");
}

TEST_CASE("colorToSVGHex clamps out-of-range values",
          "[PlottingSVG][SVGUtils]") {
    // Negative values clamped to 0
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{-0.5f, 0.0f, 0.0f, 1.0f}) == "#000000");
    // Values > 1 clamped to 255
    REQUIRE(PlottingSVG::colorToSVGHex(glm::vec4{2.0f, 2.0f, 2.0f, 1.0f}) == "#FFFFFF");
}

TEST_CASE("colorToSVGHex ignores alpha channel",
          "[PlottingSVG][SVGUtils]") {
    auto const hex0 = PlottingSVG::colorToSVGHex(glm::vec4{0.5f, 0.5f, 0.5f, 0.0f});
    auto const hex1 = PlottingSVG::colorToSVGHex(glm::vec4{0.5f, 0.5f, 0.5f, 1.0f});
    REQUIRE(hex0 == hex1);
}

TEST_CASE("colorToSVGHex output is exactly 7 characters",
          "[PlottingSVG][SVGUtils]") {
    auto const result = PlottingSVG::colorToSVGHex(glm::vec4{0.3f, 0.6f, 0.9f, 1.0f});
    REQUIRE(result.size() == 7);
    REQUIRE(result[0] == '#');
}
