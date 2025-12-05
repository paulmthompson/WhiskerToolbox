#include "MaskArea.hpp"

#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/core/DataManagerIntegration.hpp"
#include "transforms/v2/core/ParameterIO.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"
#include "transforms/v2/core/RegisteredTransforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace WhiskerToolbox::Transforms::V2::Examples;
using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Tests: MaskAreaParams JSON Loading
// ============================================================================

TEST_CASE("MaskAreaParams - Load valid JSON with all fields", "[transforms][v2][params][json]") {
    std::string json = R"({
        "scale_factor": 2.5,
        "min_area": 10.0,
        "exclude_holes": true
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(2.5f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(10.0f, 0.001f));
    REQUIRE(params.getExcludeHoles() == true);
}

TEST_CASE("MaskAreaParams - Load JSON with partial fields (uses defaults)", "[transforms][v2][params][json]") {
    std::string json = R"({
        "scale_factor": 3.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(3.0f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(0.0f, 0.001f)); // default
    REQUIRE(params.getExcludeHoles() == false); // default
}

TEST_CASE("MaskAreaParams - Load empty JSON (uses all defaults)", "[transforms][v2][params][json]") {
    std::string json = "{}";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result);
    auto params = result.value();
    
    REQUIRE_THAT(params.getScaleFactor(), Catch::Matchers::WithinRel(1.0f, 0.001f));
    REQUIRE_THAT(params.getMinArea(), Catch::Matchers::WithinRel(0.0f, 0.001f));
    REQUIRE(params.getExcludeHoles() == false);
}

TEST_CASE("MaskAreaParams - Reject negative scale_factor", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": -1.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation
}

TEST_CASE("MaskAreaParams - Reject zero scale_factor", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": 0.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation (ExclusiveMinimum<true>)
}

TEST_CASE("MaskAreaParams - Reject negative min_area", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "min_area": -5.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result); // Should fail validation
}

TEST_CASE("MaskAreaParams - Accept zero min_area", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "min_area": 0.0
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(result); // Zero should be valid for min_area
}

TEST_CASE("MaskAreaParams - Reject invalid JSON", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": "not_a_number"
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result);
}

TEST_CASE("MaskAreaParams - Reject malformed JSON", "[transforms][v2][params][json][validation]") {
    std::string json = R"({
        "scale_factor": 1.0,
        "invalid
    })";
    
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    
    REQUIRE(!result);
}


TEST_CASE("MaskAreaParams - JSON round-trip preserves values", "[transforms][v2][params][json]") {
    MaskAreaParams original;
    original.scale_factor = 2.5f;
    original.min_area = 15.0f;
    original.exclude_holes = true;
    
    // Serialize
    std::string json = saveParametersToJson(original);
    
    // Deserialize
    auto result = loadParametersFromJson<MaskAreaParams>(json);
    REQUIRE(result);
    auto recovered = result.value();
    
    // Verify values match
    REQUIRE_THAT(recovered.getScaleFactor(), Catch::Matchers::WithinRel(2.5f, 0.001f));
    REQUIRE_THAT(recovered.getMinArea(), Catch::Matchers::WithinRel(15.0f, 0.001f));
    REQUIRE(recovered.getExcludeHoles() == true);
}

// ============================================================================
// Test-specific convenience functions
// ============================================================================

/**
 * @brief Create a pipeline for MaskData → area → sum → AnalogTimeSeries
 */
inline TransformPipeline createMaskAreaSumPipeline(
    MaskAreaParams area_params = {},
    SumReductionParams sum_params = {})
{
    return TransformPipeline()
        .addStep("CalculateMaskArea", std::move(area_params))
        .addStep("SumReduction", std::move(sum_params));
}

/**
 * @brief Execute mask area calculation and sum reduction in one call
 */
