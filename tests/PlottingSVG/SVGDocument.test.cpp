/**
 * @file SVGDocument.test.cpp
 * @brief Tests for PlottingSVG::SVGDocument XML assembly.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "PlottingSVG/SVGDocument.hpp"

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::EndsWith;
using Catch::Matchers::StartsWith;

TEST_CASE("SVGDocument build produces valid XML header",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(800, 600);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, StartsWith("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
}

TEST_CASE("SVGDocument build contains svg element with dimensions",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(1920, 1080);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring("width=\"1920\""));
    REQUIRE_THAT(svg, ContainsSubstring("height=\"1080\""));
    REQUIRE_THAT(svg, ContainsSubstring("viewBox=\"0 0 1920 1080\""));
}

TEST_CASE("SVGDocument build ends with closing svg tag",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(100, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, EndsWith("</svg>\n"));
}

TEST_CASE("SVGDocument build contains xmlns attribute",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(100, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring("xmlns=\"http://www.w3.org/2000/svg\""));
}

TEST_CASE("SVGDocument default background is white",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(100, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring(R"(fill="#FFFFFF")"));
}

TEST_CASE("SVGDocument setBackground changes fill color",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.setBackground("#1A2B3C");
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring(R"(fill="#1A2B3C")"));
}

TEST_CASE("SVGDocument default description is WhiskerToolbox Export",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(100, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring("<desc>WhiskerToolbox Export</desc>"));
}

TEST_CASE("SVGDocument setDescription changes desc text",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.setDescription("Custom export");
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring("<desc>Custom export</desc>"));
}

TEST_CASE("SVGDocument empty description omits desc element",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.setDescription("");
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, !ContainsSubstring("<desc>"));
}

TEST_CASE("SVGDocument addElements creates named g layer",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.addElements("test_layer", {R"(<circle cx="10" cy="10" r="5"/>)"});
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring(R"(<g id="test_layer">)"));
    REQUIRE_THAT(svg, ContainsSubstring(R"(<circle cx="10" cy="10" r="5"/>)"));
}

TEST_CASE("SVGDocument addElements preserves insertion order",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.addElements("first", {"<line/>"});
    doc.addElements("second", {"<rect/>"});
    doc.addElements("third", {"<circle/>"});
    std::string const svg = doc.build();

    auto const pos_first = svg.find(R"(id="first")");
    auto const pos_second = svg.find(R"(id="second")");
    auto const pos_third = svg.find(R"(id="third")");

    REQUIRE(pos_first != std::string::npos);
    REQUIRE(pos_second != std::string::npos);
    REQUIRE(pos_third != std::string::npos);
    REQUIRE(pos_first < pos_second);
    REQUIRE(pos_second < pos_third);
}

TEST_CASE("SVGDocument addElements appends to existing layer",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument doc(100, 100);
    doc.addElements("data", {"<line/>"});
    doc.addElements("data", {"<rect/>"});
    std::string const svg = doc.build();

    // Only one <g id="data"> group
    auto const first_pos = svg.find(R"(id="data")");
    auto const second_pos = svg.find(R"(id="data")", first_pos + 1);
    REQUIRE(first_pos != std::string::npos);
    REQUIRE(second_pos == std::string::npos);

    // Both elements present
    REQUIRE_THAT(svg, ContainsSubstring("<line/>"));
    REQUIRE_THAT(svg, ContainsSubstring("<rect/>"));
}

TEST_CASE("SVGDocument with no layers produces only header background and footer",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(200, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring("<?xml"));
    REQUIRE_THAT(svg, ContainsSubstring("<svg"));
    REQUIRE_THAT(svg, ContainsSubstring("<rect"));
    REQUIRE_THAT(svg, ContainsSubstring("</svg>"));
    // No <g> layers
    REQUIRE_THAT(svg, !ContainsSubstring("<g id="));
}

TEST_CASE("SVGDocument background rect covers full viewport",
          "[PlottingSVG][SVGDocument]") {
    PlottingSVG::SVGDocument const doc(100, 100);
    std::string const svg = doc.build();

    REQUIRE_THAT(svg, ContainsSubstring(R"(width="100%" height="100%")"));
}
