#include "Lineage/EntityResolver.hpp"
#include "Lineage/LineageRecorder.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "DataManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "transforms/v2/algorithms/MaskArea/MaskArea.hpp"
#include "transforms/v2/algorithms/SumReduction/SumReduction.hpp"
#include "transforms/v2/core/ElementRegistry.hpp"
#include "transforms/v2/core/TransformPipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

using namespace WhiskerToolbox::Entity::Lineage;
using namespace WhiskerToolbox::Transforms::V2;
using namespace WhiskerToolbox::Transforms::V2::Examples;
using Catch::Matchers::UnorderedEquals;

namespace {

/**
 * @brief Create test MaskData with known EntityIds
 * 
 * Creates masks at different times to test lineage resolution:
 * - T0: 1 mask (4 pixels) 
 * - T10: 2 masks (2 and 3 pixels)
 * - T20: 1 mask (5 pixels)
 */
std::shared_ptr<MaskData> createTestMaskData(std::shared_ptr<TimeFrame> time_frame) {
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);

    // T0: One mask with 4 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{1, 0},
                Point2D<uint32_t>{1, 1}});
        mask_data->addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);
    }

    // T10: Two masks
    {
        Mask2D mask1({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{1, 0}});
        mask_data->addAtTime(TimeFrameIndex(1), mask1, NotifyObservers::No);

        Mask2D mask2({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{0, 2}});
        mask_data->addAtTime(TimeFrameIndex(1), mask2, NotifyObservers::No);
    }

    // T20: One mask with 5 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{1, 0},
                Point2D<uint32_t>{1, 1},
                Point2D<uint32_t>{2, 0}});
        mask_data->addAtTime(TimeFrameIndex(2), mask, NotifyObservers::No);
    }

    return mask_data;
}

}// anonymous namespace