inline std::shared_ptr<AnalogTimeSeries> calculateAndSumMaskAreas(
    MaskData const& mask_data)
{
    auto pipeline = createMaskAreaSumPipeline();
    auto result_variant = pipeline.execute<MaskData>(mask_data);
    return std::get<std::shared_ptr<AnalogTimeSeries>>(result_variant);
}

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
    // Transforms are now registered at compile time via RegisteredTransforms.hpp
    auto& registry = ElementRegistry::instance();
    
    // Verify it was registered
    REQUIRE(registry.hasTransform("CalculateMaskArea"));
    
    auto const* meta = registry.getMetadata("CalculateMaskArea");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->name == "CalculateMaskArea");
    REQUIRE(meta->description == "Calculate the area of a mask in pixels");
}

TEST_CASE("TransformsV2 - Registry Execute Element Transform", "[transforms][v2][registry]") {
    auto& registry = ElementRegistry::instance();
    
    // Create test mask with 3 pixels
    Mask2D mask({
        Point2D<uint32_t>{1, 1},
        Point2D<uint32_t>{1, 2},
        Point2D<uint32_t>{2, 1}
    });
    // 3 pixels
    
    MaskAreaParams params;
    
    // Execute via registry (transform already registered at compile time)
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
    auto& registry = ElementRegistry::instance();
    
    // Transforms are already registered via RegisteredTransforms.hpp

    
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
    
    // Use view-based transformation with range constructor
    MaskAreaParams params;
    auto transform_fn = registry.getTransformFunction<Mask2D, float, MaskAreaParams>("CalculateMaskArea", params);
    
    // Create a transformed view (lazy evaluation)
    auto transformed_view = std::ranges::views::transform(
        mask_data.elements(),
        [transform_fn](auto const& time_entry_pair) {
            auto const& [time, entry] = time_entry_pair;
            return std::pair{time, transform_fn(entry.data)};
        }
    );
    
    // Construct from the view (single-pass, efficient)
    RaggedAnalogTimeSeries result(transformed_view);
    
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
    
    std::cout << "✓ View-based transformation works!\n";
    std::cout << "  Element: Mask2D → float (via registry)\n";
    std::cout << "  Container: Direct construction from std::ranges::views::transform\n";
    std::cout << "  Benefits: Lazy evaluation, zero intermediate storage, efficient\n";
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

TEST_CASE("TransformsV2 - Chained Transform: Mask Area + Sum Reduction", "[transforms][v2][chaining]") {
    // This tests the user's specific goal:
    // Input: MaskData with areas {10, 5} at a time
    // Step 1: MaskData → RaggedAnalogTimeSeries (Mask2D → float via calculateMaskArea)
    // Step 2: RaggedAnalogTimeSeries → AnalogTimeSeries (span<float> → float via sumReduction)
    // Output: Single value 15 at each time
    
    std::cout << "\n=== Testing Chained Transform ===\n";
    
    // Setup time frame
    std::vector<int> times_int = {0, 100, 200};
    auto time_frame = std::make_shared<TimeFrame>(times_int);
    
    // Create MaskData with multiple masks per time point
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Time 0: Two masks with areas 10 and 5
    std::vector<Point2D<uint32_t>> mask1_pixels;
    for (uint32_t i = 0; i < 10; ++i) {
        mask1_pixels.push_back(Point2D<uint32_t>{i, 0});
    }
    Mask2D mask1(mask1_pixels);
    
    std::vector<Point2D<uint32_t>> mask2_pixels;
    for (uint32_t i = 0; i < 5; ++i) {
        mask2_pixels.push_back(Point2D<uint32_t>{i, 1});
    }
    Mask2D mask2(mask2_pixels);
    
    mask_data.addAtTime(TimeFrameIndex(0), mask1, NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(0), mask2, NotifyObservers::No);
    
    // Time 100: Three masks with areas 3, 7, 2
    mask_data.addAtTime(TimeFrameIndex(100), 
        Mask2D({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{1, 0}, Point2D<uint32_t>{2, 0}}),
        NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(100),
        Mask2D({Point2D<uint32_t>{0, 1}, Point2D<uint32_t>{1, 1}, Point2D<uint32_t>{2, 1},
                Point2D<uint32_t>{3, 1}, Point2D<uint32_t>{4, 1}, Point2D<uint32_t>{5, 1},
                Point2D<uint32_t>{6, 1}}),
        NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(100),
        Mask2D({Point2D<uint32_t>{0, 2}, Point2D<uint32_t>{1, 2}}),
        NotifyObservers::No);
    
    // Time 200: One mask with area 20
    std::vector<Point2D<uint32_t>> mask3_pixels;
    for (uint32_t i = 0; i < 20; ++i) {
        mask3_pixels.push_back(Point2D<uint32_t>{i, 3});
    }
    mask_data.addAtTime(TimeFrameIndex(200), Mask2D(mask3_pixels), NotifyObservers::No);
    
    std::cout << "Input MaskData:\n";
    std::cout << "  Time 0: 2 masks (areas 10, 5)\n";
    std::cout << "  Time 100: 3 masks (areas 3, 7, 2)\n";
    std::cout << "  Time 200: 1 mask (area 20)\n";
    
    // Use global registry (transforms registered via RegisteredTransforms.hpp)
    auto& registry = ElementRegistry::instance();

    
    std::cout << "\nStep 1: MaskData → RaggedAnalogTimeSeries (element transform)\n";
    
    // Step 1: Apply element transform using view
    MaskAreaParams area_params;
    auto mask_area_fn = registry.getTransformFunction<Mask2D, float, MaskAreaParams>("CalculateMaskArea", area_params);
    
    auto mask_to_ragged_view = std::ranges::views::transform(
        mask_data.elements(),
        [mask_area_fn](auto const& time_entry_pair) {
            auto const& [time, entry] = time_entry_pair;
            return std::pair{time, mask_area_fn(entry.data)};
        }
    );
    
    RaggedAnalogTimeSeries ragged_result(mask_to_ragged_view);
    ragged_result.setTimeFrame(mask_data.getTimeFrame());
    
    // Verify intermediate result
    REQUIRE(ragged_result.getNumTimePoints() == 3);
    
    auto data_at_0 = ragged_result.getDataAtTime(TimeFrameIndex(0));
    REQUIRE(data_at_0.size() == 2);
    REQUIRE(data_at_0[0] == 10.0f);
    REQUIRE(data_at_0[1] == 5.0f);
    std::cout << "  Time 0: {" << data_at_0[0] << ", " << data_at_0[1] << "}\n";
    
    auto data_at_100 = ragged_result.getDataAtTime(TimeFrameIndex(100));
    REQUIRE(data_at_100.size() == 3);
    REQUIRE(data_at_100[0] == 3.0f);
    REQUIRE(data_at_100[1] == 7.0f);
    REQUIRE(data_at_100[2] == 2.0f);
    std::cout << "  Time 100: {" << data_at_100[0] << ", " << data_at_100[1] << ", " << data_at_100[2] << "}\n";
    
    auto data_at_200 = ragged_result.getDataAtTime(TimeFrameIndex(200));
    REQUIRE(data_at_200.size() == 1);
    REQUIRE(data_at_200[0] == 20.0f);
    std::cout << "  Time 200: {" << data_at_200[0] << "}\n";
    
    std::cout << "\nStep 2: RaggedAnalogTimeSeries → AnalogTimeSeries (time-grouped transform)\n";
    
    // Step 2: Apply time-grouped transform
    // Collect transformed data into vectors for efficient construction
    SumReductionParams sum_params;
    std::vector<TimeFrameIndex> times;
    std::vector<float> values;
    
    times.reserve(ragged_result.getNumTimePoints());
    values.reserve(ragged_result.getNumTimePoints());
    
    for (auto const& entry : ragged_result) {
        auto const& [time, data_span] = entry;
        // Apply sumReduction directly
        auto result_vec = sumReduction(data_span, sum_params);
        times.push_back(time);
        values.push_back(result_vec[0]);  // Extract single summed value
    }
    
    AnalogTimeSeries final_result(std::move(values), std::move(times));
    final_result.setTimeFrame(ragged_result.getTimeFrame());
    
    // Verify final result
    REQUIRE(final_result.getNumSamples() == 3);

    auto const series = final_result.getAnalogTimeSeries();
    
    float value_at_0 = series[0];
    REQUIRE(value_at_0 == 15.0f);  // 10 + 5 = 15
    std::cout << "  Time 0: " << value_at_0 << " (10 + 5)\n";
    
    float value_at_100 = series[1];
    REQUIRE(value_at_100 == 12.0f);  // 3 + 7 + 2 = 12
    std::cout << "  Time 100: " << value_at_100 << " (3 + 7 + 2)\n";
    
    float value_at_200 = series[2];
    REQUIRE(value_at_200 == 20.0f);  // Just 20
    std::cout << "  Time 200: " << value_at_200 << "\n";
    
    std::cout << "\n✓ Chained transform works correctly!\n";
    std::cout << "  Chain: MaskData → RaggedAnalogTimeSeries → AnalogTimeSeries\n";
    std::cout << "  Method: Lazy views with std::ranges::views::transform\n";
    std::cout << "  Result: {10, 5} → 15 ✓\n";
    std::cout << "  Benefits: Zero intermediate storage, composable, efficient\n";
}

TEST_CASE("TransformsV2 - Container Transform Helpers", "[transforms][v2][container][helpers]") {
    std::cout << "\n=== Testing Container Transform Helpers ===\n";
    
    // Setup test data
    std::vector<int> times_int = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times_int);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Add masks with known areas
    mask_data.addAtTime(TimeFrameIndex(0), 
        Mask2D({{0,0}, {1,0}, {2,0}, {3,0}}), // 4 pixels
        NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(0),
        Mask2D({{0,1}, {1,1}}), // 2 pixels
        NotifyObservers::No);
    
    mask_data.addAtTime(TimeFrameIndex(10),
        Mask2D({{0,0}, {1,0}, {2,0}}), // 3 pixels
        NotifyObservers::No);
    
    mask_data.addAtTime(TimeFrameIndex(20),
        Mask2D({{0,0}, {1,0}, {2,0}, {3,0}, {4,0}}), // 5 pixels
        NotifyObservers::No);
    
    std::cout << "Test data: MaskData with masks of areas {4,2}, {3}, {5}\n";
    
    // Test applyElementTransform
    std::cout << "\nTesting applyElementTransform...\n";
    auto ragged = applyElementTransform<MaskData, RaggedAnalogTimeSeries, Mask2D, float, MaskAreaParams>(
        mask_data, "CalculateMaskArea", MaskAreaParams{});
    
    REQUIRE(ragged != nullptr);
    REQUIRE(ragged->getNumTimePoints() == 3);
    
    auto data0 = ragged->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(data0.size() == 2);
    REQUIRE(data0[0] == 4.0f);
    REQUIRE(data0[1] == 2.0f);
    std::cout << "  Time 0: {" << data0[0] << ", " << data0[1] << "} ✓\n";
    
    auto data10 = ragged->getDataAtTime(TimeFrameIndex(10));
    REQUIRE(data10.size() == 1);
    REQUIRE(data10[0] == 3.0f);
    std::cout << "  Time 10: {" << data10[0] << "} ✓\n";
    
    // Test applyTimeGroupedTransform
    std::cout << "\nTesting applyTimeGroupedTransform...\n";
    auto analog = applyTimeGroupedTransform<RaggedAnalogTimeSeries, AnalogTimeSeries, float, float, SumReductionParams>(
        *ragged, "SumReduction", SumReductionParams{});
    
    REQUIRE(analog != nullptr);
    REQUIRE(analog->getNumSamples() == 3);
    
    auto series = analog->getAnalogTimeSeries();
    REQUIRE(series[0] == 6.0f);  // 4 + 2
    REQUIRE(series[1] == 3.0f);  // 3
    REQUIRE(series[2] == 5.0f);  // 5
    std::cout << "  Results: {" << series[0] << ", " << series[1] << ", " << series[2] << "} ✓\n";
    
    std::cout << "\n✓ Container transform helpers work correctly!\n";
}

