/**
 * @file SVGScalebar.test.cpp
 * @brief Tests for PlottingSVG::SVGScalebar decoration.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Decorations/SVGScalebar.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("SVGScalebar returns empty for zero time span",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(100, 0.0f, 0.0f);
    auto const elements = scalebar.render(800, 600);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGScalebar returns empty for negative time span",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(100, 500.0f, 100.0f);
    auto const elements = scalebar.render(800, 600);
    REQUIRE(elements.empty());
}

TEST_CASE("SVGScalebar returns empty for zero canvas dimensions",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(100, 0.0f, 1000.0f);
    REQUIRE(scalebar.render(0, 600).empty());
    REQUIRE(scalebar.render(800, 0).empty());
}

TEST_CASE("SVGScalebar produces exactly 4 elements",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(100, 0.0f, 1000.0f);
    auto const elements = scalebar.render(1000, 500);
    REQUIRE(elements.size() == 4);
}

TEST_CASE("SVGScalebar elements are bar, left tick, right tick, label",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(200, 0.0f, 2000.0f);
    auto const elements = scalebar.render(1920, 1080);

    // Bar: horizontal line
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
    // Left tick: vertical line
    REQUIRE_THAT(elements[1], ContainsSubstring("<line"));
    // Right tick: vertical line
    REQUIRE_THAT(elements[2], ContainsSubstring("<line"));
    // Label: text with the scalebar length
    REQUIRE_THAT(elements[3], ContainsSubstring("<text"));
    REQUIRE_THAT(elements[3], ContainsSubstring("200"));
}

TEST_CASE("SVGScalebar uses configured stroke color",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setStrokeColor("#FF0000");
    auto const elements = scalebar.render(800, 600);

    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#FF0000")"));
    REQUIRE_THAT(elements[1], ContainsSubstring(R"(stroke="#FF0000")"));
    REQUIRE_THAT(elements[2], ContainsSubstring(R"(stroke="#FF0000")"));
}

TEST_CASE("SVGScalebar uses configured label fill color",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setLabelFillColor("#0000FF");
    auto const elements = scalebar.render(800, 600);

    REQUIRE_THAT(elements[3], ContainsSubstring(R"(fill="#0000FF")"));
}

TEST_CASE("SVGScalebar uses configured bar thickness",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setBarThickness(6);
    auto const elements = scalebar.render(800, 600);

    REQUIRE_THAT(elements[0], ContainsSubstring("stroke-width=\"6\""));
}

TEST_CASE("SVGScalebar uses configured font size",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setLabelFontSize(20);
    auto const elements = scalebar.render(800, 600);

    REQUIRE_THAT(elements[3], ContainsSubstring("font-size=\"20\""));
}

TEST_CASE("SVGScalebar label has text-anchor middle",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar const scalebar(100, 0.0f, 1000.0f);
    auto const elements = scalebar.render(800, 600);

    REQUIRE_THAT(elements[3], ContainsSubstring(R"(text-anchor="middle")"));
}

TEST_CASE("SVGScalebar BottomRight corner positions bar near bottom-right",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setCorner(PlottingSVG::ScalebarCorner::BottomRight);
    scalebar.setPadding(50);
    int const W = 1000;
    int const H = 500;
    auto const elements = scalebar.render(W, H);
    REQUIRE(elements.size() == 4);

    // bar_width = 100 * (1000 / 1000) = 100 px
    // bar_x = 1000 - 100 - 50 = 850
    // bar_y = 500 - 50 = 450
    REQUIRE_THAT(elements[0], ContainsSubstring("x1=\"850\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("y1=\"450\""));
}

TEST_CASE("SVGScalebar BottomLeft corner positions bar near bottom-left",
          "[PlottingSVG][SVGScalebar]") {
    PlottingSVG::SVGScalebar scalebar(100, 0.0f, 1000.0f);
    scalebar.setCorner(PlottingSVG::ScalebarCorner::BottomLeft);
    scalebar.setPadding(50);
    int const W = 1000;
    int const H = 500;
    auto const elements = scalebar.render(W, H);
    REQUIRE(elements.size() == 4);

    // bar_x = padding = 50
    // bar_y = 500 - 50 = 450
    REQUIRE_THAT(elements[0], ContainsSubstring("x1=\"50\""));
    REQUIRE_THAT(elements[0], ContainsSubstring("y1=\"450\""));
}
