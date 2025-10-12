#include "point_particle_filter.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>

// Helper function to generate a circular mask
Mask2D generateCircleMask(uint32_t center_x, uint32_t center_y, uint32_t radius) {
    Mask2D mask;
    for (uint32_t y = center_y - radius; y <= center_y + radius; ++y) {
        for (uint32_t x = center_x - radius; x <= center_x + radius; ++x) {
            float dx = static_cast<float>(x) - static_cast<float>(center_x);
            float dy = static_cast<float>(y) - static_cast<float>(center_y);
            if (dx * dx + dy * dy <= static_cast<float>(radius * radius)) {
                mask.push_back({x, y});
            }
        }
    }
    return mask;
}

// Helper function to generate a rectangular mask
Mask2D generateRectangleMask(uint32_t x_min, uint32_t x_max, uint32_t y_min, uint32_t y_max) {
    Mask2D mask;
    for (uint32_t y = y_min; y <= y_max; ++y) {
        for (uint32_t x = x_min; x <= x_max; ++x) {
            mask.push_back({x, y});
        }
    }
    return mask;
}

TEST_CASE("PointParticleFilter: Basic straight line tracking", "[PointParticleFilter]") {
    // Create test data
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    // Create a group for tracking
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Add ground truth points at frames 0 and 10 (moving horizontally)
    Point2D<float> start_point{50.0f, 50.0f};
    Point2D<float> end_point{150.0f, 50.0f};
    
    point_data->addAtTime(TimeFrameIndex(0), start_point);
    point_data->addAtTime(TimeFrameIndex(10), end_point);
    
    // Get entity IDs and assign to group
    auto all_entries = point_data->GetAllPointEntriesAsRange();
    for (auto const & entry_pair : all_entries) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    // Create masks for each frame (horizontal corridor)
    for (int i = 0; i <= 10; ++i) {
        auto mask = generateRectangleMask(40, 160, 45, 55);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    // Run particle filter
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        500,    // num_particles
        15.0f,  // transition_radius
        0.05f   // random_walk_prob
    );
    
    REQUIRE(result != nullptr);
    
    // Check that we have tracked points for all frames
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.size() >= 2);
    
    // Check start and end points are close to ground truth
    auto start_result = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(start_result.size() == 1);
    REQUIRE_THAT(start_result[0].x, Catch::Matchers::WithinAbs(start_point.x, 5.0));
    REQUIRE_THAT(start_result[0].y, Catch::Matchers::WithinAbs(start_point.y, 5.0));
    
    auto end_result = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(end_result.size() == 1);
    REQUIRE_THAT(end_result[0].x, Catch::Matchers::WithinAbs(end_point.x, 5.0));
    REQUIRE_THAT(end_result[0].y, Catch::Matchers::WithinAbs(end_point.y, 5.0));
}

TEST_CASE("PointParticleFilter: Tracking with gaps in masks", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Ground truth at frames 0 and 10
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{100.0f, 150.0f});
    
    // Assign to group
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    // Add masks with some gaps (frames 0, 2, 4, 6, 8, 10)
    for (int i = 0; i <= 10; i += 2) {
        auto mask = generateCircleMask(100, 100 + i * 5, 30);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        1000,
        20.0f,
        0.1f
    );
    
    REQUIRE(result != nullptr);
    
    // Should still produce some tracked points
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.size() >= 2);
}

TEST_CASE("PointParticleFilter: Multiple groups tracked independently", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    // Create two groups
    auto group1 = group_manager->createGroup("Group 1", "First group");
    auto group2 = group_manager->createGroup("Group 2", "Second group");
    
    // Add ground truth for group 1 (left trajectory)
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{50.0f, 50.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{50.0f, 100.0f});
    
    // Add ground truth for group 2 (right trajectory)
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{150.0f, 50.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{150.0f, 100.0f});
    
    // Assign entities to groups
    auto all_entries = point_data->GetAllPointEntriesAsRange();
    std::vector<EntityId> entity_ids;
    for (auto const & entry_pair : all_entries) {
        for (auto const & entry : entry_pair.entries) {
            entity_ids.push_back(entry.entity_id);
        }
    }
    
    // Assign first two entities to group 1, next two to group 2
    REQUIRE(entity_ids.size() == 4);
    group_manager->addEntityToGroup(entity_ids[0], group1);
    group_manager->addEntityToGroup(entity_ids[1], group1);
    group_manager->addEntityToGroup(entity_ids[2], group2);
    group_manager->addEntityToGroup(entity_ids[3], group2);

    // Create masks covering both areas
    for (int i = 0; i <= 5; ++i) {
        auto mask = generateRectangleMask(30, 170, 40, 110);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        500,
        20.0f,
        0.05f
    );
    
    REQUIRE(result != nullptr);
    
    // Check that we have results for multiple frames
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.size() >= 2);
}

TEST_CASE("PointParticleFilter: Single ground truth point (no tracking)", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Only one ground truth point
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    // Add mask
    auto mask = generateCircleMask(100, 100, 30);
    mask_data->addAtTime(TimeFrameIndex(0), std::move(mask));
    
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        100,
        10.0f,
        0.1f
    );
    
    REQUIRE(result != nullptr);
    
    // Should just return the single point
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.size() == 1);
    
    auto points = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(points.size() == 1);
    REQUIRE_THAT(points[0].x, Catch::Matchers::WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(points[0].y, Catch::Matchers::WithinAbs(100.0f, 1.0f));
}

