#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mask_connected_component.hpp"
#include "Masks/Mask_Data.hpp"

#include <set>

TEST_CASE("MaskConnectedComponent basic functionality", "[mask_connected_component]") {
    
    SECTION("removes small components while preserving large ones") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create a mask with a large component (9 pixels) and small components (1-2 pixels each)
        std::vector<Point2D<uint32_t>> large_component = {
            {1, 1}, {2, 1}, {3, 1},  // Row 1
            {1, 2}, {2, 2}, {3, 2},  // Row 2  
            {1, 3}, {2, 3}, {3, 3}   // Row 3 (3x3 square)
        };
        
        std::vector<Point2D<uint32_t>> small_component1 = {
            {7, 1}  // Single pixel
        };
        
        std::vector<Point2D<uint32_t>> small_component2 = {
            {7, 7}, {8, 7}  // Two adjacent pixels
        };
        
        // Add all components to the same time
        mask_data->addAtTime(TimeFrameIndex(0), large_component, false);
        mask_data->addAtTime(TimeFrameIndex(0), small_component1, false);
        mask_data->addAtTime(TimeFrameIndex(0), small_component2, false);
        
        // Set threshold to 5 - should keep the 9-pixel component, remove the 1 and 2-pixel components
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 5;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        
        // Check that we have data at time 0
        auto times = result->getTimesWithData();
        REQUIRE(times.size() == 1);
        REQUIRE(times[0] == TimeFrameIndex(0));
        
        // Get masks at time 0
        auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
        
        // Should have one mask (the large component preserved)
        REQUIRE(result_masks.size() == 1);
        
        // The preserved mask should have 9 points
        auto const & preserved_mask = result_masks[0];
        REQUIRE(preserved_mask.size() == 9);
        
        // Verify the preserved points are from the large component
        std::set<std::pair<uint32_t, uint32_t>> expected_points;
        for (auto const & point : large_component) {
            expected_points.insert({point.x, point.y});
        }
        
        std::set<std::pair<uint32_t, uint32_t>> actual_points;
        for (auto const & point : preserved_mask) {
            actual_points.insert({point.x, point.y});
        }
        
        REQUIRE(actual_points == expected_points);
    }
    
    SECTION("preserves all components when threshold is 1") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({5, 5});
        
        // Create several small components
        std::vector<Point2D<uint32_t>> component1 = {{1, 1}};
        std::vector<Point2D<uint32_t>> component2 = {{3, 3}};
        std::vector<Point2D<uint32_t>> component3 = {{0, 4}, {1, 4}};  // 2-pixel component
        
        mask_data->addAtTime(TimeFrameIndex(10), component1, false);
        mask_data->addAtTime(TimeFrameIndex(10), component2, false);
        mask_data->addAtTime(TimeFrameIndex(10), component3, false);
        
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 1;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        
        auto const & result_masks = result->getAtTime(TimeFrameIndex(10));
        
        // Should preserve all 3 components
        REQUIRE(result_masks.size() == 3);
        
        // Total pixels should be 1 + 1 + 2 = 4
        size_t total_pixels = 0;
        for (auto const & mask : result_masks) {
            total_pixels += mask.size();
        }
        REQUIRE(total_pixels == 4);
    }
    
    SECTION("removes all components when threshold is too high") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({10, 10});
        
        // Create some medium-sized components
        std::vector<Point2D<uint32_t>> component1 = {{0, 0}, {1, 0}, {0, 1}};  // 3 pixels
        std::vector<Point2D<uint32_t>> component2 = {{5, 5}, {6, 5}};  // 2 pixels
        
        mask_data->addAtTime(TimeFrameIndex(5), component1, false);
        mask_data->addAtTime(TimeFrameIndex(5), component2, false);
        
        // Set threshold higher than any component
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 10;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        
        // Should have no masks at time 5 (all removed)
        auto const & result_masks = result->getAtTime(TimeFrameIndex(5));
        REQUIRE(result_masks.empty());
        
        // Should have no times with data
        auto times = result->getTimesWithData();
        REQUIRE(times.empty());
    }
    
    SECTION("handles empty mask data") {
        auto mask_data = std::make_shared<MaskData>();
        
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 5;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        REQUIRE(result->getTimesWithData().empty());
    }
    
    SECTION("handles multiple time points") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({8, 8});
        
        // Time 0: Large component (should be preserved)
        std::vector<Point2D<uint32_t>> large_comp = {
            {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}  // 6 pixels
        };
        
        // Time 1: Small component (should be removed)
        std::vector<Point2D<uint32_t>> small_comp = {
            {5, 5}, {5, 6}  // 2 pixels
        };
        
        // Time 2: Medium component (should be preserved)
        std::vector<Point2D<uint32_t>> medium_comp = {
            {3, 3}, {4, 3}, {3, 4}, {4, 4}, {3, 5}  // 5 pixels
        };
        
        mask_data->addAtTime(TimeFrameIndex(0), large_comp, false);
        mask_data->addAtTime(TimeFrameIndex(1), small_comp, false);
        mask_data->addAtTime(TimeFrameIndex(2), medium_comp, false);
        
        auto params = std::make_unique<MaskConnectedComponentParameters>();
        params->threshold = 4;
        
        auto result = remove_small_connected_components(mask_data.get(), params.get());
        
        REQUIRE(result != nullptr);
        
        auto times = result->getTimesWithData();
        std::sort(times.begin(), times.end());
        
        // Should preserve times 0 and 2, remove time 1
        REQUIRE(times.size() == 2);
        REQUIRE(times[0] == TimeFrameIndex(0));
        REQUIRE(times[1] == TimeFrameIndex(2));
        
        // Verify preserved components have correct sizes
        auto const & masks_t0 = result->getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_t0.size() == 1);
        REQUIRE(masks_t0[0].size() == 6);
        
        auto const & masks_t2 = result->getAtTime(TimeFrameIndex(2));
        REQUIRE(masks_t2.size() == 1);
        REQUIRE(masks_t2[0].size() == 5);
        
        // Time 1 should be empty
        auto const & masks_t1 = result->getAtTime(TimeFrameIndex(1));
        REQUIRE(masks_t1.empty());
    }
}

