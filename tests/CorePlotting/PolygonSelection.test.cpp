#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "CorePlotting/Selection/PolygonSelection.hpp"

#include <glm/vec2.hpp>

#include <vector>

using namespace CorePlotting::Selection;
using Catch::Matchers::UnorderedEquals;

namespace {

/// Helper to build a simple square polygon from (0,0) to (10,10)
std::vector<glm::vec2> unitSquare()
{
    return {{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f}};
}

/// Triangle (0,0) -> (10,0) -> (5,10)
std::vector<glm::vec2> triangle()
{
    return {{0.0f, 0.0f}, {10.0f, 0.0f}, {5.0f, 10.0f}};
}

}  // namespace

TEST_CASE("selectPointsInPolygon with square polygon")
{
    auto poly = unitSquare();
    std::vector<float> x = {5.0f, 15.0f, 5.0f, -1.0f, 8.0f};
    std::vector<float> y = {5.0f, 5.0f, 15.0f, 5.0f, 8.0f};

    auto result = selectPointsInPolygon(poly, x, y);

    // Points at (5,5) and (8,8) are inside; (15,5), (5,15), (-1,5) are outside
    REQUIRE(result.selected_indices.size() == 2);
    CHECK_THAT(result.selected_indices,
               UnorderedEquals(std::vector<std::size_t>{0, 4}));
}

TEST_CASE("selectPointsInPolygon with triangle polygon")
{
    auto poly = triangle();
    std::vector<float> x = {5.0f, 1.0f, 9.0f, 5.0f};
    std::vector<float> y = {1.0f, 1.0f, 1.0f, 8.0f};

    auto result = selectPointsInPolygon(poly, x, y);

    // All points inside the triangle
    // (5,1) inside, (1,1) inside, (9,1) inside, (5,8) inside
    CHECK(result.selected_indices.size() == 4);
}

TEST_CASE("selectPointsInPolygon with no points inside")
{
    auto poly = unitSquare();
    std::vector<float> x = {15.0f, -5.0f, 20.0f};
    std::vector<float> y = {15.0f, -5.0f, 20.0f};

    auto result = selectPointsInPolygon(poly, x, y);

    CHECK(result.selected_indices.empty());
}

TEST_CASE("selectPointsInPolygon with empty points")
{
    auto poly = unitSquare();
    std::vector<float> x;
    std::vector<float> y;

    auto result = selectPointsInPolygon(poly, x, y);

    CHECK(result.selected_indices.empty());
}

TEST_CASE("selectPointsInPolygon all points inside")
{
    auto poly = unitSquare();
    std::vector<float> x = {1.0f, 5.0f, 9.0f};
    std::vector<float> y = {1.0f, 5.0f, 9.0f};

    auto result = selectPointsInPolygon(poly, x, y);

    REQUIRE(result.selected_indices.size() == 3);
    CHECK_THAT(result.selected_indices,
               UnorderedEquals(std::vector<std::size_t>{0, 1, 2}));
}

TEST_CASE("selectPointsInPolygon concave polygon")
{
    // L-shaped polygon
    std::vector<glm::vec2> poly = {
        {0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 5.0f},
        {5.0f, 5.0f}, {5.0f, 10.0f}, {0.0f, 10.0f}
    };

    // Point in the bottom part of L: should be inside
    // Point in the "missing" upper-right: should be outside
    std::vector<float> x = {2.0f, 8.0f, 2.0f};
    std::vector<float> y = {2.0f, 8.0f, 8.0f};

    auto result = selectPointsInPolygon(poly, x, y);

    // (2,2) inside bottom, (8,8) outside upper-right, (2,8) inside left arm
    REQUIRE(result.selected_indices.size() == 2);
    CHECK_THAT(result.selected_indices,
               UnorderedEquals(std::vector<std::size_t>{0, 2}));
}
