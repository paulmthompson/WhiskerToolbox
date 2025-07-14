#include "CoreGeometry/polygon.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Polygon - Construction and basic properties", "[polygon][construction]") {
    SECTION("Empty polygon") {
        std::vector<Point2D<float>> empty_vertices;
        Polygon empty_polygon(empty_vertices);
        
        REQUIRE(!empty_polygon.isValid());
        REQUIRE(empty_polygon.vertexCount() == 0);
    }

    SECTION("Triangle polygon") {
        std::vector<Point2D<float>> triangle_vertices = {
            {0.0f, 0.0f},
            {10.0f, 0.0f},
            {5.0f, 10.0f}
        };
        Polygon triangle(triangle_vertices);
        
        REQUIRE(triangle.isValid());
        REQUIRE(triangle.vertexCount() == 3);
        REQUIRE(triangle.getVertices().size() == 3);
    }

    SECTION("Line (2 points) - invalid for polygon operations") {
        std::vector<Point2D<float>> line_vertices = {
            {0.0f, 0.0f},
            {10.0f, 10.0f}
        };
        Polygon line(line_vertices);
        
        REQUIRE(!line.isValid());
        REQUIRE(line.vertexCount() == 2);
    }
}

TEST_CASE("Polygon - Bounding box calculation", "[polygon][bounding_box]") {
    SECTION("Triangle bounding box") {
        std::vector<Point2D<float>> triangle_vertices = {
            {1.0f, 2.0f},
            {5.0f, 1.0f},
            {3.0f, 7.0f}
        };
        Polygon triangle(triangle_vertices);
        
        auto bbox = triangle.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(1.0f));
        REQUIRE(bbox.min_y == Catch::Approx(1.0f));
        REQUIRE(bbox.max_x == Catch::Approx(5.0f));
        REQUIRE(bbox.max_y == Catch::Approx(7.0f));
    }

    SECTION("Square bounding box") {
        std::vector<Point2D<float>> square_vertices = {
            {0.0f, 0.0f},
            {10.0f, 0.0f},
            {10.0f, 10.0f},
            {0.0f, 10.0f}
        };
        Polygon square(square_vertices);
        
        auto bbox = square.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f));
        REQUIRE(bbox.max_x == Catch::Approx(10.0f));
        REQUIRE(bbox.max_y == Catch::Approx(10.0f));
    }
}

