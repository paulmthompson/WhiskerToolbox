#include "transforms/Masks/mask_principal_axis.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <memory>

TEST_CASE("Mask principal axis calculation - Core functionality", "[mask][principal_axis][transform]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating principal axis from empty mask data") {
        auto result = calculate_mask_principal_axis(mask_data.get());

        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Single point mask (insufficient for principal axis)") {
        // Single point mask - should be skipped
        std::vector<uint32_t> x_coords = {5};
        std::vector<uint32_t> y_coords = {5};
        mask_data->addAtTime(TimeFrameIndex(10), x_coords, y_coords);

        auto result = calculate_mask_principal_axis(mask_data.get());

        // Should be empty since single point masks are skipped
        REQUIRE(result->getTimesWithData().empty());
    }

    SECTION("Horizontal line mask - major axis should be horizontal") {
        // Create a horizontal line of points
        std::vector<uint32_t> x_coords = {0, 1, 2, 3, 4, 5};
        std::vector<uint32_t> y_coords = {2, 2, 2, 2, 2, 2};
        mask_data->addAtTime(TimeFrameIndex(20), x_coords, y_coords);

        auto params = std::make_unique<MaskPrincipalAxisParameters>();
        params->axis_type = PrincipalAxisType::Major;

        auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

        auto const & times = result->getTimesWithData();
        REQUIRE(times.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(20));

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(20));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);// Line should have 2 points

        // For a horizontal line, major axis should be roughly horizontal
        auto const & line = lines[0];
        float dx = line[1].x - line[0].x;
        float dy = line[1].y - line[0].y;
        float angle = std::atan2(std::abs(dy), std::abs(dx));

        // Angle should be close to 0 or π (horizontal)
        REQUIRE((angle < 0.2f || angle > (M_PI - 0.2f)));// Within ~11 degrees of horizontal
    }

    SECTION("Vertical line mask - major axis should be vertical") {
        // Create a vertical line of points
        std::vector<uint32_t> x_coords = {3, 3, 3, 3, 3, 3};
        std::vector<uint32_t> y_coords = {0, 1, 2, 3, 4, 5};
        mask_data->addAtTime(TimeFrameIndex(30), x_coords, y_coords);

        auto params = std::make_unique<MaskPrincipalAxisParameters>();
        params->axis_type = PrincipalAxisType::Major;

        auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(30));
        REQUIRE(lines.size() == 1);

        // For a vertical line, major axis should be roughly vertical
        auto const & line = lines[0];
        float dx = line[1].x - line[0].x;
        float dy = line[1].y - line[0].y;
        float angle = std::atan2(std::abs(dy), std::abs(dx));

        // Angle should be close to π/2 (vertical)
        REQUIRE_THAT(angle, Catch::Matchers::WithinAbs(M_PI / 2.0f, 0.2f));// Within ~11 degrees of vertical
    }

    SECTION("Diagonal line mask - test axis calculation") {
        // Create a diagonal line of points (45 degrees)
        std::vector<uint32_t> x_coords = {0, 1, 2, 3, 4};
        std::vector<uint32_t> y_coords = {0, 1, 2, 3, 4};
        mask_data->addAtTime(TimeFrameIndex(40), x_coords, y_coords);

        auto params = std::make_unique<MaskPrincipalAxisParameters>();
        params->axis_type = PrincipalAxisType::Major;

        auto result = calculate_mask_principal_axis(mask_data.get(), params.get());

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(40));
        REQUIRE(lines.size() == 1);

        // For a 45-degree line, major axis should be roughly at 45 degrees
        auto const & line = lines[0];
        float dx = line[1].x - line[0].x;
        float dy = line[1].y - line[0].y;
        float angle = std::atan2(dy, dx);

        // Angle should be close to π/4 or -3π/4 (45 degrees or equivalent)
        float normalized_angle = std::fmod(std::abs(angle), M_PI / 2.0f);
        REQUIRE_THAT(normalized_angle, Catch::Matchers::WithinAbs(M_PI / 4.0f, 0.2f));
    }

    SECTION("Rectangle mask - test major vs minor axis") {
        // Create a rectangle that's wider than it is tall
        std::vector<uint32_t> x_coords, y_coords;
        for (uint32_t x = 0; x <= 6; ++x) {
            for (uint32_t y = 0; y <= 2; ++y) {
                x_coords.push_back(x);
                y_coords.push_back(y);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(50), x_coords, y_coords);

        // Test major axis
        auto major_params = std::make_unique<MaskPrincipalAxisParameters>();
        major_params->axis_type = PrincipalAxisType::Major;
        auto major_result = calculate_mask_principal_axis(mask_data.get(), major_params.get());

        // Test minor axis
        auto minor_params = std::make_unique<MaskPrincipalAxisParameters>();
        minor_params->axis_type = PrincipalAxisType::Minor;
        auto minor_result = calculate_mask_principal_axis(mask_data.get(), minor_params.get());

        auto const & major_lines = major_result->getLinesAtTime(TimeFrameIndex(50));
        auto const & minor_lines = minor_result->getLinesAtTime(TimeFrameIndex(50));

        REQUIRE(major_lines.size() == 1);
        REQUIRE(minor_lines.size() == 1);

        // Major axis should be more horizontal (rectangle is wider than tall)
        auto const & major_line = major_lines[0];
        float major_dx = major_line[1].x - major_line[0].x;
        float major_dy = major_line[1].y - major_line[0].y;
        float major_angle = std::atan2(std::abs(major_dy), std::abs(major_dx));

        // Minor axis should be more vertical
        auto const & minor_line = minor_lines[0];
        float minor_dx = minor_line[1].x - minor_line[0].x;
        float minor_dy = minor_line[1].y - minor_line[0].y;
        float minor_angle = std::atan2(std::abs(minor_dy), std::abs(minor_dx));

        // Major axis should be more horizontal than vertical
        REQUIRE(major_angle < M_PI / 4.0f);
        // Minor axis should be more vertical than horizontal
        REQUIRE(minor_angle > M_PI / 4.0f);
    }

    SECTION("Multiple masks at one timestamp") {
        // First mask - horizontal line
        std::vector<uint32_t> x1 = {0, 1, 2, 3};
        std::vector<uint32_t> y1 = {1, 1, 1, 1};
        mask_data->addAtTime(TimeFrameIndex(60), x1, y1);

        // Second mask - vertical line
        std::vector<uint32_t> x2 = {5, 5, 5, 5};
        std::vector<uint32_t> y2 = {0, 1, 2, 3};
        mask_data->addAtTime(TimeFrameIndex(60), x2, y2);

        auto result = calculate_mask_principal_axis(mask_data.get());

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(60));
        REQUIRE(lines.size() == 2);// Two masks = two principal axis lines
    }

    SECTION("Verify image size is preserved") {
        // Set image size on mask data
        ImageSize test_size{640, 480};
        mask_data->setImageSize(test_size);

        // Add some mask data
        std::vector<uint32_t> x = {100, 200, 300};
        std::vector<uint32_t> y = {100, 100, 100};
        mask_data->addAtTime(TimeFrameIndex(100), x, y);

        auto result = calculate_mask_principal_axis(mask_data.get());

        // Verify image size is copied
        REQUIRE(result->getImageSize().width == test_size.width);
        REQUIRE(result->getImageSize().height == test_size.height);
    }
}

