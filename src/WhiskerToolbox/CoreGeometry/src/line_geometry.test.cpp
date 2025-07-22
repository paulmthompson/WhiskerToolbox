#include "CoreGeometry/line_geometry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>


/**
 * @brief Test file for lines utility functions
 */

TEST_CASE("get_position_at_percentage - Basic functionality", "[get_position_at_percentage]") {
    SECTION("Empty line") {
        Line2D empty_line;
        auto result = get_position_at_percentage(empty_line, 0.5f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }

    SECTION("Single point line") {
        auto single_point = Line2D(std::vector<Point2D<float>>{{5.0f, 10.0f}});
        auto result = get_position_at_percentage(single_point, 0.5f);
        REQUIRE(result.x == 5.0f);
        REQUIRE(result.y == 10.0f);
    }

    SECTION("Two point line") {
        auto two_points = Line2D(std::vector<Point2D<float>>{
                {0.0f, 0.0f},
                {10.0f, 0.0f}
        });

        // Test at 0% (start)
        auto result_start = get_position_at_percentage(two_points, 0.0f);
        REQUIRE(result_start.x == 0.0f);
        REQUIRE(result_start.y == 0.0f);

        // Test at 50% (middle)
        auto result_middle = get_position_at_percentage(two_points, 0.5f);
        REQUIRE(result_middle.x == 5.0f);
        REQUIRE(result_middle.y == 0.0f);

        // Test at 100% (end)
        auto result_end = get_position_at_percentage(two_points, 1.0f);
        REQUIRE(result_end.x == 10.0f);
        REQUIRE(result_end.y == 0.0f);
    }
}

TEST_CASE("get_position_at_percentage - Complex line", "[get_position_at_percentage]") {
    // Create a right triangle: (0,0) -> (3,0) -> (3,4)
    // Total length = 3 + 4 = 7
    auto triangle = Line2D({
            {0.0f, 0.0f},  // Start
            {3.0f, 0.0f},  // Corner
            {3.0f, 4.0f}   // End
    });

    SECTION("At 0% - should be at start") {
        auto result = get_position_at_percentage(triangle, 0.0f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }

    SECTION("At ~43% - should be at corner (3/7)") {
        auto result = get_position_at_percentage(triangle, 3.0f/7.0f);
        REQUIRE(result.x == 3.0f);
        REQUIRE(result.y == 0.0f);
    }

    SECTION("At 100% - should be at end") {
        auto result = get_position_at_percentage(triangle, 1.0f);
        REQUIRE(result.x == 3.0f);
        REQUIRE(result.y == 4.0f);
    }
}

TEST_CASE("get_position_at_percentage - Edge cases", "[get_position_at_percentage]") {
    auto line = Line2D({
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{10.0f, 10.0f}
    });

    SECTION("Percentage below 0 should clamp to 0") {
        auto result = get_position_at_percentage(line, -0.5f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }

    SECTION("Percentage above 1 should clamp to 1") {
        auto result = get_position_at_percentage(line, 1.5f);
        REQUIRE(result.x == 10.0f);
        REQUIRE(result.y == 10.0f);
    }
}

TEST_CASE("Line segment extraction functionality", "[lines][segment]") {

    SECTION("Basic segment extraction") {
        // Create a simple horizontal line
        auto line = Line2D({{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}, {30.0f, 0.0f}});

        // Extract middle 50% (25% to 75%)
        auto segment = get_segment_between_percentages(line, 0.25f, 0.75f);

        REQUIRE(!segment.empty());
        REQUIRE(segment.size() >= 2);

        // First point should be at 25% position (7.5, 0)
        REQUIRE(segment[0].x == Catch::Approx(7.5f).margin(0.1f));
        REQUIRE(segment[0].y == Catch::Approx(0.0f).margin(0.1f));

        // Last point should be at 75% position (22.5, 0)
        REQUIRE(segment.back().x == Catch::Approx(22.5f).margin(0.1f));
        REQUIRE(segment.back().y == Catch::Approx(0.0f).margin(0.1f));
    }

    SECTION("Full line segment (0% to 100%)") {
        Line2D line = Line2D({{0.0f, 0.0f}, {10.0f, 10.0f}, {20.0f, 0.0f}});

        auto segment = get_segment_between_percentages(line, 0.0f, 1.0f);

        REQUIRE(segment.size() == line.size());
        REQUIRE(segment[0].x == line[0].x);
        REQUIRE(segment[0].y == line[0].y);
        REQUIRE(segment.back().x == line.back().x);
        REQUIRE(segment.back().y == line.back().y);
    }

    SECTION("Complex multi-segment line") {
        // Create an L-shaped line: horizontal then vertical
        // 10 + 10 = 20 total length
        auto line = Line2D({{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}});

        // Extract from 25% to 75% (should span the corner)
        // 25% of 20 = 5, 75% of 20 = 15
        // So we expect to start at (5, 0) and end at (10, 5)
        auto segment = get_segment_between_percentages(line, 0.25f, 0.75f);

        REQUIRE(!segment.empty());
        REQUIRE(segment.size() >= 2);

        // Should start at 25% along first segment
        REQUIRE(segment[0].x == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(segment[0].y == Catch::Approx(0.0f).margin(0.1f));

        // Should end at 25% along second segment
        REQUIRE(segment.back().x == Catch::Approx(10.0f).margin(0.1f));
        REQUIRE(segment.back().y == Catch::Approx(5.0f).margin(0.1f));
    }
}

TEST_CASE("Line segment extraction edge cases", "[lines][segment][edge-cases]") {

    SECTION("Empty line") {
        Line2D empty_line;
        auto segment = get_segment_between_percentages(empty_line, 0.25f, 0.75f);
        REQUIRE(segment.empty());
    }

    SECTION("Single point line") {
        auto single_point = Line2D({{5.0f, 5.0f}});
        auto segment = get_segment_between_percentages(single_point, 0.25f, 0.75f);
        REQUIRE(segment.empty());
    }

    SECTION("Two point line") {
        auto line = Line2D({{0.0f, 0.0f}, {10.0f, 0.0f}});

        auto segment = get_segment_between_percentages(line, 0.25f, 0.75f);

        REQUIRE(segment.size() == 2);
        REQUIRE(segment[0].x == Catch::Approx(2.5f).margin(0.1f));
        REQUIRE(segment.back().x == Catch::Approx(7.5f).margin(0.1f));
    }

    SECTION("Invalid percentage ranges") {
        auto line = Line2D({{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}});

        // Start >= End
        auto segment1 = get_segment_between_percentages(line, 0.75f, 0.25f);
        REQUIRE(segment1.empty());

        // Start == End
        auto segment2 = get_segment_between_percentages(line, 0.5f, 0.5f);
        REQUIRE(segment2.empty());
    }

    SECTION("Out of range percentages are clamped") {
        auto line = Line2D({{0.0f, 0.0f}, {10.0f, 0.0f}});

        // Negative start percentage should be clamped to 0
        auto segment1 = get_segment_between_percentages(line, -0.5f, 0.5f);
        REQUIRE(!segment1.empty());
        REQUIRE(segment1[0].x == Catch::Approx(0.0f).margin(0.1f));

        // End percentage > 1 should be clamped to 1
        auto segment2 = get_segment_between_percentages(line, 0.5f, 1.5f);
        REQUIRE(!segment2.empty());
        REQUIRE(segment2.back().x == Catch::Approx(10.0f).margin(0.1f));
    }

    SECTION("Zero-length segments") {
        // Line where all points are the same (zero total distance)
        auto line = Line2D({{5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f}});
        auto segment = get_segment_between_percentages(line, 0.25f, 0.75f);
        REQUIRE(segment.empty());
    }
}