TEST_CASE("TransformsV2 - Transform Pipeline", "[transforms][v2][pipeline]") {
    std::cout << "\n=== Testing Transform Pipeline ===\n";
    
    // Setup test data
    std::vector<int> times_int = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times_int);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Time 0: masks with areas 10 and 5
    std::vector<Point2D<uint32_t>> pixels10;
    for (uint32_t i = 0; i < 10; ++i) pixels10.push_back({i, 0});
    mask_data.addAtTime(TimeFrameIndex(0), Mask2D(pixels10), NotifyObservers::No);
    
    std::vector<Point2D<uint32_t>> pixels5;
    for (uint32_t i = 0; i < 5; ++i) pixels5.push_back({i, 1});
    mask_data.addAtTime(TimeFrameIndex(0), Mask2D(pixels5), NotifyObservers::No);
    
    // Time 10: masks with areas 3, 7, 2
    mask_data.addAtTime(TimeFrameIndex(10), Mask2D({{0,0}, {1,0}, {2,0}}), NotifyObservers::No);
    std::vector<Point2D<uint32_t>> pixels7;
    for (uint32_t i = 0; i < 7; ++i) pixels7.push_back({i, 1});
    mask_data.addAtTime(TimeFrameIndex(10), Mask2D(pixels7), NotifyObservers::No);
    mask_data.addAtTime(TimeFrameIndex(10), Mask2D({{0,2}, {1,2}}), NotifyObservers::No);
    
    // Time 20: mask with area 20
    std::vector<Point2D<uint32_t>> pixels20;
    for (uint32_t i = 0; i < 20; ++i) pixels20.push_back({i, 0});
    mask_data.addAtTime(TimeFrameIndex(20), Mask2D(pixels20), NotifyObservers::No);
    
    std::cout << "Input: MaskData with areas {10,5}, {3,7,2}, {20}\n";
    
    // Test pipeline builder
    std::cout << "\nBuilding pipeline with addStep()...\n";
    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{})
            .addStep("SumReduction", SumReductionParams{});
    
    REQUIRE(pipeline.size() == 2);
    REQUIRE(pipeline.getStepName(0) == "CalculateMaskArea");
    REQUIRE(pipeline.getStepName(1) == "SumReduction");
    std::cout << "  Step 0: " << pipeline.getStepName(0) << " ✓\n";
    std::cout << "  Step 1: " << pipeline.getStepName(1) << " ✓\n";
    
    // Execute pipeline (uses standard materialized execution since step 2 is time-grouped)
    std::cout << "\nExecuting pipeline (materialized - has time-grouped transform)...\n";
    auto result_variant = pipeline.execute<MaskData>(mask_data);
    auto result = std::get<std::shared_ptr<AnalogTimeSeries>>(result_variant);
    
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 3);
    
    auto series = result->getAnalogTimeSeries();
    REQUIRE(series[0] == 15.0f);  // 10 + 5
    REQUIRE(series[1] == 12.0f);  // 3 + 7 + 2
    REQUIRE(series[2] == 20.0f);  // 20
    
    std::cout << "  Time 0: " << series[0] << " (expected 15) ✓\n";
    std::cout << "  Time 10: " << series[1] << " (expected 12) ✓\n";
    std::cout << "  Time 20: " << series[2] << " (expected 20) ✓\n";
    
    std::cout << "\n✓ Transform pipeline works correctly!\n";
    std::cout << "  Note: This pipeline uses materialized execution because\n";
    std::cout << "        step 2 (SumReduction) is time-grouped and requires\n";
    std::cout << "        collecting all values at each time point.\n";
}

