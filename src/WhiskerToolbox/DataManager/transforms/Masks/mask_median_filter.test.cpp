#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_median_filter.hpp"
#include "Masks/Mask_Data.hpp"
#include "DataManagerTypes.hpp"

TEST_CASE("MaskMedianFilter basic functionality", "[mask_median_filter]") {
    
    SECTION("filters noise from simple mask") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a 4x4 solid square
        std::vector<Point2D<uint32_t>> solid_square;
        for (uint32_t row = 3; row < 7; ++row) {
            for (uint32_t col = 3; col < 7; ++col) {
                solid_square.emplace_back(col, row);
            }
        }
        
        // Add isolated noise pixels
        solid_square.emplace_back(0, 0); // Top-left corner
        solid_square.emplace_back(9, 9); // Bottom-right corner
        solid_square.emplace_back(1, 8); // Isolated pixel
        
        mask_data->addAtTime(TimeFrameIndex(0), solid_square);
        
        MaskMedianFilterParameters params;
        params.window_size = 3;
        auto result = apply_median_filter(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().size() == 1);
        
        auto filtered_masks = result->getAtTime(TimeFrameIndex(0));
        REQUIRE(filtered_masks.size() == 1);
        
        // The solid square should mostly survive, isolated noise should be reduced
        REQUIRE(filtered_masks[0].size() > 0);
        REQUIRE(filtered_masks[0].size() < solid_square.size()); // Some noise removed
        
        // Check that core structure is preserved
        bool found_core = false;
        for (auto const& point : filtered_masks[0]) {
            if (point.x >= 4 && point.x <= 5 && point.y >= 4 && point.y <= 5) {
                found_core = true;
                break;
            }
        }
        REQUIRE(found_core); // Core of solid square should be preserved
    }
    
    SECTION("handles different window sizes") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({11, 11});
        
        // Create a 5x5 solid square with some noise
        std::vector<Point2D<uint32_t>> pattern;
        for (uint32_t row = 3; row < 8; ++row) {
            for (uint32_t col = 3; col < 8; ++col) {
                pattern.emplace_back(col, row);
            }
        }
        
        // Add noise
        pattern.emplace_back(0, 0);
        pattern.emplace_back(10, 10);
        pattern.emplace_back(0, 10);
        pattern.emplace_back(10, 0);
        
        mask_data->addAtTime(TimeFrameIndex(1), pattern);
        
        // Test different window sizes
        MaskMedianFilterParameters params_3x3;
        params_3x3.window_size = 3;
        auto result_3x3 = apply_median_filter(mask_data.get(), &params_3x3);
        
        MaskMedianFilterParameters params_5x5;
        params_5x5.window_size = 5;
        auto result_5x5 = apply_median_filter(mask_data.get(), &params_5x5);
        
        REQUIRE(result_3x3);
        REQUIRE(result_5x5);
        
        auto masks_3x3 = result_3x3->getAtTime(TimeFrameIndex(1));
        auto masks_5x5 = result_5x5->getAtTime(TimeFrameIndex(1));
        
        REQUIRE(masks_3x3.size() == 1);
        REQUIRE(masks_5x5.size() == 1);
        
        // Different window sizes should produce different results
        REQUIRE(masks_3x3[0].size() != masks_5x5[0].size());
    }
    
    SECTION("preserves large solid regions") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({12, 12});
        
        // Create a large 6x6 solid square (should survive median filtering)
        std::vector<Point2D<uint32_t>> large_square;
        for (uint32_t row = 3; row < 9; ++row) {
            for (uint32_t col = 3; col < 9; ++col) {
                large_square.emplace_back(col, row);
            }
        }
        
        mask_data->addAtTime(TimeFrameIndex(2), large_square);
        
        MaskMedianFilterParameters params;
        params.window_size = 3;
        auto result = apply_median_filter(mask_data.get(), &params);
        
        REQUIRE(result);
        auto filtered_masks = result->getAtTime(TimeFrameIndex(2));
        REQUIRE(filtered_masks.size() == 1);
        
        // Large solid square should be well preserved
        REQUIRE(filtered_masks[0].size() >= 16); // Should preserve substantial core
        
        // Center should definitely be preserved
        bool found_center = false;
        for (auto const& point : filtered_masks[0]) {
            if (point.x >= 5 && point.x <= 6 && point.y >= 5 && point.y <= 6) {
                found_center = true;
                break;
            }
        }
        REQUIRE(found_center);
    }
    
    SECTION("handles multiple masks at same time") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({15, 10});
        
        // First mask: solid 3x3 square on left
        std::vector<Point2D<uint32_t>> square1;
        for (uint32_t row = 3; row < 6; ++row) {
            for (uint32_t col = 2; col < 5; ++col) {
                square1.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(3), square1);
        
        // Second mask: solid 3x3 square on right
        std::vector<Point2D<uint32_t>> square2;
        for (uint32_t row = 3; row < 6; ++row) {
            for (uint32_t col = 10; col < 13; ++col) {
                square2.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(3), square2);
        
        // Third mask: noise pixels
        std::vector<Point2D<uint32_t>> noise;
        noise.emplace_back(0, 0);
        noise.emplace_back(14, 9);
        noise.emplace_back(7, 1);
        mask_data->addAtTime(TimeFrameIndex(3), noise);
        
        MaskMedianFilterParameters params;
        params.window_size = 3;
        auto result = apply_median_filter(mask_data.get(), &params);
        
        REQUIRE(result);
        auto filtered_masks = result->getAtTime(TimeFrameIndex(3));
        
        // The two solid squares should survive, but the noise mask should be filtered out
        REQUIRE(filtered_masks.size() == 2);
        
        // Count total points in filtered vs original
        size_t total_filtered = 0;
        for (auto const& mask : filtered_masks) {
            total_filtered += mask.size();
        }
        
        size_t total_original = square1.size() + square2.size() + noise.size();
        
        // Should have removed the noise (but preserved the solid squares)
        REQUIRE(total_filtered < total_original);
        REQUIRE(total_filtered > 0); // But not everything
        
        // Check that both solid squares have substantial content preserved
        REQUIRE(filtered_masks[0].size() > 4); // Should preserve most of the 3x3 square
        REQUIRE(filtered_masks[1].size() > 4); // Should preserve most of the 3x3 square
    }
    
    SECTION("handles empty mask data") {
        auto mask_data = std::make_shared<MaskData>();
        
        MaskMedianFilterParameters params;
        auto result = apply_median_filter(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("handles null input") {
        MaskMedianFilterParameters params;
        auto result = apply_median_filter(nullptr, &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("validates window size parameters") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create larger 4x4 square that can survive median filtering
        std::vector<Point2D<uint32_t>> square;
        for (uint32_t row = 3; row < 7; ++row) {
            for (uint32_t col = 3; col < 7; ++col) {
                square.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(0), square);
        
        // Test invalid even window size (should use default)
        MaskMedianFilterParameters params_even;
        params_even.window_size = 4;
        auto result_even = apply_median_filter(mask_data.get(), &params_even);
        
        // Test invalid zero window size (should use default)
        MaskMedianFilterParameters params_zero;
        params_zero.window_size = 0;
        auto result_zero = apply_median_filter(mask_data.get(), &params_zero);
        
        // Test negative window size (should use default)
        MaskMedianFilterParameters params_neg;
        params_neg.window_size = -1;
        auto result_neg = apply_median_filter(mask_data.get(), &params_neg);
        
        // All should succeed (use defaults) and return valid results
        REQUIRE(result_even);
        REQUIRE(result_zero);
        REQUIRE(result_neg);
        
        REQUIRE(result_even->getTimesWithData().size() == 1);
        REQUIRE(result_zero->getTimesWithData().size() == 1);
        REQUIRE(result_neg->getTimesWithData().size() == 1);
    }
}

TEST_CASE("MaskMedianFilterOperation interface tests", "[mask_median_filter][operation]") {
    
    SECTION("operation metadata") {
        MaskMedianFilterOperation op;
        
        REQUIRE(op.getName() == "Apply Median Filter");
        REQUIRE(op.getTargetInputTypeIndex() == std::type_index(typeid(std::shared_ptr<MaskData>)));
        
        auto default_params = op.getDefaultParameters();
        REQUIRE(default_params != nullptr);
        auto median_filter_params = dynamic_cast<MaskMedianFilterParameters*>(default_params.get());
        REQUIRE(median_filter_params != nullptr);
        REQUIRE(median_filter_params->window_size == 3); // Default window size
    }
    
    SECTION("canApply method") {
        MaskMedianFilterOperation op;
        
        // Test with valid MaskData
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant valid_variant = mask_data;
        REQUIRE(op.canApply(valid_variant));
        
        // Test with null MaskData
        std::shared_ptr<MaskData> null_mask_data = nullptr;
        DataTypeVariant null_variant = null_mask_data;
        REQUIRE_FALSE(op.canApply(null_variant));

    }
    
    SECTION("execute with valid input") {
        MaskMedianFilterOperation op;
        
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a pattern with noise
        std::vector<Point2D<uint32_t>> pattern;
        
        // 5x5 solid square that can survive median filtering
        for (uint32_t row = 2; row < 7; ++row) {
            for (uint32_t col = 2; col < 7; ++col) {
                pattern.emplace_back(col, row);
            }
        }
        
        // Add noise
        pattern.emplace_back(0, 0);
        pattern.emplace_back(9, 9);
        
        mask_data->addAtTime(TimeFrameIndex(0), pattern);
        
        DataTypeVariant input_variant = mask_data;
        MaskMedianFilterParameters params;
        params.window_size = 3;
        
        auto result_variant = op.execute(input_variant, &params, [](int){});
        
        REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
        
        auto result_mask_data = std::get<std::shared_ptr<MaskData>>(result_variant);
        REQUIRE(result_mask_data);
        
        auto result_masks = result_mask_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(result_masks.size() == 1);
        
        // Should have reduced noise (fewer total pixels)
        REQUIRE(result_masks[0].size() < pattern.size());
        REQUIRE(result_masks[0].size() > 0); // But not empty
    }
    
    SECTION("execute with invalid parameters") {
        MaskMedianFilterOperation op;
        
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({8, 8});
        
        // Add 3x3 solid square that can survive median filtering
        std::vector<Point2D<uint32_t>> simple_mask;
        for (uint32_t row = 2; row < 5; ++row) {
            for (uint32_t col = 2; col < 5; ++col) {
                simple_mask.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(TimeFrameIndex(0), simple_mask);
        
        DataTypeVariant input_variant = mask_data;
        
        // Test with wrong parameter type (should use defaults)
        auto result_variant = op.execute(input_variant, nullptr, [](int){});
        
        REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
        
        auto result_mask_data = std::get<std::shared_ptr<MaskData>>(result_variant);
        REQUIRE(result_mask_data);
        
        // Should handle gracefully and return valid result
        auto result_masks = result_mask_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(result_masks.size() == 1);
        REQUIRE(result_masks[0].size() > 0); // Should have some content preserved
    }
} 