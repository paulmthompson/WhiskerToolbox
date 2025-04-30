
#include "transforms/Masks/mask_area.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>

TEST_CASE("Mask area calculation - Core functionality", "[mask][area][transform]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating area from empty mask data") {
        auto result = area(mask_data);

        REQUIRE(result->getAnalogTimeSeries().empty());
        REQUIRE(result->getTimeSeries().empty());
    }

    SECTION("Calculating area from single mask at one timestamp") {
        // Create a simple mask (3 points)
        std::vector<float> x_coords = {1.0f, 2.0f, 3.0f};
        std::vector<float> y_coords = {1.0f, 2.0f, 3.0f};
        mask_data->addMaskAtTime(10, x_coords, y_coords);

        auto result = area(mask_data);

        auto& values = result->getAnalogTimeSeries();
        auto& times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == 10);
        REQUIRE(values[0] == 3.0f); // 3 points in the mask
    }

    SECTION("Calculating area from multiple masks at one timestamp") {
        // First mask (3 points)
        std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
        std::vector<float> y1 = {1.0f, 2.0f, 3.0f};
        mask_data->addMaskAtTime(20, x1, y1);

        // Second mask (5 points)
        std::vector<float> x2 = {4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        std::vector<float> y2 = {4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        mask_data->addMaskAtTime(20, x2, y2);

        auto result = area(mask_data);

        auto& values = result->getAnalogTimeSeries();
        auto& times = result->getTimeSeries();

        REQUIRE(times.size() == 1);
        REQUIRE(values.size() == 1);
        REQUIRE(times[0] == 20);
        REQUIRE(values[0] == 8.0f); // 3 + 5 = 8 points total
    }

    SECTION("Calculating areas from masks across multiple timestamps") {
        // Timestamp 30: One mask with 2 points
        std::vector<float> x1 = {1.0f, 2.0f};
        std::vector<float> y1 = {1.0f, 2.0f};
        mask_data->addMaskAtTime(30, x1, y1);

        // Timestamp 40: Two masks with 3 and 4 points
        std::vector<float> x2 = {1.0f, 2.0f, 3.0f};
        std::vector<float> y2 = {1.0f, 2.0f, 3.0f};
        mask_data->addMaskAtTime(40, x2, y2);

        std::vector<float> x3 = {4.0f, 5.0f, 6.0f, 7.0f};
        std::vector<float> y3 = {4.0f, 5.0f, 6.0f, 7.0f};
        mask_data->addMaskAtTime(40, x3, y3);

        // Timestamp 50: No masks (empty)
        mask_data->clearMasksAtTime(50);

        auto result = area(mask_data);

        auto& values = result->getAnalogTimeSeries();
        auto& times = result->getTimeSeries();

        REQUIRE(times.size() == 3);
        REQUIRE(values.size() == 3);

        // Check timestamp 30
        auto time30_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), 30));
        REQUIRE(values[time30_idx] == 2.0f);

        // Check timestamp 40
        auto time40_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), 40));
        REQUIRE(values[time40_idx] == 7.0f); // 3 + 4 = 7 points

        // Check timestamp 50
        auto time50_idx = std::distance(times.begin(), std::find(times.begin(), times.end(), 50));
        REQUIRE(values[time50_idx] == 0.0f); // Empty mask
    }

    SECTION("Verify returned AnalogTimeSeries structure") {
        // Add some mask data
        std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f};
        std::vector<float> y = {1.0f, 2.0f, 3.0f, 4.0f};
        mask_data->addMaskAtTime(100, x, y);

        auto result = area(mask_data);

        // Verify it's a proper AnalogTimeSeries
        REQUIRE(result != nullptr);
        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);

        // Statistics should work on the result
        REQUIRE(result->getMeanValue() == 4.0f);
        REQUIRE(result->getMinValue() == 4.0f);
        REQUIRE(result->getMaxValue() == 4.0f);
    }
}

TEST_CASE("Mask area calculation - Edge cases and error handling", "[mask][area][transform][edge]") {
    auto mask_data = std::make_shared<MaskData>();

    SECTION("Calculating with null mask data") {
        std::shared_ptr<MaskData> null_mask;
        auto result = area(null_mask);

        REQUIRE(result != nullptr);
        REQUIRE(result->getAnalogTimeSeries().empty());
    }

    SECTION("Masks with zero points") {
        // Add an empty mask (should be handled gracefully)
        std::vector<float> empty_x;
        std::vector<float> empty_y;
        mask_data->addMaskAtTime(10, empty_x, empty_y);

        auto result = area(mask_data);

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE(result->getAnalogTimeSeries()[0] == 0.0f);
    }

    SECTION("Mixed empty and non-empty masks") {
        // Add an empty mask
        std::vector<float> empty_x;
        std::vector<float> empty_y;
        mask_data->addMaskAtTime(20, empty_x, empty_y);

        // Add a non-empty mask at the same timestamp
        std::vector<float> x = {1.0f, 2.0f, 3.0f};
        std::vector<float> y = {1.0f, 2.0f, 3.0f};
        mask_data->addMaskAtTime(20, x, y);

        auto result = area(mask_data);

        REQUIRE(result->getAnalogTimeSeries().size() == 1);
        REQUIRE(result->getTimeSeries().size() == 1);
        REQUIRE(result->getAnalogTimeSeries()[0] == 3.0f); // Only counts the non-empty mask
    }

    SECTION("Large number of masks and points") {
        // Create a timestamp with many small masks
        for (int i = 0; i < 10; i++) {
            std::vector<float> x(i+1, 1.0f);
            std::vector<float> y(i+1, 1.0f);
            mask_data->addMaskAtTime(30, x, y);
        }

        auto result = area(mask_data);

        // Sum of 1 + 2 + 3 + ... + 10 = 55
        REQUIRE(result->getAnalogTimeSeries()[0] == 55.0f);
    }
}