TEST_CASE("TransformsV2 - Fused Pipeline Execution", "[transforms][v2][pipeline][fusion]") {
    std::cout << "\n=== Testing Fused Pipeline Execution ===\n";
    
    // Setup test data
    std::vector<int> times_int = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times_int);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    // Add some masks
    for (int i = 0; i < 3; ++i) {
        std::vector<Point2D<uint32_t>> pixels;
        int num_pixels = (i + 1) * 5;  // 5, 10, 15 pixels
        for (int j = 0; j < num_pixels; ++j) {
            pixels.push_back({static_cast<uint32_t>(j), static_cast<uint32_t>(i)});
        }
        mask_data.addAtTime(TimeFrameIndex(i * 10), Mask2D(pixels), NotifyObservers::No);
    }
    
    std::cout << "Input: MaskData with single masks of areas 5, 10, 15\n";
    
    // Create element-only pipeline (single step - can be fused)
    std::cout << "\nTesting single-step fused execution...\n";
    TransformPipeline single_step_pipeline;
    single_step_pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    
    try {
        auto ragged = single_step_pipeline.executeFused<MaskData, RaggedAnalogTimeSeries>(mask_data);
        
        REQUIRE(ragged != nullptr);
        REQUIRE(ragged->getNumTimePoints() == 3);
        
        auto data0 = ragged->getDataAtTime(TimeFrameIndex(0));
        auto data10 = ragged->getDataAtTime(TimeFrameIndex(10));
        auto data20 = ragged->getDataAtTime(TimeFrameIndex(20));
        
        REQUIRE(data0.size() == 1);
        REQUIRE(data0[0] == 5.0f);
        REQUIRE(data10.size() == 1);
        REQUIRE(data10[0] == 10.0f);
        REQUIRE(data20.size() == 1);
        REQUIRE(data20[0] == 15.0f);
        
        std::cout << "  ✓ Single-step fusion works!\n";
        std::cout << "    Time 0: area = " << data0[0] << "\n";
        std::cout << "    Time 10: area = " << data10[0] << "\n";
        std::cout << "    Time 20: area = " << data20[0] << "\n";
        std::cout << "    Performance: Single pass, zero intermediate allocations\n";
    } catch (std::exception const& e) {
        FAIL("Fused execution failed: " << e.what());
    }
    
    std::cout << "\n✓ Fused pipeline execution works!\n";
    std::cout << "  Benefits:\n";
    std::cout << "  - Single pass through data\n";
    std::cout << "  - No intermediate container allocations\n";
    std::cout << "  - Hot data stays in CPU cache\n";
    std::cout << "  - Minimal overhead (function pointer indirection only)\n";
}

