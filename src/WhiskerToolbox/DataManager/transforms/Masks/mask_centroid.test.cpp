#include "transforms/Masks/mask_centroid.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

TEST_CASE("Mask centroid calculation - Core functionality", "[mask][centroid][transform]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating centroid from empty mask data") {
        auto result = calculate_mask_centroid(mask_data.get());

        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Calculating centroid from single mask at one timestamp") {
        // Create a simple mask (3 points forming a triangle)
        std::vector<uint32_t> x_coords = {0, 3, 0};
        std::vector<uint32_t> y_coords = {0, 0, 3};
        mask_data->addAtTime(10, x_coords, y_coords);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & times = result->getTimesWithData();
        REQUIRE(times.size() == 1);
        REQUIRE(times[0] == 10);

        auto const & points = result->getPointsAtTime(10);
        REQUIRE(points.size() == 1);

        // Centroid of triangle with vertices (0,0), (3,0), (0,3) should be (1,1)
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }

    SECTION("Calculating centroid from multiple masks at one timestamp") {
        // First mask (simple square)
        std::vector<uint32_t> x1 = {0, 1, 0, 1};
        std::vector<uint32_t> y1 = {0, 0, 1, 1};
        mask_data->addAtTime(20, x1, y1);

        // Second mask (offset square)
        std::vector<uint32_t> x2 = {4, 5, 4, 5};
        std::vector<uint32_t> y2 = {4, 4, 5, 5};
        mask_data->addAtTime(20, x2, y2);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & times = result->getTimesWithData();
        REQUIRE(times.size() == 1);
        REQUIRE(times[0] == 20);

        auto const & points = result->getPointsAtTime(20);
        REQUIRE(points.size() == 2);// Two masks = two centroids

        // Sort points by x coordinate for consistent testing
        auto sorted_points = points;
        std::sort(sorted_points.begin(), sorted_points.end(),
                  [](auto const & a, auto const & b) { return a.x < b.x; });

        // First centroid: (0+1+0+1)/4 = 0.5, (0+0+1+1)/4 = 0.5
        REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(0.5f, 0.001f));

        // Second centroid: (4+5+4+5)/4 = 4.5, (4+4+5+5)/4 = 4.5
        REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(4.5f, 0.001f));
        REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(4.5f, 0.001f));
    }

    SECTION("Calculating centroids from masks across multiple timestamps") {
        // Timestamp 30: One mask (line of 3 points)
        std::vector<uint32_t> x1 = {0, 2, 4};
        std::vector<uint32_t> y1 = {0, 0, 0};
        mask_data->addAtTime(30, x1, y1);

        // Timestamp 40: One mask (vertical line)
        std::vector<uint32_t> x2 = {1, 1, 1};
        std::vector<uint32_t> y2 = {0, 3, 6};
        mask_data->addAtTime(40, x2, y2);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & times = result->getTimesWithData();
        REQUIRE(times.size() == 2);

        // Check timestamp 30 centroid: (0+2+4)/3 = 2, (0+0+0)/3 = 0
        auto const & points30 = result->getPointsAtTime(30);
        REQUIRE(points30.size() == 1);
        REQUIRE_THAT(points30[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points30[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));

        // Check timestamp 40 centroid: (1+1+1)/3 = 1, (0+3+6)/3 = 3
        auto const & points40 = result->getPointsAtTime(40);
        REQUIRE(points40.size() == 1);
        REQUIRE_THAT(points40[0].x, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(points40[0].y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    }

    SECTION("Verify image size is preserved") {
        // Set image size on mask data
        ImageSize test_size{640, 480};
        mask_data->setImageSize(test_size);

        // Add some mask data
        std::vector<uint32_t> x = {100, 200, 300};
        std::vector<uint32_t> y = {100, 150, 200};
        mask_data->addAtTime(100, x, y);

        auto result = calculate_mask_centroid(mask_data.get());

        // Verify image size is copied
        REQUIRE(result->getImageSize().width == test_size.width);
        REQUIRE(result->getImageSize().height == test_size.height);

        // Verify calculation is correct: (100+200+300)/3 = 200, (100+150+200)/3 = 150
        auto const & points = result->getPointsAtTime(100);
        REQUIRE(points.size() == 1);
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(200.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
    }
}

TEST_CASE("Mask centroid calculation - Edge cases and error handling", "[mask][centroid][transform][edge]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Masks with zero points") {
        // Add an empty mask (should be handled gracefully)
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(10, empty_x, empty_y);

        auto result = calculate_mask_centroid(mask_data.get());

        // Empty masks should be skipped
        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Mixed empty and non-empty masks") {
        // Add an empty mask
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;
        mask_data->addAtTime(20, empty_x, empty_y);

        // Add a non-empty mask at the same timestamp
        std::vector<uint32_t> x = {2, 4};
        std::vector<uint32_t> y = {1, 3};
        mask_data->addAtTime(20, x, y);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & times = result->getTimesWithData();
        REQUIRE(times.size() == 1);
        REQUIRE(times[0] == 20);

        auto const & points = result->getPointsAtTime(20);
        REQUIRE(points.size() == 1);// Only counts the non-empty mask

        // Centroid: (2+4)/2 = 3, (1+3)/2 = 2
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(3.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    }

    SECTION("Single point masks") {
        // Each mask has only one point
        std::vector<uint32_t> x1 = {5};
        std::vector<uint32_t> y1 = {7};
        mask_data->addAtTime(30, x1, y1);

        std::vector<uint32_t> x2 = {10};
        std::vector<uint32_t> y2 = {15};
        mask_data->addAtTime(30, x2, y2);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & points = result->getPointsAtTime(30);
        REQUIRE(points.size() == 2);

        // Sort points by x coordinate for consistent testing
        auto sorted_points = points;
        std::sort(sorted_points.begin(), sorted_points.end(),
                  [](auto const & a, auto const & b) { return a.x < b.x; });

        // First mask centroid should be exactly the single point
        REQUIRE_THAT(sorted_points[0].x, Catch::Matchers::WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(sorted_points[0].y, Catch::Matchers::WithinAbs(7.0f, 0.001f));

        // Second mask centroid should be exactly the single point
        REQUIRE_THAT(sorted_points[1].x, Catch::Matchers::WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(sorted_points[1].y, Catch::Matchers::WithinAbs(15.0f, 0.001f));
    }

    SECTION("Large coordinates") {
        // Test with large coordinate values to ensure no overflow
        std::vector<uint32_t> x = {1000000, 1000001, 1000002};
        std::vector<uint32_t> y = {2000000, 2000001, 2000002};
        mask_data->addAtTime(40, x, y);

        auto result = calculate_mask_centroid(mask_data.get());

        auto const & points = result->getPointsAtTime(40);
        REQUIRE(points.size() == 1);

        // Centroid: (1000000+1000001+1000002)/3 = 1000001, (2000000+2000001+2000002)/3 = 2000001
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(1000001.0f, 0.1f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(2000001.0f, 0.1f));
    }

    SECTION("Null input handling") {
        auto result = calculate_mask_centroid(nullptr);
        REQUIRE(result->getTimesWithData().empty());
    }
}

TEST_CASE("MaskCentroidOperation - Operation interface", "[mask][centroid][operation]") {
    MaskCentroidOperation operation;

    SECTION("Operation name") {
        REQUIRE(operation.getName() == "Calculate Mask Centroid");
    }

    SECTION("Target type index") {
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
    }

    SECTION("Can apply to valid MaskData") {
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant variant = mask_data;
        REQUIRE(operation.canApply(variant));
    }

    SECTION("Cannot apply to null MaskData") {
        std::shared_ptr<MaskData> null_mask;
        DataTypeVariant variant = null_mask;
        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("Default parameters") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params != nullptr);
        REQUIRE(dynamic_cast<MaskCentroidParameters *>(params.get()) != nullptr);
    }

    SECTION("Execute operation") {
        auto mask_data = std::make_shared<MaskData>();

        // Add test data
        std::vector<uint32_t> x = {0, 2, 4};
        std::vector<uint32_t> y = {0, 0, 0};
        mask_data->addAtTime(50, x, y);

        DataTypeVariant input_variant = mask_data;
        auto params = operation.getDefaultParameters();

        auto result_variant = operation.execute(input_variant, params.get());

        REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));

        auto result = std::get<std::shared_ptr<PointData>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & points = result->getPointsAtTime(50);
        REQUIRE(points.size() == 1);
        REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
}