TEST_CASE("Transform Lineage Integration - MaskArea Full Workflow", "[lineage][transforms][integration]") {
    // 1. Create DataManager and TimeFrame
    DataManager dm;
    std::vector<int> times = {0, 10, 20, 30, 40};
    auto time_frame = std::make_shared<TimeFrame>(times);

    // 2. Create MaskData with known structure
    auto mask_data = createTestMaskData(time_frame);

    // Store in DataManager
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Verify masks have EntityIds assigned
    auto mask_ids_t0 = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto mask_ids_t1 = mask_data->getEntityIdsAtTime(TimeFrameIndex(1));
    auto mask_ids_t2 = mask_data->getEntityIdsAtTime(TimeFrameIndex(2));

    REQUIRE(mask_ids_t0.size() == 1);
    REQUIRE(mask_ids_t1.size() == 2);
    REQUIRE(mask_ids_t2.size() == 1);

    INFO("Mask EntityIds at T0: " << mask_ids_t0[0].id);
    INFO("Mask EntityIds at T1: " << mask_ids_t1[0].id << ", " << mask_ids_t1[1].id);
    INFO("Mask EntityIds at T2: " << mask_ids_t2[0].id);

    // 3. Execute MaskArea transform via pipeline
    MaskAreaParams params;
    auto pipeline = TransformPipeline()
                            .addStep("CalculateMaskArea", params);

    auto result_variant = pipeline.execute<MaskData>(*mask_data);
    auto areas = std::get<std::shared_ptr<RaggedAnalogTimeSeries>>(result_variant);

    // Verify transform worked
    REQUIRE(areas->getNumTimePoints() == 3);

    auto areas_t0 = areas->getDataAtTime(TimeFrameIndex(0));
    auto areas_t1 = areas->getDataAtTime(TimeFrameIndex(1));
    auto areas_t2 = areas->getDataAtTime(TimeFrameIndex(2));

    REQUIRE(areas_t0.size() == 1);
    CHECK(areas_t0[0] == 4.0f);// 4 pixels

    REQUIRE(areas_t1.size() == 2);
    CHECK(areas_t1[0] == 2.0f);// 2 pixels
    CHECK(areas_t1[1] == 3.0f);// 3 pixels

    REQUIRE(areas_t2.size() == 1);
    CHECK(areas_t2[0] == 5.0f);// 5 pixels

    // 4. Store result in DataManager
    dm.setData<RaggedAnalogTimeSeries>("mask_areas", areas, TimeKey("time"));

    // 5. Record lineage using LineageRecorder
    // Get lineage type from transform metadata
    auto & registry = ElementRegistry::instance();
    auto const * meta = registry.getMetadata("CalculateMaskArea");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->lineage_type == TransformLineageType::OneToOneByTime);

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "mask_areas",// output key
            "masks",     // input key
            meta->lineage_type);

    // Verify lineage was recorded
    REQUIRE(dm.getLineageRegistry()->hasLineage("mask_areas"));
    auto lineage = dm.getLineageRegistry()->getLineage("mask_areas");
    REQUIRE(lineage.has_value());
    REQUIRE(std::holds_alternative<OneToOneByTime>(lineage.value()));
    CHECK(std::get<OneToOneByTime>(lineage.value()).source_key == "masks");

    // 6. Use EntityResolver to trace back to source masks
    EntityResolver resolver(&dm);

    SECTION("Resolve single element at T0") {
        // The area value at T0, index 0 should resolve to the mask at T0, index 0
        auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(0), 0);

        REQUIRE(source_ids.size() == 1);
        CHECK(source_ids[0] == mask_ids_t0[0]);
    }

    SECTION("Resolve first element at T1") {
        // First area at T1 should resolve to first mask at T1
        auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(1), 0);

        REQUIRE(source_ids.size() == 1);
        CHECK(source_ids[0] == mask_ids_t1[0]);
    }

    SECTION("Resolve second element at T1") {
        // Second area at T1 should resolve to second mask at T1
        auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(1), 1);

        REQUIRE(source_ids.size() == 1);
        CHECK(source_ids[0] == mask_ids_t1[1]);
    }

    SECTION("Resolve element at T2") {
        auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(2), 0);

        REQUIRE(source_ids.size() == 1);
        CHECK(source_ids[0] == mask_ids_t2[0]);
    }

    SECTION("Resolve to root (single step chain)") {
        // Since masks are source data, resolveToRoot should give same result
        auto root_ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(1), 1);

        REQUIRE(root_ids.size() == 1);
        CHECK(root_ids[0] == mask_ids_t1[1]);
    }
}