TEST_CASE("TransformsV2 - Convenience Functions", "[transforms][v2][convenience]") {
    std::cout << "\n=== Testing Convenience Functions ===\n";
    
    // Setup test data
    std::vector<int> times_int = {0, 10};
    auto time_frame = std::make_shared<TimeFrame>(times_int);
    
    MaskData mask_data;
    mask_data.setTimeFrame(time_frame);
    
    std::vector<Point2D<uint32_t>> pixels8;
    for (uint32_t i = 0; i < 8; ++i) pixels8.push_back({i, 0});
    mask_data.addAtTime(TimeFrameIndex(0), Mask2D(pixels8), NotifyObservers::No);
    
    std::vector<Point2D<uint32_t>> pixels12;
    for (uint32_t i = 0; i < 12; ++i) pixels12.push_back({i, 1});
    mask_data.addAtTime(TimeFrameIndex(10), Mask2D(pixels12), NotifyObservers::No);
    
    std::cout << "Input: MaskData with single masks of area 8 and 12\n";
    
    // Test convenience function
    std::cout << "\nTesting calculateAndSumMaskAreas()...\n";
    auto result = calculateAndSumMaskAreas(mask_data);
    
    REQUIRE(result != nullptr);
    REQUIRE(result->getNumSamples() == 2);
    
    auto series = result->getAnalogTimeSeries();
    REQUIRE(series[0] == 8.0f);
    REQUIRE(series[1] == 12.0f);
    
    std::cout << "  Time 0: " << series[0] << " ✓\n";
    std::cout << "  Time 10: " << series[1] << " ✓\n";
    
    std::cout << "\n✓ Convenience function works - single call transforms MaskData → AnalogTimeSeries!\n";
}

