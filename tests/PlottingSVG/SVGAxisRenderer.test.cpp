/**
 * @file SVGAxisRenderer.test.cpp
 * @brief Tests for PlottingSVG::SVGAxisRenderer decoration.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/Decorations/SVGAxisRenderer.hpp"

using Catch::Matchers::ContainsSubstring;

TEST_CASE("SVGAxisRenderer returns empty for zero canvas",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer const axis(0.0f, 10.0f, 1.0f, "X");
    REQUIRE(axis.render(0, 100).empty());
    REQUIRE(axis.render(100, 0).empty());
}

TEST_CASE("SVGAxisRenderer returns empty for zero range",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer const axis(5.0f, 5.0f, 1.0f, "X");
    REQUIRE(axis.render(800, 600).empty());
}

TEST_CASE("SVGAxisRenderer returns empty for inverted range",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer const axis(10.0f, 0.0f, 1.0f, "X");
    REQUIRE(axis.render(800, 600).empty());
}

TEST_CASE("SVGAxisRenderer returns empty when margin too large",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 1.0f, "X");
    axis.setMargin(500);// 2*500 >= 800
    REQUIRE(axis.render(800, 600).empty());
}

TEST_CASE("SVGAxisRenderer Bottom produces spine line",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    axis.setMargin(48);
    auto const elements = axis.render(800, 600);

    REQUIRE(!elements.empty());
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
    REQUIRE_THAT(elements[0], ContainsSubstring("stroke-width=\"1\""));
}

TEST_CASE("SVGAxisRenderer Bottom with ticks produces tick lines and labels",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    axis.setMargin(48);
    auto const elements = axis.render(800, 600);

    // spine + (ticks at 0, 5, 10) × (tick_line + tick_label) = 1 + 3*2 = 7
    REQUIRE(elements.size() >= 5);// at least spine + some ticks

    // Check that there are <text> elements (tick labels)
    bool has_text = false;
    for (auto const & el: elements) {
        if (el.find("<text") != std::string::npos) {
            has_text = true;
            break;
        }
    }
    REQUIRE(has_text);
}

TEST_CASE("SVGAxisRenderer Bottom with title includes title text",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "Time (ms)");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    auto const elements = axis.render(800, 600);

    bool has_title = false;
    for (auto const & el: elements) {
        if (el.find("Time (ms)") != std::string::npos) {
            has_title = true;
            break;
        }
    }
    REQUIRE(has_title);
}

TEST_CASE("SVGAxisRenderer Left produces vertical spine",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 100.0f, 25.0f, "Value");
    axis.setPosition(PlottingSVG::AxisPosition::Left);
    axis.setMargin(48);
    auto const elements = axis.render(800, 600);

    REQUIRE(!elements.empty());
    // First element is spine <line>
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
}

TEST_CASE("SVGAxisRenderer Left with title has rotated text",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 100.0f, 25.0f, "Amplitude");
    axis.setPosition(PlottingSVG::AxisPosition::Left);
    auto const elements = axis.render(800, 600);

    bool has_rotated_title = false;
    for (auto const & el: elements) {
        if (el.find("rotate(-90") != std::string::npos &&
            el.find("Amplitude") != std::string::npos) {
            has_rotated_title = true;
            break;
        }
    }
    REQUIRE(has_rotated_title);
}

TEST_CASE("SVGAxisRenderer no ticks emitted when tick_interval is zero",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 0.0f, "");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    auto const elements = axis.render(800, 600);

    // Should only have spine (no ticks, no labels, no title)
    REQUIRE(elements.size() == 1);
    REQUIRE_THAT(elements[0], ContainsSubstring("<line"));
}

TEST_CASE("SVGAxisRenderer uses configured axis color",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "");
    axis.setAxisColor("#FF0000");
    auto const elements = axis.render(800, 600);

    REQUIRE(!elements.empty());
    REQUIRE_THAT(elements[0], ContainsSubstring(R"(stroke="#FF0000")"));
}

TEST_CASE("SVGAxisRenderer uses configured label color",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "Title");
    axis.setLabelColor("#00FF00");
    auto const elements = axis.render(800, 600);

    // Find a text element and check fill color
    bool has_green_label = false;
    for (auto const & el: elements) {
        if (el.find("<text") != std::string::npos &&
            el.find(R"(fill="#00FF00")") != std::string::npos) {
            has_green_label = true;
            break;
        }
    }
    REQUIRE(has_green_label);
}

TEST_CASE("SVGAxisRenderer escapes special characters in title",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "A & B < C > D");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    auto const elements = axis.render(800, 600);

    bool has_escaped = false;
    for (auto const & el: elements) {
        if (el.find("A &amp; B &lt; C &gt; D") != std::string::npos) {
            has_escaped = true;
            break;
        }
    }
    REQUIRE(has_escaped);
}

TEST_CASE("SVGAxisRenderer empty title omits title element",
          "[PlottingSVG][SVGAxisRenderer]") {
    PlottingSVG::SVGAxisRenderer axis(0.0f, 10.0f, 5.0f, "");
    axis.setPosition(PlottingSVG::AxisPosition::Bottom);
    auto const elements = axis.render(800, 600);

    // Count text elements — should only be tick labels, no title
    int text_count = 0;
    for (auto const & el: elements) {
        if (el.find("<text") != std::string::npos) {
            ++text_count;
        }
    }
    // 3 ticks at 0, 5, 10 → 3 tick labels
    // No title text
    REQUIRE(text_count == 3);
}