TEST_CASE("Transform Lineage Integration - Chain: MaskArea → SumReduction", "[lineage][transforms][integration]") {
    // Test a two-step pipeline with different lineage types

    DataManager dm;
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = createTestMaskData(time_frame);
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Capture mask EntityIds before transform
    auto mask_ids_t0 = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto mask_ids_t1 = mask_data->getEntityIdsAtTime(TimeFrameIndex(1));// 2 masks

    // Execute MaskArea first
    MaskAreaParams area_params;
    auto area_pipeline = TransformPipeline().addStep("CalculateMaskArea", area_params);
    auto areas_variant = area_pipeline.execute<MaskData>(*mask_data);
    auto areas = std::get<std::shared_ptr<RaggedAnalogTimeSeries>>(areas_variant);
    dm.setData<RaggedAnalogTimeSeries>("mask_areas", areas, TimeKey("time"));

    // Record lineage for areas
    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "mask_areas",
            "masks",
            TransformLineageType::OneToOneByTime);

    // Execute SumReduction on areas
    SumReductionParams sum_params;
    auto sum_pipeline = TransformPipeline().addStep("SumReduction", sum_params);
    auto sums_variant = sum_pipeline.execute<RaggedAnalogTimeSeries>(*areas);
    auto sums = std::get<std::shared_ptr<AnalogTimeSeries>>(sums_variant);
    dm.setData<AnalogTimeSeries>("total_areas", sums, TimeKey("time"));

    // Record lineage for total_areas (AllToOneByTime since SumReduction aggregates)
    auto & registry = ElementRegistry::instance();
    auto const * sum_meta = registry.getMetadata("SumReduction");
    REQUIRE(sum_meta != nullptr);
    REQUIRE(sum_meta->lineage_type == TransformLineageType::AllToOneByTime);

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "total_areas",
            "mask_areas",
            sum_meta->lineage_type);

    // Verify chain: total_areas → mask_areas → masks
    auto chain = dm.getLineageRegistry()->getLineageChain("total_areas");
    REQUIRE(chain.size() == 3);
    CHECK(chain[0] == "total_areas");
    CHECK(chain[1] == "mask_areas");
    CHECK(chain[2] == "masks");

    EntityResolver resolver(&dm);

    SECTION("Resolve total_areas at T1 - should get ALL mask EntityIds at T1") {
        // SumReduction aggregates all values at a time → AllToOneByTime lineage
        // So resolving total_areas[T1] should return ALL source entities at T1
        auto source_ids = resolver.resolveToSource("total_areas", TimeFrameIndex(1));

        // Should get all intermediate area values at T1 (which themselves map to all masks at T1)
        // But since mask_areas doesn't have EntityIds (it's AnalogTimeSeries), 
        // we need to resolve through the chain
        // This tests the OneToOneByTime case for mask_areas → masks
        // For AllToOneByTime (total_areas → mask_areas), we get all entries at that time
        INFO("Source IDs count: " << source_ids.size());
    }

    SECTION("Resolve to root from total_areas") {
        // Should traverse: total_areas → mask_areas → masks
        auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(1));

        // At T1, we have 2 masks, SumReduction aggregates both
        // So root resolution should return both mask EntityIds
        REQUIRE(root_ids.size() == 2);
        CHECK_THAT(root_ids, UnorderedEquals(std::vector<EntityId>{mask_ids_t1[0], mask_ids_t1[1]}));
    }
}

TEST_CASE("Transform Lineage Integration - Source Container Lineage", "[lineage][transforms][integration]") {
    DataManager dm;
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = createTestMaskData(time_frame);
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Record masks as source data
    LineageRecorder::recordSource(*dm.getLineageRegistry(), "masks");

    REQUIRE(dm.getLineageRegistry()->isSource("masks"));

    EntityResolver resolver(&dm);

    // Resolving source data should return its own EntityIds
    auto ids = resolver.resolveToSource("masks", TimeFrameIndex(0), 0);
    REQUIRE(ids.size() == 1);
    CHECK(ids[0] == mask_data->getEntityIdsAtTime(TimeFrameIndex(0))[0]);
}

// =============================================================================
// Full Pipeline Tests: MaskData → MaskArea → SumReduction → AnalogTimeSeries
// =============================================================================

namespace {

/**
 * @brief Create MaskData with exactly one mask per time point
 * 
 * - T0: 1 mask (3 pixels)
 * - T1: 1 mask (5 pixels)
 * - T2: 1 mask (2 pixels)
 * - T3: 1 mask (4 pixels)
 */
std::shared_ptr<MaskData> createSingleMaskPerTimeData(std::shared_ptr<TimeFrame> time_frame) {
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);

    // T0: One mask with 3 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{0, 2}});
        mask_data->addAtTime(TimeFrameIndex(0), mask, NotifyObservers::No);
    }

    // T1: One mask with 5 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{1, 0},
                Point2D<uint32_t>{1, 1},
                Point2D<uint32_t>{2, 0}});
        mask_data->addAtTime(TimeFrameIndex(1), mask, NotifyObservers::No);
    }

    // T2: One mask with 2 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{1, 0}});
        mask_data->addAtTime(TimeFrameIndex(2), mask, NotifyObservers::No);
    }

    // T3: One mask with 4 pixels
    {
        Mask2D mask({
                Point2D<uint32_t>{0, 0},
                Point2D<uint32_t>{0, 1},
                Point2D<uint32_t>{1, 0},
                Point2D<uint32_t>{1, 1}});
        mask_data->addAtTime(TimeFrameIndex(3), mask, NotifyObservers::No);
    }

    return mask_data;
}