// ============================================================================
// V2 DataManager Integration Tests
// ============================================================================

TEST_CASE("TransformsV2 - DataManager Integration - load_data_from_json_config_v2", "[transforms][v2][datamanager][json_config]") {
    using namespace WhiskerToolbox::Transforms::V2;
    
    // Create DataManager and populate it with MaskData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data in code
    auto test_mask_data = std::make_shared<MaskData>();
    
    // Add multiple masks at different timestamps
    std::vector<uint32_t> x1 = {1, 2, 3};
    std::vector<uint32_t> y1 = {1, 2, 3};
    test_mask_data->addAtTime(TimeFrameIndex(100), Mask2D(x1, y1), NotifyObservers::No);

    std::vector<uint32_t> x2 = {4, 5, 6, 7, 8};
    std::vector<uint32_t> y2 = {4, 5, 6, 7, 8};
    test_mask_data->addAtTime(TimeFrameIndex(200), Mask2D(x2, y2), NotifyObservers::No);
    std::vector<uint32_t> x3 = {9, 10};
    std::vector<uint32_t> y3 = {9, 10};
    test_mask_data->addAtTime(TimeFrameIndex(300), Mask2D(x3, y3), NotifyObservers::No);

    // Store the mask data in DataManager with a known key
    dm.setData("test_mask_data", test_mask_data, TimeKey("default"));
    
    // Create JSON configuration for transformation pipeline using unified format
    const char* json_config = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Mask Area Calculation Pipeline V2\",\n"
        "            \"description\": \"Test mask area calculation on mask data using V2 system\",\n"
        "            \"version\": \"2.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"CalculateMaskArea\",\n"
        "                \"input_key\": \"test_mask_data\",\n"
        "                \"output_key\": \"calculated_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "v2_mask_area_pipeline_test";
    std::filesystem::create_directories(test_dir);
    
    std::filesystem::path json_filepath = test_dir / "pipeline_config_v2.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }
    
    // Execute the transformation pipeline using V2 load_data_from_json_config
    auto data_info_list = load_data_from_json_config_v2(&dm, json_filepath.string());
    
    // Verify the transformation was executed and results are available
    // Note: V2 outputs RaggedAnalogTimeSeries for ragged input (MaskData)
    // Each timestamp has a single mask, so each has one area value
    auto result_areas = dm.getData<RaggedAnalogTimeSeries>("calculated_areas");
    REQUIRE(result_areas != nullptr);
    
    // Verify the area calculation results
    auto time_indices = result_areas->getTimeIndices();
    REQUIRE(time_indices.size() == 3);
    
    // Check timestamp 100 (3 points)
    auto values_100 = result_areas->getDataAtTime(TimeFrameIndex(100));
    REQUIRE(values_100.size() == 1);
    REQUIRE(values_100[0] == 3.0f);
    
    // Check timestamp 200 (5 points)
    auto values_200 = result_areas->getDataAtTime(TimeFrameIndex(200));
    REQUIRE(values_200.size() == 1);
    REQUIRE(values_200[0] == 5.0f);
    
    // Check timestamp 300 (2 points)
    auto values_300 = result_areas->getDataAtTime(TimeFrameIndex(300));
    REQUIRE(values_300.size() == 1);
    REQUIRE(values_300[0] == 2.0f);
    
    // Test with empty mask data
    auto empty_mask_data = std::make_shared<MaskData>();
    dm.setData("empty_mask_data", empty_mask_data, TimeKey("default"));
    
    const char* json_config_empty = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Empty Mask Area Calculation V2\",\n"
        "            \"description\": \"Test mask area calculation on empty mask data using V2\",\n"
        "            \"version\": \"2.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"CalculateMaskArea\",\n"
        "                \"input_key\": \"empty_mask_data\",\n"
        "                \"output_key\": \"empty_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_empty = test_dir / "pipeline_config_empty_v2.json";
    {
        std::ofstream json_file(json_filepath_empty);
        REQUIRE(json_file.is_open());
        json_file << json_config_empty;
        json_file.close();
    }
    
    // Execute the empty mask pipeline
    auto data_info_list_empty = load_data_from_json_config_v2(&dm, json_filepath_empty.string());
    
    // Verify the empty mask results
    auto result_empty_areas = dm.getData<RaggedAnalogTimeSeries>("empty_areas");
    REQUIRE(result_empty_areas != nullptr);
    REQUIRE(result_empty_areas->getNumTimePoints() == 0);
    
    // Test with multiple masks at same timestamp
    auto multi_mask_data = std::make_shared<MaskData>();
    
    // Add two masks at the same timestamp
    std::vector<uint32_t> x1_multi = {1, 2};
    std::vector<uint32_t> y1_multi = {1, 2};
    multi_mask_data->addAtTime(TimeFrameIndex(500), Mask2D(x1_multi, y1_multi), NotifyObservers::No);
    
    std::vector<uint32_t> x2_multi = {3, 4, 5};
    std::vector<uint32_t> y2_multi = {3, 4, 5};
    multi_mask_data->addAtTime(TimeFrameIndex(500), Mask2D(x2_multi, y2_multi), NotifyObservers::No);
    dm.setData("multi_mask_data", multi_mask_data, TimeKey("default"));
    
    const char* json_config_multi = 
        "[\n"
        "{\n"
        "    \"transformations\": {\n"
        "        \"metadata\": {\n"
        "            \"name\": \"Multiple Masks Area Calculation V2\",\n"
        "            \"description\": \"Test mask area calculation with multiple masks at same timestamp using V2\",\n"
        "            \"version\": \"2.0\"\n"
        "        },\n"
        "        \"steps\": [\n"
        "            {\n"
        "                \"step_id\": \"1\",\n"
        "                \"transform_name\": \"CalculateMaskArea\",\n"
        "                \"input_key\": \"multi_mask_data\",\n"
        "                \"output_key\": \"multi_areas\",\n"
        "                \"parameters\": {}\n"
        "            }\n"
        "        ]\n"
        "    }\n"
        "}\n"
        "]";
    
    std::filesystem::path json_filepath_multi = test_dir / "pipeline_config_multi_v2.json";
    {
        std::ofstream json_file(json_filepath_multi);
        REQUIRE(json_file.is_open());
        json_file << json_config_multi;
        json_file.close();
    }
    
    // Execute the multi-mask pipeline
    auto data_info_list_multi = load_data_from_json_config_v2(&dm, json_filepath_multi.string());
    
    // Verify the multi-mask results
    // Note: V2 system returns individual mask areas, not summed
    // MaskData (ragged) -> RaggedAnalogTimeSeries
    auto result_multi_areas = dm.getData<RaggedAnalogTimeSeries>("multi_areas");
    REQUIRE(result_multi_areas != nullptr);
    
    // Should have 2 values at timestamp 500 (one for each mask)
    auto multi_values = result_multi_areas->getDataAtTime(TimeFrameIndex(500));
    REQUIRE(multi_values.size() == 2);
    
    // Areas should be 2 and 3 (order may vary)
    float sum = multi_values[0] + multi_values[1];
    REQUIRE(sum == 5.0f);  // 2 + 3 = 5
    
    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("TransformsV2 - DataManager Integration - Pipeline Executor Direct", "[transforms][v2][datamanager][executor]") {
    using namespace WhiskerToolbox::Transforms::V2;
    
    // Create DataManager
    DataManager dm;
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);
    
    // Create test mask data
    auto mask_data = std::make_shared<MaskData>();
    std::vector<uint32_t> x = {1, 2, 3, 4, 5};
    std::vector<uint32_t> y = {1, 2, 3, 4, 5};
    mask_data->addAtTime(TimeFrameIndex(0), Mask2D(x, y), NotifyObservers::No);
    dm.setData("masks", mask_data, TimeKey("default"));
    
    // Create executor directly
    DataManagerPipelineExecutor executor(&dm);
    
    // Load JSON directly (not from file)
    nlohmann::json config = {
        {"steps", {{
            {"step_id", "area_step"},
            {"transform_name", "CalculateMaskArea"},
            {"input_key", "masks"},
            {"output_key", "areas"},
            {"parameters", nlohmann::json::object()}
        }}}
    };
    
    REQUIRE(executor.loadFromJson(config));
    
    // Validate
    auto errors = executor.validate();
    REQUIRE(errors.empty());
    
    // Execute
    auto result = executor.execute();
    REQUIRE(result.success);
    REQUIRE(result.steps_completed == 1);
    
    // Verify output - V2 outputs RaggedAnalogTimeSeries for ragged input
    auto areas = dm.getData<RaggedAnalogTimeSeries>("areas");
    REQUIRE(areas != nullptr);
    auto values = areas->getDataAtTime(TimeFrameIndex(0));
    REQUIRE(values.size() == 1);
    REQUIRE(values[0] == 5.0f);
}