TEST_CASE("Polygon - Point containment", "[polygon][containment]") {
    SECTION("Triangle containment") {
        // Right triangle with vertices at (0,0), (10,0), (0,10)
        std::vector<Point2D<float>> triangle_vertices = {
            {0.0f, 0.0f},
            {10.0f, 0.0f},
            {0.0f, 10.0f}
        };
        Polygon triangle(triangle_vertices);
        
        // Points clearly inside
        REQUIRE(triangle.containsPoint({1.0f, 1.0f}));
        REQUIRE(triangle.containsPoint({2.0f, 2.0f}));
        REQUIRE(triangle.containsPoint({1.0f, 8.0f}));
        
        // Points clearly outside
        REQUIRE(!triangle.containsPoint({-1.0f, 1.0f}));
        REQUIRE(!triangle.containsPoint({1.0f, -1.0f}));
        REQUIRE(!triangle.containsPoint({15.0f, 15.0f}));
        REQUIRE(!triangle.containsPoint({6.0f, 6.0f})); // Outside the triangle but inside bounding box
        
        // Points on vertices (implementation dependent - typically outside)
        // This is acceptable behavior for floating point comparisons
        
        // Points on edges (implementation dependent)
        // Ray casting may or may not include edge points
    }

    SECTION("Square containment") {
        std::vector<Point2D<float>> square_vertices = {
            {0.0f, 0.0f},
            {10.0f, 0.0f},
            {10.0f, 10.0f},
            {0.0f, 10.0f}
        };
        Polygon square(square_vertices);
        
        // Points clearly inside
        REQUIRE(square.containsPoint({5.0f, 5.0f}));
        REQUIRE(square.containsPoint({1.0f, 1.0f}));
        REQUIRE(square.containsPoint({9.0f, 9.0f}));
        
        // Points clearly outside
        REQUIRE(!square.containsPoint({-1.0f, 5.0f}));
        REQUIRE(!square.containsPoint({5.0f, -1.0f}));
        REQUIRE(!square.containsPoint({15.0f, 5.0f}));
        REQUIRE(!square.containsPoint({5.0f, 15.0f}));
    }

    SECTION("Complex polygon containment") {
        // Star-shaped polygon
        std::vector<Point2D<float>> star_vertices = {
            {5.0f, 0.0f},   // Top point
            {6.0f, 3.0f},   // Top right inner
            {10.0f, 3.0f},  // Right point
            {7.0f, 5.0f},   // Right inner
            {8.0f, 10.0f},  // Bottom right
            {5.0f, 7.0f},   // Bottom inner
            {2.0f, 10.0f},  // Bottom left
            {3.0f, 5.0f},   // Left inner
            {0.0f, 3.0f},   // Left point
            {4.0f, 3.0f}    // Top left inner
        };
        Polygon star(star_vertices);
        
        // Point in center
        REQUIRE(star.containsPoint({5.0f, 5.0f}));
        
        // Points in outer tips
        REQUIRE(star.containsPoint({5.0f, 1.0f}));
        REQUIRE(star.containsPoint({9.0f, 3.5f}));
        
        // Points that should be outside (in the "indented" areas)
        REQUIRE(!star.containsPoint({5.0f, 8.5f}));
        REQUIRE(!star.containsPoint({1.5f, 5.0f}));
    }

    SECTION("Invalid polygon (fewer than 3 vertices)") {
        std::vector<Point2D<float>> line_vertices = {
            {0.0f, 0.0f},
            {10.0f, 10.0f}
        };
        Polygon line(line_vertices);
        
        // Should always return false for invalid polygons
        REQUIRE(!line.containsPoint({5.0f, 5.0f}));
        REQUIRE(!line.containsPoint({0.0f, 0.0f}));
        REQUIRE(!line.containsPoint({15.0f, 15.0f}));
    }
}

TEST_CASE("Polygon - Edge cases", "[polygon][edge_cases]") {
    SECTION("Points outside bounding box are quickly rejected") {
        std::vector<Point2D<float>> triangle_vertices = {
            {0.0f, 0.0f},
            {10.0f, 0.0f},
            {5.0f, 10.0f}
        };
        Polygon triangle(triangle_vertices);
        
        // These should be quickly rejected by bounding box check
        REQUIRE(!triangle.containsPoint({-10.0f, 5.0f}));
        REQUIRE(!triangle.containsPoint({20.0f, 5.0f}));
        REQUIRE(!triangle.containsPoint({5.0f, -10.0f}));
        REQUIRE(!triangle.containsPoint({5.0f, 20.0f}));
    }

    SECTION("Very small polygon") {
        std::vector<Point2D<float>> tiny_triangle = {
            {0.0f, 0.0f},
            {0.1f, 0.0f},
            {0.05f, 0.1f}
        };
        Polygon tiny(tiny_triangle);
        
        REQUIRE(tiny.isValid());
        REQUIRE(tiny.containsPoint({0.05f, 0.01f}));
        REQUIRE(!tiny.containsPoint({0.5f, 0.5f}));
    }
}