/**
 * @brief Create MaskData with multiple masks per time point
 * 
 * - T0: 3 masks (areas: 2, 4, 3 = sum 9)
 * - T1: 2 masks (areas: 5, 1 = sum 6)
 * - T2: 4 masks (areas: 1, 2, 3, 4 = sum 10)
 * - T3: 1 mask (area: 6 = sum 6)
 */
std::shared_ptr<MaskData> createMultipleMasksPerTimeData(std::shared_ptr<TimeFrame> time_frame) {
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(time_frame);

    // T0: Three masks with areas 2, 4, 3
    {
        Mask2D mask1({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1}});// 2 pixels
        Mask2D mask2({Point2D<uint32_t>{1, 0}, Point2D<uint32_t>{1, 1},
                      Point2D<uint32_t>{2, 0}, Point2D<uint32_t>{2, 1}});// 4 pixels
        Mask2D mask3({Point2D<uint32_t>{3, 0}, Point2D<uint32_t>{3, 1},
                      Point2D<uint32_t>{3, 2}});// 3 pixels
        mask_data->addAtTime(TimeFrameIndex(0), mask1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), mask2, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(0), mask3, NotifyObservers::No);
    }

    // T1: Two masks with areas 5, 1
    {
        Mask2D mask1({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1},
                      Point2D<uint32_t>{0, 2}, Point2D<uint32_t>{1, 0},
                      Point2D<uint32_t>{1, 1}});// 5 pixels
        Mask2D mask2({Point2D<uint32_t>{2, 0}});// 1 pixel
        mask_data->addAtTime(TimeFrameIndex(1), mask1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(1), mask2, NotifyObservers::No);
    }

    // T2: Four masks with areas 1, 2, 3, 4
    {
        Mask2D mask1({Point2D<uint32_t>{0, 0}});// 1 pixel
        Mask2D mask2({Point2D<uint32_t>{1, 0}, Point2D<uint32_t>{1, 1}});// 2 pixels
        Mask2D mask3({Point2D<uint32_t>{2, 0}, Point2D<uint32_t>{2, 1},
                      Point2D<uint32_t>{2, 2}});// 3 pixels
        Mask2D mask4({Point2D<uint32_t>{3, 0}, Point2D<uint32_t>{3, 1},
                      Point2D<uint32_t>{3, 2}, Point2D<uint32_t>{3, 3}});// 4 pixels
        mask_data->addAtTime(TimeFrameIndex(2), mask1, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), mask2, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), mask3, NotifyObservers::No);
        mask_data->addAtTime(TimeFrameIndex(2), mask4, NotifyObservers::No);
    }

    // T3: One mask with area 6
    {
        Mask2D mask({Point2D<uint32_t>{0, 0}, Point2D<uint32_t>{0, 1},
                     Point2D<uint32_t>{0, 2}, Point2D<uint32_t>{1, 0},
                     Point2D<uint32_t>{1, 1}, Point2D<uint32_t>{1, 2}});// 6 pixels
        mask_data->addAtTime(TimeFrameIndex(3), mask, NotifyObservers::No);
    }

    return mask_data;
}

}// anonymous namespace