TEST_CASE("PointParticleFilter: Curved trajectory", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Ground truth following a quarter circle
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{150.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{100.0f, 150.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    // Create circular masks centered at (100, 100) with radius 50
    for (int i = 0; i <= 10; ++i) {
        auto mask = generateCircleMask(100, 100, 55);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        1000,
        15.0f,
        0.05f
    );
    
    REQUIRE(result != nullptr);
    
    // Check endpoints
    auto start_result = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(start_result.size() == 1);
    REQUIRE_THAT(start_result[0].x, Catch::Matchers::WithinAbs(150.0f, 5.0f));
    REQUIRE_THAT(start_result[0].y, Catch::Matchers::WithinAbs(100.0f, 5.0f));
    
    auto end_result = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(end_result.size() == 1);
    REQUIRE_THAT(end_result[0].x, Catch::Matchers::WithinAbs(100.0f, 5.0f));
    REQUIRE_THAT(end_result[0].y, Catch::Matchers::WithinAbs(150.0f, 5.0f));
}

TEST_CASE("PointParticleFilterOperation: Transform operation interface", "[PointParticleFilter]") {
    PointParticleFilterOperation operation;
    
    SECTION("Operation name") {
        REQUIRE(operation.getName() == "Track Points Through Masks (Particle Filter)");
    }
    
    SECTION("Target type index") {
        auto type_idx = operation.getTargetInputTypeIndex();
        REQUIRE(type_idx == std::type_index(typeid(std::shared_ptr<PointData>)));
    }
    
    SECTION("Can apply to PointData") {
        auto point_data = std::make_shared<PointData>();
        DataTypeVariant variant = point_data;
        REQUIRE(operation.canApply(variant));
    }
    
    SECTION("Cannot apply to wrong type") {
        auto mask_data = std::make_shared<MaskData>();
        DataTypeVariant variant = mask_data;
        REQUIRE_FALSE(operation.canApply(variant));
    }
    
    SECTION("Cannot apply to null pointer") {
        std::shared_ptr<PointData> null_ptr = nullptr;
        DataTypeVariant variant = null_ptr;
        REQUIRE_FALSE(operation.canApply(variant));
    }
}

TEST_CASE("PointParticleFilterOperation: Execute with valid data", "[PointParticleFilter]") {
    PointParticleFilterOperation operation;
    
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Add simple ground truth
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{120.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    // Add masks
    for (int i = 0; i <= 5; ++i) {
        auto mask = generateRectangleMask(90, 130, 90, 110);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    // Create parameters
    PointParticleFilterParameters params;
    params.mask_data = mask_data;
    params.group_manager = group_manager.get();
    params.num_particles = 500;
    params.transition_radius = 15.0f;
    params.random_walk_prob = 0.05f;
    
    // Execute operation
    DataTypeVariant input_variant = point_data;
    auto result_variant = operation.execute(input_variant, &params);
    
    REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));
    
    auto result = std::get<std::shared_ptr<PointData>>(result_variant);
    REQUIRE(result != nullptr);
    
    // Verify result has data
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.size() >= 2);
}

TEST_CASE("PointParticleFilterOperation: Execute with missing parameters", "[PointParticleFilter]") {
    PointParticleFilterOperation operation;
    
    auto point_data = std::make_shared<PointData>();
    DataTypeVariant input_variant = point_data;
    
    SECTION("Null parameters") {
        auto result_variant = operation.execute(input_variant, nullptr);
        REQUIRE_FALSE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));
    }
    
    SECTION("Missing mask data") {
        auto group_manager = std::make_shared<EntityGroupManager>();
        PointParticleFilterParameters params;
        params.mask_data = nullptr;
        params.group_manager = group_manager.get();
        
        auto result_variant = operation.execute(input_variant, &params);
        REQUIRE_FALSE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));
    }
    
    SECTION("Missing group manager") {
        auto mask_data = std::make_shared<MaskData>();
        PointParticleFilterParameters params;
        params.mask_data = mask_data;
        params.group_manager = nullptr;
        
        auto result_variant = operation.execute(input_variant, &params);
        REQUIRE_FALSE(std::holds_alternative<std::shared_ptr<PointData>>(result_variant));
    }
}

TEST_CASE("PointParticleFilter: Empty input handling", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    // No ground truth points, no groups
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        100,
        10.0f,
        0.1f
    );
    
    REQUIRE(result != nullptr);
    
    // Should return empty PointData
    auto result_times = result->getTimesWithData();
    std::vector<TimeFrameIndex> times_vec(result_times.begin(), result_times.end());
    REQUIRE(times_vec.empty());
}

TEST_CASE("PointParticleFilter: Progress callback is called", "[PointParticleFilter]") {
    auto point_data = std::make_shared<PointData>();
    auto mask_data = std::make_shared<MaskData>();
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{120.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(entry.entity_id, group_id);
        }
    }
    
    for (int i = 0; i <= 5; ++i) {
        auto mask = generateCircleMask(100 + i * 4, 100, 20);
        mask_data->addAtTime(TimeFrameIndex(i), std::move(mask));
    }
    
    // Track progress callback calls
    int callback_count = 0;
    int last_progress = -1;
    
    auto result = pointParticleFilter(
        point_data.get(),
        mask_data.get(),
        group_manager.get(),
        100,
        10.0f,
        0.1f,
        [&callback_count, &last_progress](int progress) {
            ++callback_count;
            last_progress = progress;
        }
    );
    
    REQUIRE(result != nullptr);
    REQUIRE(callback_count > 0);
    REQUIRE(last_progress == 100);  // Should end at 100%
}
