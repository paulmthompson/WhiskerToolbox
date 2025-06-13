#include "transforms/Lines/line_min_point_dist.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

TEST_CASE("Line to point minimum distance calculation - Core functionality", "[line][point][distance][transform]") {
    auto line_data = std::make_shared<LineData>();
    auto point_data = std::make_shared<PointData>();

    SECTION("Basic distance calculation between a line and a point") {
        // Create a simple horizontal line from (0,0) to (10,0)
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 10;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create a point at (5, 5) - should be 5 units away from the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtIndex(0), Catch::Matchers::WithinAbs(5.0f, 0.001f));
        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(timestamp));
    }

    SECTION("Multiple points with different distances") {
        // Create a simple vertical line from (5,0) to (5,10)
        std::vector<float> line_x = {5.0f, 5.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        int timestamp = 20;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create multiple points at different distances from the line
        std::vector<Point2D<float>> points = {
                Point2D<float>{0.0f, 5.0f},    // 5 units away
                Point2D<float>{8.0f, 5.0f},    // 3 units away
                Point2D<float>{5.0f, 15.0f},   // 5 units away
                Point2D<float>{6.0f, 8.0f}     // 1 unit away
        };
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result - minimum distance should be 1.0
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtIndex(0), Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(timestamp));
    }

    SECTION("Multiple timesteps with lines and points") {
        // Create lines at different timestamps
        // Timestamp 30: Line from (0,0) to (10,0)
        std::vector<float> line_x1 = {0.0f, 10.0f};
        std::vector<float> line_y1 = {0.0f, 0.0f};
        line_data->addLineAtTime(30, line_x1, line_y1);

        // Timestamp 40: Line from (0,0) to (0,10)
        std::vector<float> line_x2 = {0.0f, 0.0f};
        std::vector<float> line_y2 = {0.0f, 10.0f};
        line_data->addLineAtTime(40, line_x2, line_y2);

        // Create points at different timestamps
        // Timestamp 30: Point at (5,2) - should be 2 units away
        std::vector<Point2D<float>> points1 = {Point2D<float>{5.0f, 2.0f}};
        point_data->overwritePointsAtTime(30, points1);

        // Timestamp 40: Point at (3,5) - should be 3 units away
        std::vector<Point2D<float>> points2 = {Point2D<float>{3.0f, 5.0f}};
        point_data->overwritePointsAtTime(40, points2);

        // Timestamp 50: No line data - should be skipped
        std::vector<Point2D<float>> points3 = {Point2D<float>{1.0f, 1.0f}};
        point_data->overwritePointsAtTime(50, points3);

        // Calculate the minimum distances
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 2); // Only timesteps 30 and 40 should have results

        // Find the indices for both timestamps
        size_t idx30 = 0;
        size_t idx40 = 1;
        if (result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(0)) == TimeFrameIndex(40)) {
            idx30 = 1;
            idx40 = 0;
        }

        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx30)) == TimeFrameIndex(30));
        REQUIRE_THAT(result->getDataAtIndex(idx30), Catch::Matchers::WithinAbs(2.0f, 0.001f));

        REQUIRE(result->getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(idx40)) == TimeFrameIndex(40));
        REQUIRE_THAT(result->getDataAtIndex(idx40), Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Scaling points with different image sizes") {
        // Set different image sizes for line and point data
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{50, 50}); // Half the resolution

        // Create a line from (0,0) to (100,0) in the line's coordinate space
        std::vector<float> line_x = {0.0f, 100.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 60;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create a point at (25, 10) in the point's coordinate space
        // After scaling to line coordinates, this should be at (50, 20)
        // Distance to the line should be 20 units
        std::vector<Point2D<float>> points = {Point2D<float>{25.0f, 10.0f}};
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance with scaling
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtIndex(0), Catch::Matchers::WithinAbs(20.0f, 0.001f));
    }

    SECTION("Point directly on the line has zero distance") {
        // Create a diagonal line from (0,0) to (10,10)
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 10.0f};
        int timestamp = 70;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create a point at (5, 5) - exactly on the line
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtIndex(0), Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}

TEST_CASE("Line to point minimum distance calculation - Edge cases and error handling", "[line][point][distance][edge]") {
    auto line_data = std::make_shared<LineData>();
    auto point_data = std::make_shared<PointData>();

    SECTION("Empty line data results in empty output") {
        // Create only point data
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->overwritePointsAtTime(10, points);

        // Calculate the minimum distance with no line data
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Empty point data results in empty output") {
        // Create only line data
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->addLineAtTime(10, line_x, line_y);

        // Calculate the minimum distance with no point data
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("No matching timestamps between line and point data") {
        // Line at timestamp 20
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->addLineAtTime(20, line_x, line_y);

        // Point at timestamp 30 (different from line)
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->overwritePointsAtTime(30, points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is empty
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Line with only one point (invalid)") {
        // Create a "line" with only one point
        std::vector<float> line_x = {5.0f};
        std::vector<float> line_y = {5.0f};
        int timestamp = 40;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create a point
        std::vector<Point2D<float>> points = {Point2D<float>{10.0f, 10.0f}};
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify no result is produced for this timestamp
        REQUIRE(result->getNumSamples() == 0);
    }

    SECTION("Invalid image sizes") {
        // Set invalid image sizes
        line_data->setImageSize(ImageSize{100, 100});
        point_data->setImageSize(ImageSize{-1, -1}); // Invalid

        // Create a line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        int timestamp = 50;
        line_data->addLineAtTime(timestamp, line_x, line_y);

        // Create a point
        std::vector<Point2D<float>> points = {Point2D<float>{5.0f, 5.0f}};
        point_data->overwritePointsAtTime(timestamp, points);

        // Calculate the minimum distance - should use original point coordinates
        auto result = line_min_point_dist(line_data.get(), point_data.get());

        // Verify the result is calculated without scaling
        REQUIRE(result->getNumSamples() == 1);
        REQUIRE_THAT(result->getDataAtIndex(0), Catch::Matchers::WithinAbs(5.0f, 0.001f));
    }

    SECTION("Transform operation with null point data in parameters") {
        // Create a line
        std::vector<float> line_x = {0.0f, 10.0f};
        std::vector<float> line_y = {0.0f, 0.0f};
        line_data->addLineAtTime(60, line_x, line_y);

        // Setup the operation and parameters
        LineMinPointDistOperation operation;
        DataTypeVariant line_variant = line_data;
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = nullptr; // Null point data

        // Execute the operation
        DataTypeVariant result = operation.execute(line_variant, params.get());

        // Verify the operation returns an empty variant when point_data is null
        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }

    SECTION("Transform operation with incorrect input type") {
        // Setup the operation with wrong input type (point data instead of line data)
        LineMinPointDistOperation operation;
        DataTypeVariant point_variant = point_data; // Wrong input type
        auto params = std::make_unique<LineMinPointDistParameters>();
        params->point_data = point_data;

        // Execute the operation
        DataTypeVariant result = operation.execute(point_variant, params.get());

        // Verify the operation returns an empty variant for wrong input
        REQUIRE(!std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result));
    }
}