TEST_CASE("Full Pipeline - Single Mask Per Time", "[lineage][transforms][integration][pipeline]") {
    // Test the full chain: MaskData → MaskArea → SumReduction → AnalogTimeSeries
    // With exactly ONE mask at each time point

    DataManager dm;
    std::vector<int> times = {0, 10, 20, 30};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = createSingleMaskPerTimeData(time_frame);
    
    // Must add to DataManager FIRST to get EntityIds assigned
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Now capture EntityIds (after DataManager assignment)
    std::vector<EntityId> expected_ids;
    for (int t = 0; t < 4; ++t) {
        auto ids = mask_data->getEntityIdsAtTime(TimeFrameIndex(t));
        REQUIRE(ids.size() == 1);
        expected_ids.push_back(ids[0]);
        INFO("T" << t << " EntityId: " << ids[0].id);
    }

    // Execute pipeline (masks already in DataManager)
    // Step 1: MaskArea transform
    MaskAreaParams area_params;
    auto area_pipeline = TransformPipeline().addStep("CalculateMaskArea", area_params);
    auto areas_variant = area_pipeline.execute<MaskData>(*mask_data);
    auto areas = std::get<std::shared_ptr<RaggedAnalogTimeSeries>>(areas_variant);
    dm.setData<RaggedAnalogTimeSeries>("mask_areas", areas, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "mask_areas",
            "masks",
            TransformLineageType::OneToOneByTime);

    // Step 2: SumReduction transform
    SumReductionParams sum_params;
    auto sum_pipeline = TransformPipeline().addStep("SumReduction", sum_params);
    auto sums_variant = sum_pipeline.execute<RaggedAnalogTimeSeries>(*areas);
    auto totals = std::get<std::shared_ptr<AnalogTimeSeries>>(sums_variant);
    dm.setData<AnalogTimeSeries>("total_areas", totals, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "total_areas",
            "mask_areas",
            TransformLineageType::AllToOneByTime);

    // Verify transform results
    REQUIRE(totals->getNumSamples() == 4);
    
    // Single mask per time: area values should match pixel counts
    // T0: 3, T1: 5, T2: 2, T3: 4
    auto total_data = totals->getAnalogTimeSeries();
    CHECK(total_data[0] == 3.0f);
    CHECK(total_data[1] == 5.0f);
    CHECK(total_data[2] == 2.0f);
    CHECK(total_data[3] == 4.0f);

    // Verify lineage chain
    auto chain = dm.getLineageRegistry()->getLineageChain("total_areas");
    REQUIRE(chain.size() == 3);
    CHECK(chain[0] == "total_areas");
    CHECK(chain[1] == "mask_areas");
    CHECK(chain[2] == "masks");

    EntityResolver resolver(&dm);

    SECTION("resolveToSource returns immediate parent info") {
        // resolveToSource goes one step: total_areas → mask_areas
        // Since mask_areas has no EntityIds, this should still work through lineage
        auto source_ids = resolver.resolveToSource("total_areas", TimeFrameIndex(0));
        // AllToOneByTime returns all entities at time from source
        // mask_areas has 1 value at T0, which maps to 1 mask
        INFO("resolveToSource at T0 returned " << source_ids.size() << " ids");
    }

    SECTION("resolveToRoot at each time - single mask") {
        for (int t = 0; t < 4; ++t) {
            auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(t));

            INFO("Time " << t << ": expected EntityId " << expected_ids[t].id);
            REQUIRE(root_ids.size() == 1);
            CHECK(root_ids[0] == expected_ids[t]);
        }
    }

    SECTION("resolveToRoot from intermediate container") {
        // Resolve from mask_areas (intermediate) should also work
        for (int t = 0; t < 4; ++t) {
            auto root_ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(t), 0);

            REQUIRE(root_ids.size() == 1);
            CHECK(root_ids[0] == expected_ids[t]);
        }
    }
}