TEST_CASE("Mask principal axis calculation - Edge cases", "[mask][principal_axis][transform][edge]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Two identical points (no variance)") {
        // Two identical points - should be handled gracefully
        std::vector<uint32_t> x_coords = {5, 5};
        std::vector<uint32_t> y_coords = {5, 5};
        mask_data->addAtTime(TimeFrameIndex(10), x_coords, y_coords);

        auto result = calculate_mask_principal_axis(mask_data.get());

        // Should still produce a result, even if axis direction is arbitrary
        auto const & times = result->getTimesWithData();
        if (!times.empty()) {
            auto const & lines = result->getLinesAtTime(TimeFrameIndex(10));
            REQUIRE(lines.size() <= 1);// Should produce at most one line
        }
    }

    SECTION("Circular point distribution") {
        // Create points in a rough circle
        std::vector<uint32_t> x_coords, y_coords;
        int center_x = 10, center_y = 10, radius = 5;

        for (int angle_deg = 0; angle_deg < 360; angle_deg += 30) {
            float angle_rad = angle_deg * M_PI / 180.0f;
            int x = center_x + static_cast<int>(radius * std::cos(angle_rad));
            int y = center_y + static_cast<int>(radius * std::sin(angle_rad));
            x_coords.push_back(static_cast<uint32_t>(x));
            y_coords.push_back(static_cast<uint32_t>(y));
        }

        mask_data->addAtTime(TimeFrameIndex(20), x_coords, y_coords);

        auto result = calculate_mask_principal_axis(mask_data.get());

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(20));
        REQUIRE(lines.size() == 1);

        // For a circle, major and minor axes should be similar in magnitude
        // (though direction may be arbitrary due to symmetry)
        REQUIRE(lines[0].size() == 2);
    }

    SECTION("Large coordinates") {
        // Test with large coordinate values
        std::vector<uint32_t> x = {1000000, 1000001, 1000002};
        std::vector<uint32_t> y = {2000000, 2000000, 2000000};
        mask_data->addAtTime(TimeFrameIndex(30), x, y);

        auto result = calculate_mask_principal_axis(mask_data.get());

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(30));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);
    }

    SECTION("Null input handling") {
        auto result = calculate_mask_principal_axis(nullptr);
        REQUIRE(result->getTimesWithData().empty());
    }
}

