#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_hole_filling.hpp"
#include "Masks/Mask_Data.hpp"
#include "DataManagerTypes.hpp"

TEST_CASE("MaskHoleFilling basic functionality", "[mask_hole_filling]") {
    
    SECTION("fills holes in simple rectangular mask") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a hollow rectangle with hole in the middle
        std::vector<Point2D<uint32_t>> hollow_rect;
        
        // Outer border
        for (uint32_t row = 2; row < 8; ++row) {
            for (uint32_t col = 2; col < 8; ++col) {
                if (row == 2 || row == 7 || col == 2 || col == 7) {
                    hollow_rect.emplace_back(col, row);
                }
            }
        }
        
        mask_data->addAtTime(0, hollow_rect);
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().size() == 1);
        
        auto filled_masks = result->getAtTime(0);
        REQUIRE(filled_masks.size() == 1);
        
        // The hole should now be filled - should have more points than original
        REQUIRE(filled_masks[0].size() > hollow_rect.size());
        
        // Check that interior points are now present
        bool found_interior = false;
        for (auto const& point : filled_masks[0]) {
            if (point.x == 4 && point.y == 4) { // Center should be filled
                found_interior = true;
                break;
            }
        }
        REQUIRE(found_interior);
    }
    
    SECTION("preserves solid masks without holes") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({8, 8});
        
        // Create a solid 3x3 square
        std::vector<Point2D<uint32_t>> solid_square;
        for (uint32_t row = 2; row < 5; ++row) {
            for (uint32_t col = 2; col < 5; ++col) {
                solid_square.emplace_back(col, row);
            }
        }
        
        mask_data->addAtTime(1, solid_square);
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().size() == 1);
        
        auto result_masks = result->getAtTime(1);
        REQUIRE(result_masks.size() == 1);
        
        // Should have same number of points (no holes to fill)
        REQUIRE(result_masks[0].size() == solid_square.size());
    }
    
    SECTION("handles multiple masks at same time") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({12, 8});
        
        // First mask: hollow rectangle (4x4 with hole in middle)
        std::vector<Point2D<uint32_t>> hollow_rect;
        for (uint32_t row = 1; row < 5; ++row) {
            for (uint32_t col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    hollow_rect.emplace_back(col, row);
                }
            }
        }
        mask_data->addAtTime(2, hollow_rect);
        
        // Second mask: small solid 2x2 square
        std::vector<Point2D<uint32_t>> solid_square;
        for (uint32_t row = 1; row < 3; ++row) {
            for (uint32_t col = 7; col < 9; ++col) {
                solid_square.emplace_back(col, row);
            }
        }
        mask_data->addAtTime(2, solid_square);
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        auto result_masks = result->getAtTime(2);
        REQUIRE(result_masks.size() == 2);
        
        // Verify we have the expected sizes
        std::vector<size_t> mask_sizes;
        for (auto const& mask : result_masks) {
            mask_sizes.push_back(mask.size());
        }
        std::sort(mask_sizes.begin(), mask_sizes.end());
        
        // Should have one mask with 4 points (unchanged 2x2) and one with 16 points (filled 4x4)
        REQUIRE(mask_sizes[0] == 4);  // Small solid square unchanged
        REQUIRE(mask_sizes[1] == 16); // Hollow rectangle filled to solid 4x4
    }
    
    SECTION("handles empty mask data") {
        auto mask_data = std::make_shared<MaskData>();
        
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(mask_data.get(), &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("handles null input") {
        MaskHoleFillingParameters params;
        auto result = fill_mask_holes(nullptr, &params);
        
        REQUIRE(result);
        REQUIRE(result->getTimesWithData().empty());
    }
}

TEST_CASE("MaskHoleFillingOperation interface tests", "[mask_hole_filling][operation]") {
    
    SECTION("operation metadata") {
        MaskHoleFillingOperation op;
        
        REQUIRE(op.getName() == "Fill Mask Holes");
        REQUIRE(op.getTargetInputTypeIndex() == std::type_index(typeid(std::shared_ptr<MaskData>)));
        
        auto default_params = op.getDefaultParameters();
        REQUIRE(default_params != nullptr);
        auto hole_filling_params = dynamic_cast<MaskHoleFillingParameters*>(default_params.get());
        REQUIRE(hole_filling_params != nullptr);
    }
    
    SECTION("canApply method") {
        MaskHoleFillingOperation op;
        
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
        MaskHoleFillingOperation op;
        
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({6, 6});
        
        // Create a donut shape (outer ring)
        std::vector<Point2D<uint32_t>> donut;
        for (uint32_t row = 1; row < 5; ++row) {
            for (uint32_t col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    donut.emplace_back(col, row);
                }
            }
        }
        mask_data->addAtTime(0, donut);
        
        DataTypeVariant input_variant = mask_data;
        MaskHoleFillingParameters params;
        
        auto result_variant = op.execute(input_variant, &params, [](int){});
        
        REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
        
        auto result_mask_data = std::get<std::shared_ptr<MaskData>>(result_variant);
        REQUIRE(result_mask_data);
        
        auto result_masks = result_mask_data->getAtTime(0);
        REQUIRE(result_masks.size() == 1);
        
        // Should have filled the hole (16 points total for 4x4 square)
        REQUIRE(result_masks[0].size() == 16);
    }
} 