TEST_CASE("Full Pipeline - Multiple Masks Per Time", "[lineage][transforms][integration][pipeline]") {
    // Test the full chain: MaskData → MaskArea → SumReduction → AnalogTimeSeries
    // With MULTIPLE masks at each time point (varying counts)

    DataManager dm;
    std::vector<int> times = {0, 10, 20, 30};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = createMultipleMasksPerTimeData(time_frame);
    
    // Must add to DataManager FIRST to get EntityIds assigned
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Now capture EntityIds (after DataManager assignment)
    // T0: 3 masks, T1: 2 masks, T2: 4 masks, T3: 1 mask
    auto ids_t0 = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto ids_t1 = mask_data->getEntityIdsAtTime(TimeFrameIndex(1));
    auto ids_t2 = mask_data->getEntityIdsAtTime(TimeFrameIndex(2));
    auto ids_t3 = mask_data->getEntityIdsAtTime(TimeFrameIndex(3));

    REQUIRE(ids_t0.size() == 3);
    REQUIRE(ids_t1.size() == 2);
    REQUIRE(ids_t2.size() == 4);
    REQUIRE(ids_t3.size() == 1);

    // Execute pipeline manually (masks already in DataManager)
    // Step 1: MaskArea transform
    MaskAreaParams area_params;
    auto area_pipeline = TransformPipeline().addStep("CalculateMaskArea", area_params);
    auto areas_variant = area_pipeline.execute<MaskData>(*mask_data);
    auto areas = std::get<std::shared_ptr<RaggedAnalogTimeSeries>>(areas_variant);
    dm.setData<RaggedAnalogTimeSeries>("mask_areas", areas, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "mask_areas",
            "masks",
            TransformLineageType::OneToOneByTime);

    // Step 2: SumReduction transform
    SumReductionParams sum_params;
    auto sum_pipeline = TransformPipeline().addStep("SumReduction", sum_params);
    auto sums_variant = sum_pipeline.execute<RaggedAnalogTimeSeries>(*areas);
    auto totals = std::get<std::shared_ptr<AnalogTimeSeries>>(sums_variant);
    dm.setData<AnalogTimeSeries>("total_areas", totals, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "total_areas",
            "mask_areas",
            TransformLineageType::AllToOneByTime);

    // Verify transform results (sum of areas at each time)
    REQUIRE(totals->getNumSamples() == 4);
    
    auto total_data = totals->getAnalogTimeSeries();
    // T0: 2+4+3 = 9, T1: 5+1 = 6, T2: 1+2+3+4 = 10, T3: 6
    CHECK(total_data[0] == 9.0f);
    CHECK(total_data[1] == 6.0f);
    CHECK(total_data[2] == 10.0f);
    CHECK(total_data[3] == 6.0f);

    // Verify intermediate data
    CHECK(areas->getCountAtTime(TimeFrameIndex(0)) == 3);
    CHECK(areas->getCountAtTime(TimeFrameIndex(1)) == 2);
    CHECK(areas->getCountAtTime(TimeFrameIndex(2)) == 4);
    CHECK(areas->getCountAtTime(TimeFrameIndex(3)) == 1);

    EntityResolver resolver(&dm);

    SECTION("resolveToRoot at T0 - 3 masks") {
        auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(0));

        REQUIRE(root_ids.size() == 3);
        CHECK_THAT(root_ids, UnorderedEquals(std::vector<EntityId>{ids_t0[0], ids_t0[1], ids_t0[2]}));
    }

    SECTION("resolveToRoot at T1 - 2 masks") {
        auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(1));

        REQUIRE(root_ids.size() == 2);
        CHECK_THAT(root_ids, UnorderedEquals(std::vector<EntityId>{ids_t1[0], ids_t1[1]}));
    }

    SECTION("resolveToRoot at T2 - 4 masks") {
        auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(2));

        REQUIRE(root_ids.size() == 4);
        CHECK_THAT(root_ids, UnorderedEquals(std::vector<EntityId>{
                ids_t2[0], ids_t2[1], ids_t2[2], ids_t2[3]}));
    }

    SECTION("resolveToRoot at T3 - 1 mask") {
        auto root_ids = resolver.resolveToRoot("total_areas", TimeFrameIndex(3));

        REQUIRE(root_ids.size() == 1);
        CHECK(root_ids[0] == ids_t3[0]);
    }

    SECTION("resolveToRoot from intermediate - specific index") {
        // Resolve mask_areas[T2, index=2] should give the 3rd mask at T2
        auto root_ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(2), 2);

        REQUIRE(root_ids.size() == 1);
        CHECK(root_ids[0] == ids_t2[2]);
    }

    SECTION("Verify all individual mask_areas resolve correctly") {
        // At T0, there are 3 areas → 3 masks
        for (size_t i = 0; i < 3; ++i) {
            auto root_ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(0), i);
            REQUIRE(root_ids.size() == 1);
            CHECK(root_ids[0] == ids_t0[i]);
        }

        // At T2, there are 4 areas → 4 masks
        for (size_t i = 0; i < 4; ++i) {
            auto root_ids = resolver.resolveToRoot("mask_areas", TimeFrameIndex(2), i);
            REQUIRE(root_ids.size() == 1);
            CHECK(root_ids[0] == ids_t2[i]);
        }
    }
}