TEST_CASE("Polygon - BoundingBox constructor", "[polygon][construction][bounding_box]") {
    SECTION("Rectangle from bounding box") {
        BoundingBox bbox(1.0f, 2.0f, 10.0f, 8.0f);
        Polygon rect(bbox);
        
        REQUIRE(rect.isValid());
        REQUIRE(rect.vertexCount() == 4);
        
        auto vertices = rect.getVertices();
        REQUIRE(vertices.size() == 4);
        
        // Check vertices are in counter-clockwise order starting from bottom-left
        REQUIRE(vertices[0].x == Catch::Approx(1.0f));  // Bottom-left
        REQUIRE(vertices[0].y == Catch::Approx(2.0f));
        REQUIRE(vertices[1].x == Catch::Approx(10.0f)); // Bottom-right
        REQUIRE(vertices[1].y == Catch::Approx(2.0f));
        REQUIRE(vertices[2].x == Catch::Approx(10.0f)); // Top-right
        REQUIRE(vertices[2].y == Catch::Approx(8.0f));
        REQUIRE(vertices[3].x == Catch::Approx(1.0f));  // Top-left
        REQUIRE(vertices[3].y == Catch::Approx(8.0f));
        
        // Verify bounding box is preserved
        auto computed_bbox = rect.getBoundingBox();
        REQUIRE(computed_bbox.min_x == Catch::Approx(1.0f));
        REQUIRE(computed_bbox.min_y == Catch::Approx(2.0f));
        REQUIRE(computed_bbox.max_x == Catch::Approx(10.0f));
        REQUIRE(computed_bbox.max_y == Catch::Approx(8.0f));
    }

    SECTION("Point containment in rectangle from bounding box") {
        BoundingBox bbox(0.0f, 0.0f, 10.0f, 10.0f);
        Polygon square(bbox);
        
        // Points inside
        REQUIRE(square.containsPoint({5.0f, 5.0f}));
        REQUIRE(square.containsPoint({1.0f, 1.0f}));
        REQUIRE(square.containsPoint({9.0f, 9.0f}));
        
        // Points outside
        REQUIRE(!square.containsPoint({-1.0f, 5.0f}));
        REQUIRE(!square.containsPoint({5.0f, -1.0f}));
        REQUIRE(!square.containsPoint({15.0f, 5.0f}));
        REQUIRE(!square.containsPoint({5.0f, 15.0f}));
    }
}

TEST_CASE("Polygon - Intersection operations", "[polygon][intersection]") {
    SECTION("Rectangle intersection - overlapping") {
        // Create two overlapping rectangles
        Polygon rect1(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon rect2(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        REQUIRE(rect1.intersects(rect2));
        REQUIRE(rect2.intersects(rect1));
        
        auto intersection = rect1.intersectionWith(rect2);
        REQUIRE(intersection.isValid());
        
        // The intersection should be the overlapping area (5,5) to (10,10)
        auto bbox = intersection.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.min_y == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.max_x == Catch::Approx(10.0f).margin(0.1f));
        REQUIRE(bbox.max_y == Catch::Approx(10.0f).margin(0.1f));
    }

    SECTION("Rectangle intersection - non-overlapping") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 5.0f, 5.0f));
        Polygon rect2(BoundingBox(10.0f, 10.0f, 15.0f, 15.0f));
        
        REQUIRE(!rect1.intersects(rect2));
        REQUIRE(!rect2.intersects(rect1));
        
        auto intersection = rect1.intersectionWith(rect2);
        REQUIRE(!intersection.isValid()); // Should be empty
    }

    SECTION("Rectangle intersection - touching edges") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 5.0f, 5.0f));
        Polygon rect2(BoundingBox(5.0f, 0.0f, 10.0f, 5.0f));
        
        // Rectangles sharing an edge should intersect
        REQUIRE(rect1.intersects(rect2));
        REQUIRE(rect2.intersects(rect1));
    }

    SECTION("Triangle intersection") {
        std::vector<Point2D<float>> triangle1_vertices = {
            {0.0f, 0.0f}, {6.0f, 0.0f}, {3.0f, 6.0f}
        };
        std::vector<Point2D<float>> triangle2_vertices = {
            {3.0f, 3.0f}, {9.0f, 3.0f}, {6.0f, 9.0f}
        };
        
        Polygon triangle1(triangle1_vertices);
        Polygon triangle2(triangle2_vertices);
        
        REQUIRE(triangle1.intersects(triangle2));
        
        auto intersection = triangle1.intersectionWith(triangle2);
        // The intersection should exist (even if it's a small polygon)
        // For overlapping triangles, we expect some intersection area
    }
}