TEST_CASE("MaskConnectedComponentOperation interface", "[mask_connected_component][operation]") {
    
    SECTION("operation name and type checking") {
        MaskConnectedComponentOperation op;
        
        REQUIRE(op.getName() == "Remove Small Connected Components");
        REQUIRE(op.getTargetInputTypeIndex() == typeid(std::shared_ptr<MaskData>));
        
        // Test canApply with correct type
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant valid_variant = mask_data;
        REQUIRE(op.canApply(valid_variant));
        
        // Test canApply with null pointer
        std::shared_ptr<MaskData> null_mask;
        DataTypeVariant null_variant = null_mask;
        REQUIRE_FALSE(op.canApply(null_variant));
    }
    
    SECTION("default parameters") {
        MaskConnectedComponentOperation op;
        auto params = op.getDefaultParameters();
        
        REQUIRE(params != nullptr);
        
        auto mask_params = dynamic_cast<MaskConnectedComponentParameters*>(params.get());
        REQUIRE(mask_params != nullptr);
        REQUIRE(mask_params->threshold == 10);
    }
    
    SECTION("execute operation") {
        auto mask_data = std::make_shared<MaskData>();
        mask_data->setImageSize({6, 6});
        
        // Add a component larger than default threshold
        std::vector<Point2D<uint32_t>> large_comp = {
            {0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1},
            {0, 2}, {1, 2}, {2, 2}, {3, 0}, {3, 1}, {3, 2}  // 12 pixels
        };
        
        // Add a small component 
        std::vector<Point2D<uint32_t>> small_comp = {
            {5, 5}  // 1 pixel
        };
        
        mask_data->addAtTime(TimeFrameIndex(0), large_comp, false);
        mask_data->addAtTime(TimeFrameIndex(0), small_comp, false);
        
        MaskConnectedComponentOperation op;
        DataTypeVariant input_variant = mask_data;
        
        auto params = op.getDefaultParameters();  // threshold = 10
        auto result_variant = op.execute(input_variant, params.get());
        
        REQUIRE(std::holds_alternative<std::shared_ptr<MaskData>>(result_variant));
        
        auto result = std::get<std::shared_ptr<MaskData>>(result_variant);
        REQUIRE(result != nullptr);
        
        auto const & result_masks = result->getAtTime(TimeFrameIndex(0));
        REQUIRE(result_masks.size() == 1);  // Only large component preserved
        REQUIRE(result_masks[0].size() == 12);  // Large component has 12 pixels
    }
} 