TEST_CASE("Full Pipeline - Mixed Single and Multiple Masks", "[lineage][transforms][integration][pipeline]") {
    // Use the original test data which has:
    // T0: 1 mask, T1: 2 masks, T2: 1 mask

    DataManager dm;
    std::vector<int> times = {0, 10, 20};
    auto time_frame = std::make_shared<TimeFrame>(times);

    auto mask_data = createTestMaskData(time_frame);
    
    // Must add to DataManager FIRST to get EntityIds assigned
    dm.setData<MaskData>("masks", mask_data, TimeKey("time"));

    // Now capture EntityIds (after DataManager assignment)
    auto ids_t0 = mask_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto ids_t1 = mask_data->getEntityIdsAtTime(TimeFrameIndex(1));
    auto ids_t2 = mask_data->getEntityIdsAtTime(TimeFrameIndex(2));

    REQUIRE(ids_t0.size() == 1);
    REQUIRE(ids_t1.size() == 2);
    REQUIRE(ids_t2.size() == 1);

    // Execute pipeline manually (masks already in DataManager)
    // Step 1: MaskArea transform
    MaskAreaParams area_params;
    auto area_pipeline = TransformPipeline().addStep("CalculateMaskArea", area_params);
    auto areas_variant = area_pipeline.execute<MaskData>(*mask_data);
    auto areas = std::get<std::shared_ptr<RaggedAnalogTimeSeries>>(areas_variant);
    dm.setData<RaggedAnalogTimeSeries>("mask_areas", areas, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "mask_areas",
            "masks",
            TransformLineageType::OneToOneByTime);

    // Step 2: SumReduction transform
    SumReductionParams sum_params;
    auto sum_pipeline = TransformPipeline().addStep("SumReduction", sum_params);
    auto sums_variant = sum_pipeline.execute<RaggedAnalogTimeSeries>(*areas);
    auto totals = std::get<std::shared_ptr<AnalogTimeSeries>>(sums_variant);
    dm.setData<AnalogTimeSeries>("total_areas", totals, TimeKey("time"));

    LineageRecorder::record(
            *dm.getLineageRegistry(),
            "total_areas",
            "mask_areas",
            TransformLineageType::AllToOneByTime);

    // Verify sums
    auto total_data = totals->getAnalogTimeSeries();
    // T0: 4 pixels, T1: 2+3=5, T2: 5 pixels
    CHECK(total_data[0] == 4.0f);
    CHECK(total_data[1] == 5.0f);
    CHECK(total_data[2] == 5.0f);

    EntityResolver resolver(&dm);

    SECTION("resolveToRoot handles varying mask counts") {
        // T0: 1 mask
        auto root_t0 = resolver.resolveToRoot("total_areas", TimeFrameIndex(0));
        REQUIRE(root_t0.size() == 1);
        CHECK(root_t0[0] == ids_t0[0]);

        // T1: 2 masks
        auto root_t1 = resolver.resolveToRoot("total_areas", TimeFrameIndex(1));
        REQUIRE(root_t1.size() == 2);
        CHECK_THAT(root_t1, UnorderedEquals(std::vector<EntityId>{ids_t1[0], ids_t1[1]}));

        // T2: 1 mask
        auto root_t2 = resolver.resolveToRoot("total_areas", TimeFrameIndex(2));
        REQUIRE(root_t2.size() == 1);
        CHECK(root_t2[0] == ids_t2[0]);
    }
}