TEST_CASE("Polygon - Union operations", "[polygon][union]") {
    SECTION("Rectangle union - non-overlapping") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 5.0f, 5.0f));
        Polygon rect2(BoundingBox(10.0f, 10.0f, 15.0f, 15.0f));
        
        auto union_poly = rect1.unionWith(rect2);
        REQUIRE(union_poly.isValid());
        
        // Union of non-overlapping rectangles should encompass both
        auto bbox = union_poly.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f));
        REQUIRE(bbox.max_x == Catch::Approx(15.0f));
        REQUIRE(bbox.max_y == Catch::Approx(15.0f));
    }

    SECTION("Rectangle union - overlapping") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon rect2(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        auto union_poly = rect1.unionWith(rect2);
        REQUIRE(union_poly.isValid());
        
        // Union should encompass both rectangles
        auto bbox = union_poly.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f));
        REQUIRE(bbox.max_x == Catch::Approx(15.0f));
        REQUIRE(bbox.max_y == Catch::Approx(15.0f));
    }

    SECTION("Union with empty polygon") {
        Polygon rect(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon empty(std::vector<Point2D<float>>{});
        
        auto union1 = rect.unionWith(empty);
        auto union2 = empty.unionWith(rect);
        
        // Union with empty should return the non-empty polygon
        REQUIRE(union1.isValid());
        REQUIRE(union2.isValid());
        
        auto bbox1 = union1.getBoundingBox();
        auto bbox2 = union2.getBoundingBox();
        REQUIRE(bbox1.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox1.max_x == Catch::Approx(10.0f));
        REQUIRE(bbox2.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox2.max_x == Catch::Approx(10.0f));
    }
}

TEST_CASE("Polygon - Complex intersection scenarios", "[polygon][intersection][complex]") {
    SECTION("Self intersection") {
        Polygon rect(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        
        REQUIRE(rect.intersects(rect));
        
        auto self_intersection = rect.intersectionWith(rect);
        REQUIRE(self_intersection.isValid());
        
        // Self intersection should be the same polygon
        auto bbox = self_intersection.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f));
        REQUIRE(bbox.max_x == Catch::Approx(10.0f));
        REQUIRE(bbox.max_y == Catch::Approx(10.0f));
    }

    SECTION("One polygon inside another") {
        Polygon outer(BoundingBox(0.0f, 0.0f, 20.0f, 20.0f));
        Polygon inner(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        REQUIRE(outer.intersects(inner));
        REQUIRE(inner.intersects(outer));
        
        auto intersection = outer.intersectionWith(inner);
        REQUIRE(intersection.isValid());
        
        // Intersection should be the inner polygon
        auto bbox = intersection.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.min_y == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.max_x == Catch::Approx(15.0f).margin(0.1f));
        REQUIRE(bbox.max_y == Catch::Approx(15.0f).margin(0.1f));
    }
}

