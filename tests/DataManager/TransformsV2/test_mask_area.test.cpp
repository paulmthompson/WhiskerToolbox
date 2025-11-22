#include "transforms/v2/core/ContainerTraits.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/ElementTransform.hpp"
#include "transforms/v2/examples/MaskAreaTransform.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/masks.hpp"
#include "Masks/Mask_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;

/**
 * @brief Test: Transform Mask2D to std::vector<float> (area calculation)
 * 
 * This demonstrates:
 * 1. Element-level transform: Mask2D → std::vector<float>
 * 2. Container-level automatic lifting: MaskData → RaggedAnalogTimeSeries
 * 3. Preserving shape: same number of vectors as masks
 */

TEST_CASE("TransformsV2 - Mask Area Element Transform", "[transforms][v2][element]") {
    // Create a simple test mask with 4 pixels
    Mask2D mask({
        Point2D<uint32_t>{1, 1},
        Point2D<uint32_t>{1, 2},
        Point2D<uint32_t>{2, 1},
        Point2D<uint32_t>{2, 2}
    });
    // 4 pixels
    
    MaskAreaParams params;
    auto result = calculateMaskArea(mask, params);
    
    REQUIRE(result == 4.0f);
}

TEST_CASE("TransformsV2 - Mask Area Empty Mask", "[transforms][v2][element]") {
    // Empty mask should give area of 0
    Mask2D empty_mask;  // Default constructor creates empty mask
    
    MaskAreaParams params;
    auto result = calculateMaskArea(empty_mask, params);
    
    REQUIRE(result == 0.0f);
}

TEST_CASE("TransformsV2 - Mask Area Full Mask", "[transforms][v2][element]") {
    // Mask with 100 pixels (10x10 grid)
    std::vector<Point2D<uint32_t>> pixels;
    for (uint32_t y = 0; y < 10; ++y) {
        for (uint32_t x = 0; x < 10; ++x) {
            pixels.push_back(Point2D<uint32_t>{x, y});
        }
    }
    Mask2D full_mask(pixels);
    
    MaskAreaParams params;
    auto result = calculateMaskArea(full_mask, params);
    
    REQUIRE(result == 100.0f);
}

TEST_CASE("TransformsV2 - Registry Basic Registration", "[transforms][v2][registry]") {
    ElementRegistry registry;
    
    // Register the transform
    TransformMetadata metadata;
    metadata.description = "Calculate mask area as vector";
    metadata.category = "Image Processing";
    
    registry.registerTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea",
        calculateMaskArea,
        metadata
    );
    
    // Verify it was registered
    REQUIRE(registry.hasTransform("CalculateMaskArea"));
    
    auto const* meta = registry.getMetadata("CalculateMaskArea");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->name == "CalculateMaskArea");
    REQUIRE(meta->description == "Calculate mask area as vector");
}

TEST_CASE("TransformsV2 - Registry Execute Element Transform", "[transforms][v2][registry]") {
    ElementRegistry registry;
    
    TransformMetadata metadata;
    metadata.description = "Calculate mask area as vector";
    
    registry.registerTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea",
        calculateMaskArea,
        metadata
    );
    
    // Create test mask with 3 pixels
    Mask2D mask({
        Point2D<uint32_t>{1, 1},
        Point2D<uint32_t>{1, 2},
        Point2D<uint32_t>{2, 1}
    });
    // 3 pixels
    
    MaskAreaParams params;
    
    // Execute via registry
    auto result = registry.execute<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea", mask, params);
    
    REQUIRE(result == 3.0f);
}

TEST_CASE("TransformsV2 - Container Traits", "[transforms][v2][traits]") {
    // Test element to container mapping
    using Container1 = ContainerFor_t<Mask2D>;
    REQUIRE(std::is_same_v<Container1, MaskData>);
    
    using Container2 = ContainerFor_t<float>;
    REQUIRE(std::is_same_v<Container2, RaggedAnalogTimeSeries>);
    
    // Test reverse mapping
    using Element1 = ElementFor_t<MaskData>;
    REQUIRE(std::is_same_v<Element1, Mask2D>);
    
    using Element2 = ElementFor_t<RaggedAnalogTimeSeries>;
    REQUIRE(std::is_same_v<Element2, float>);
}

TEST_CASE("TransformsV2 - Type Index Mapper", "[transforms][v2][traits]") {
    // Test runtime type mapping
    auto container_type = TypeIndexMapper::elementToContainer(typeid(Mask2D));
    REQUIRE(container_type == typeid(MaskData));
    
    auto element_type = TypeIndexMapper::containerToElement(typeid(RaggedAnalogTimeSeries));
    REQUIRE(element_type == typeid(float));
    
    auto name = TypeIndexMapper::containerToString(typeid(MaskData));
    REQUIRE(name == "MaskData");
    
    auto type_from_str = TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries");
    REQUIRE(type_from_str == typeid(RaggedAnalogTimeSeries));
}