TEST_CASE("MaskPrincipalAxisOperation - Operation interface", "[mask][principal_axis][operation]") {
    MaskPrincipalAxisOperation operation;

    SECTION("Operation name") {
        REQUIRE(operation.getName() == "Calculate Mask Principal Axis");
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

        auto * specific_params = dynamic_cast<MaskPrincipalAxisParameters *>(params.get());
        REQUIRE(specific_params != nullptr);
        REQUIRE(specific_params->axis_type == PrincipalAxisType::Major);// Default should be major
    }

    SECTION("Execute operation with major axis") {
        auto mask_data = std::make_shared<MaskData>();

        // Add test data - horizontal line
        std::vector<uint32_t> x = {0, 1, 2, 3, 4};
        std::vector<uint32_t> y = {2, 2, 2, 2, 2};
        mask_data->addAtTime(TimeFrameIndex(50), x, y);

        DataTypeVariant input_variant = mask_data;

        auto params = std::make_unique<MaskPrincipalAxisParameters>();
        params->axis_type = PrincipalAxisType::Major;

        auto result_variant = operation.execute(input_variant, params.get());

        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));

        auto result = std::get<std::shared_ptr<LineData>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(50));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);
    }

    SECTION("Execute operation with minor axis") {
        auto mask_data = std::make_shared<MaskData>();

        // Add test data - vertical line
        std::vector<uint32_t> x = {3, 3, 3, 3, 3};
        std::vector<uint32_t> y = {0, 1, 2, 3, 4};
        mask_data->addAtTime(TimeFrameIndex(60), x, y);

        DataTypeVariant input_variant = mask_data;

        auto params = std::make_unique<MaskPrincipalAxisParameters>();
        params->axis_type = PrincipalAxisType::Minor;

        auto result_variant = operation.execute(input_variant, params.get());

        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(result_variant));

        auto result = std::get<std::shared_ptr<LineData>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & lines = result->getLinesAtTime(TimeFrameIndex(60));
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 2);
    }
}