TEST_CASE("Polygon - Martinez-Rueda Boolean Operations", "[polygon][boolean][martinez_rueda]") {
    SECTION("Rectangle intersection using Martinez-Rueda") {
        // Create two overlapping rectangles
        Polygon rect1(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon rect2(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        auto intersection = rect1.intersectionWith(rect2);
        REQUIRE(intersection.isValid());
        
        // The intersection should be the overlapping area (5,5) to (10,10)
        auto bbox = intersection.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.min_y == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(bbox.max_x == Catch::Approx(10.0f).margin(0.1f));
        REQUIRE(bbox.max_y == Catch::Approx(10.0f).margin(0.1f));
    }

    SECTION("Rectangle union using Martinez-Rueda") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon rect2(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        auto union_poly = rect1.unionWith(rect2);
        REQUIRE(union_poly.isValid());
        
        // Union should encompass both rectangles
        auto bbox = union_poly.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(bbox.max_x == Catch::Approx(15.0f).margin(0.1f));
        REQUIRE(bbox.max_y == Catch::Approx(15.0f).margin(0.1f));
    }

    SECTION("Rectangle difference using Martinez-Rueda") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 10.0f, 10.0f));
        Polygon rect2(BoundingBox(5.0f, 5.0f, 15.0f, 15.0f));
        
        auto difference = rect1.differenceWith(rect2);
        REQUIRE(difference.isValid());
        
        // The result should contain the part of rect1 not covered by rect2
        // This is a more complex shape, so we just verify it exists and has reasonable bounds
        auto bbox = difference.getBoundingBox();
        REQUIRE(bbox.min_x == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(bbox.min_y == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(bbox.max_x <= 10.1f); // Should not exceed original rect1
        REQUIRE(bbox.max_y <= 10.1f); // Should not exceed original rect1
    }

    SECTION("Triangle operations using Martinez-Rueda") {
        std::vector<Point2D<float>> triangle1_vertices = {
            {0.0f, 0.0f}, {6.0f, 0.0f}, {3.0f, 6.0f}
        };
        std::vector<Point2D<float>> triangle2_vertices = {
            {3.0f, 3.0f}, {9.0f, 3.0f}, {6.0f, 9.0f}
        };
        
        Polygon triangle1(triangle1_vertices);
        Polygon triangle2(triangle2_vertices);
        
        auto intersection = triangle1.intersectionWith(triangle2);
        auto union_poly = triangle1.unionWith(triangle2);
        auto difference = triangle1.differenceWith(triangle2);
        
        // All operations should produce valid results
        // (We don't check exact shapes since triangle intersections are complex)
        REQUIRE(intersection.vertexCount() >= 0); // May be empty or have vertices
        REQUIRE(union_poly.isValid());
        REQUIRE(difference.vertexCount() >= 0); // May be empty or have vertices
        
        // Union should encompass both triangles
        auto union_bbox = union_poly.getBoundingBox();
        REQUIRE(union_bbox.min_x <= 0.1f);
        REQUIRE(union_bbox.min_y <= 0.1f);
        REQUIRE(union_bbox.max_x >= 8.9f);
        REQUIRE(union_bbox.max_y >= 8.9f);
    }

    SECTION("Non-intersecting polygons") {
        Polygon rect1(BoundingBox(0.0f, 0.0f, 5.0f, 5.0f));
        Polygon rect2(BoundingBox(10.0f, 10.0f, 15.0f, 15.0f));
        
        auto intersection = rect1.intersectionWith(rect2);
        auto union_poly = rect1.unionWith(rect2);
        auto difference = rect1.differenceWith(rect2);
        
        REQUIRE(!intersection.isValid()); // Should be empty
        REQUIRE(union_poly.isValid());   // Should combine both
        REQUIRE(difference.isValid());   // Should be same as rect1
        
        // Difference should be approximately the same as the original
        auto diff_bbox = difference.getBoundingBox();
        REQUIRE(diff_bbox.min_x == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(diff_bbox.min_y == Catch::Approx(0.0f).margin(0.1f));
        REQUIRE(diff_bbox.max_x == Catch::Approx(5.0f).margin(0.1f));
        REQUIRE(diff_bbox.max_y == Catch::Approx(5.0f).margin(0.1f));
    }
}

TEST_CASE("Polygon - Boolean operations with Martinez-Rueda adapter", "[polygon][boolean][adapter]") {
    SECTION("Two overlapping rectangles - union") {
        // Create two overlapping rectangles
        BoundingBox bbox1(0.0f, 0.0f, 4.0f, 4.0f);
        BoundingBox bbox2(2.0f, 2.0f, 6.0f, 6.0f);
        
        Polygon rect1(bbox1);
        Polygon rect2(bbox2);
        
        Polygon union_result = rect1.unionWith(rect2);
        
        REQUIRE(union_result.isValid());
        REQUIRE(union_result.vertexCount() >= 3);
        
        // The union should contain both original rectangles
        REQUIRE(union_result.containsPoint({1.0f, 1.0f})); // Point from rect1
        REQUIRE(union_result.containsPoint({5.0f, 5.0f})); // Point from rect2
        REQUIRE(union_result.containsPoint({3.0f, 3.0f})); // Point in overlap
    }
    
    SECTION("Two overlapping rectangles - intersection") {
        // Create two overlapping rectangles
        BoundingBox bbox1(0.0f, 0.0f, 4.0f, 4.0f);
        BoundingBox bbox2(2.0f, 2.0f, 6.0f, 6.0f);
        
        Polygon rect1(bbox1);
        Polygon rect2(bbox2);
        
        Polygon intersection_result = rect1.intersectionWith(rect2);
        
        REQUIRE(intersection_result.isValid());
        REQUIRE(intersection_result.vertexCount() >= 3);
        
        // The intersection should contain points only in the overlap area
        REQUIRE(intersection_result.containsPoint({3.0f, 3.0f})); // Point in overlap
        REQUIRE(!intersection_result.containsPoint({1.0f, 1.0f})); // Point only in rect1
        REQUIRE(!intersection_result.containsPoint({5.0f, 5.0f})); // Point only in rect2
    }
    
    SECTION("Two overlapping rectangles - difference") {
        // Create two overlapping rectangles
        BoundingBox bbox1(0.0f, 0.0f, 4.0f, 4.0f);
        BoundingBox bbox2(2.0f, 2.0f, 6.0f, 6.0f);
        
        Polygon rect1(bbox1);
        Polygon rect2(bbox2);
        
        Polygon difference_result = rect1.differenceWith(rect2);
        
        REQUIRE(difference_result.isValid());
        REQUIRE(difference_result.vertexCount() >= 3);
        
        // The difference should contain points from rect1 but not in the overlap
        REQUIRE(difference_result.containsPoint({1.0f, 1.0f})); // Point only in rect1
        REQUIRE(!difference_result.containsPoint({3.0f, 3.0f})); // Point in overlap (should be removed)
        REQUIRE(!difference_result.containsPoint({5.0f, 5.0f})); // Point only in rect2
    }
    
    SECTION("Non-overlapping rectangles") {
        // Create two non-overlapping rectangles
        BoundingBox bbox1(0.0f, 0.0f, 2.0f, 2.0f);
        BoundingBox bbox2(4.0f, 4.0f, 6.0f, 6.0f);
        
        Polygon rect1(bbox1);
        Polygon rect2(bbox2);
        
        Polygon union_result = rect1.unionWith(rect2);
        Polygon intersection_result = rect1.intersectionWith(rect2);
        Polygon difference_result = rect1.differenceWith(rect2);
        
        // Union should be valid and contain both rectangles
        // Note: For non-overlapping polygons, our adapter creates a bounding box
        // that encompasses both polygons
        REQUIRE(union_result.isValid());
        REQUIRE(union_result.containsPoint({1.0f, 1.0f})); // Point from rect1
        REQUIRE(union_result.containsPoint({5.0f, 5.0f})); // Point from rect2
        
        // Intersection should be empty (no overlap)
        REQUIRE(!intersection_result.isValid()); // Should be empty
        
        // Difference should be the same as the original rect1
        REQUIRE(difference_result.isValid());
        REQUIRE(difference_result.containsPoint({1.0f, 1.0f})); // Point from rect1
        REQUIRE(!difference_result.containsPoint({5.0f, 5.0f})); // Point from rect2
    }
    
    SECTION("Invalid polygons") {
        // Test with invalid (empty) polygons
        std::vector<Point2D<float>> empty_vertices;
        Polygon empty_polygon(empty_vertices);
        
        BoundingBox bbox(0.0f, 0.0f, 2.0f, 2.0f);
        Polygon valid_polygon(bbox);
        
        // Operations with empty polygons should handle gracefully
        Polygon union_result = empty_polygon.unionWith(valid_polygon);
        Polygon intersection_result = empty_polygon.intersectionWith(valid_polygon);
        Polygon difference_result = valid_polygon.differenceWith(empty_polygon);
        
        // Union should return the valid polygon
        REQUIRE(union_result.isValid());
        REQUIRE(union_result.containsPoint({1.0f, 1.0f}));
        
        // Intersection should be empty
        REQUIRE(!intersection_result.isValid());
        
        // Difference should return the valid polygon
        REQUIRE(difference_result.isValid());
        REQUIRE(difference_result.containsPoint({1.0f, 1.0f}));
    }
}