TEST_CASE("TransformsV2 - MaskData to RaggedAnalogTimeSeries Manual", "[transforms][v2][integration]") {
    // Create test data: MaskData with multiple masks at different times
    std::vector<int> times = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    auto time_frame = std::make_shared<TimeFrame>(times);  // 100 frames at 30 fps
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Add mask at time 0 (4 pixels)
    {
        Mask2D mask1({
            Point2D<uint32_t>{0, 0},
            Point2D<uint32_t>{0, 1},
            Point2D<uint32_t>{1, 0},
            Point2D<uint32_t>{1, 1}
        });
        mask_data.addAtTime(TimeFrameIndex(0), mask1, NotifyObservers::No);
    }
    
    // Add two masks at time 10 (one with 2 pixels, one with 3 pixels)
    {
        Mask2D mask2({
            Point2D<uint32_t>{0, 0},
            Point2D<uint32_t>{1, 0}
        });
        mask_data.addAtTime(TimeFrameIndex(10), mask2, NotifyObservers::No);
        
        Mask2D mask3({
            Point2D<uint32_t>{0, 0},
            Point2D<uint32_t>{0, 1},
            Point2D<uint32_t>{0, 2}
        });
        mask_data.addAtTime(TimeFrameIndex(10), mask3, NotifyObservers::No);
    }
    
    // Manually apply transform using range views
    auto result_data = std::make_shared<RaggedAnalogTimeSeries>();
    result_data->setTimeFrame(time_frame);
    
    MaskAreaParams params;
    
    // Process using the container's view interface
    for (auto [time, entry] : mask_data.elements()) {
        float area = calculateMaskArea(entry.data, params);
        result_data->appendAtTime(time, {area}, NotifyObservers::No);
    }
    
    // Verify results
    REQUIRE(result_data->getNumTimePoints() == 2);  // Two distinct times
    
    // Check time 0 - should have one vector with [4.0]
    auto data_at_0 = result_data->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(data_at_0.size() == 1);
    REQUIRE(data_at_0[0] == 4.0f);
    
    // Check time 10 - should have two vectors: [2.0] and [3.0]
    auto data_at_10 = result_data->getDataAtTime(TimeFrameIndex(10));
    REQUIRE(data_at_10.size() == 2);
    REQUIRE(data_at_10[0] == 2.0f);
    REQUIRE(data_at_10[1] == 3.0f);
    
    std::cout << "✓ Successfully transformed MaskData → RaggedAnalogTimeSeries\n";
    std::cout << "  Input: 3 masks across 2 time points\n";
    std::cout << "  Output: 3 area vectors preserving structure\n";
}

TEST_CASE("TransformsV2 - Registry Materialize Container", "[transforms][v2][integration][registry]") {
    // Test automatic container transform materialization
    ElementRegistry registry;
    
    // Register element transform: Mask2D → float
    TransformMetadata metadata;
    metadata.description = "Calculate mask area";
    metadata.category = "Image Processing";
    
    registry.registerTransform<Mask2D, float, MaskAreaParams>(
        "CalculateMaskArea",
        calculateMaskArea,
        metadata
    );
    
    // Create test data: MaskData with multiple masks
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Add mask at time 0 (4 pixels)
    mask_data.addAtTime(TimeFrameIndex(0), 
        Mask2D({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1}, 
                Point2D<uint32_t>{1, 0}, Point2D<uint32_t>{1, 1}}),
        NotifyObservers::No);
    
    // Add two masks at time 10 (2 and 3 pixels)
    mask_data.addAtTime(TimeFrameIndex(10),
        Mask2D({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{1, 0}}),
        NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(10),
        Mask2D({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1}, Point2D<uint32_t>{0, 2}}),
        NotifyObservers::No);
    
    // Materialize container transform (returns by value!)
    MaskAreaParams params;
    auto result = registry.materializeContainer<MaskData, RaggedAnalogTimeSeries>(
        "CalculateMaskArea",
        mask_data,
        params
    );
    
    // DataManager would handle setting TimeFrame
    result.setTimeFrame(mask_data.getTimeFrame());
    
    // Verify results
    REQUIRE(result.getNumTimePoints() == 2);
    
    // Check time 0
    auto data_at_0 = result.getDataAtTime(TimeFrameIndex(0));
    REQUIRE(data_at_0.size() == 1);
    REQUIRE(data_at_0[0] == 4.0f);
    
    // Check time 10
    auto data_at_10 = result.getDataAtTime(TimeFrameIndex(10));
    REQUIRE(data_at_10.size() == 2);
    REQUIRE(data_at_10[0] == 2.0f);
    REQUIRE(data_at_10[1] == 3.0f);
    
    std::cout << "✓ Registry materialize container works!\n";
    std::cout << "  Element: Mask2D → float (registered)\n";
    std::cout << "  Container: MaskData → RaggedAnalogTimeSeries (automatic)\n";
    std::cout << "  Ownership: Returned by value (caller manages)\n";
}

TEST_CASE("TransformsV2 - Range Views Work", "[transforms][v2][ranges]") {
    // Verify that the user's range views work correctly
    
    std::vector<int> times = {0, 10, 20, 30, 40};
    auto time_frame = std::make_shared<TimeFrame>(times);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Add some test data (each mask has t+1 pixels)
    for (int t = 0; t < 5; ++t) {
        std::vector<Point2D<uint32_t>> pixels;
        for (int i = 0; i <= t; ++i) {
            pixels.push_back(Point2D<uint32_t>{static_cast<uint32_t>(i), 0});
        }
        Mask2D mask(pixels);
        mask_data.addAtTime(TimeFrameIndex(t * 10), mask, NotifyObservers::No);
    }
    
    // Test that we can iterate
    int count = 0;
    for (auto [time, entry] : mask_data.elements()) {
        // Mask at time t*10 should have t+1 pixels
        int expected_pixels = count + 1;
        REQUIRE(entry.data.size() == static_cast<size_t>(expected_pixels));
        ++count;
    }
    
    REQUIRE(count == 5);
    
    std::cout << "✓ Range views work correctly\n";
}
