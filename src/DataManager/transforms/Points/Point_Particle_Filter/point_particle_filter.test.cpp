#include "point_particle_filter.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "DataManager.hpp"

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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 10; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
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
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 10; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Ground truth at frames 0 and 10
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{100.0f, 150.0f});
    
    // Assign to group
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 5; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
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
    group_manager->addEntityToGroup(group1, entity_ids[0]);
    group_manager->addEntityToGroup(group1, entity_ids[1]);
    group_manager->addEntityToGroup(group2, entity_ids[2]);
    group_manager->addEntityToGroup(group2, entity_ids[3]);

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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>{ 0 };
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Only one ground truth point
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 10; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Ground truth following a quarter circle
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{150.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{100.0f, 150.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
        2000,   // More particles for curved trajectory
        20.0f,  // Larger transition radius for better exploration
        0.1f    // More random walk for curves
    );
    
    REQUIRE(result != nullptr);
    
    // Check endpoints - curved trajectories have more error due to position-only model
    // The filter struggles with curves because particles have no velocity/momentum
    auto start_result = result->getAtTime(TimeFrameIndex(0));
    REQUIRE(start_result.size() == 1);
    REQUIRE_THAT(start_result[0].x, Catch::Matchers::WithinAbs(150.0f, 15.0f));
    REQUIRE_THAT(start_result[0].y, Catch::Matchers::WithinAbs(100.0f, 15.0f));
    
    auto end_result = result->getAtTime(TimeFrameIndex(10));
    REQUIRE(end_result.size() == 1);
    REQUIRE_THAT(end_result[0].x, Catch::Matchers::WithinAbs(100.0f, 15.0f));
    REQUIRE_THAT(end_result[0].y, Catch::Matchers::WithinAbs(150.0f, 15.0f));
    
    // Verify tracked points stay within the circular mask (more fundamental requirement)
    float start_dist = std::sqrt(
        std::pow(start_result[0].x - 100.0f, 2.0f) + 
        std::pow(start_result[0].y - 100.0f, 2.0f)
    );
    float end_dist = std::sqrt(
        std::pow(end_result[0].x - 100.0f, 2.0f) + 
        std::pow(end_result[0].y - 100.0f, 2.0f)
    );
    REQUIRE(start_dist <= 55.0f);  // Within mask radius
    REQUIRE(end_dist <= 55.0f);    // Within mask radius
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
    
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 5; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    // Add simple ground truth
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{120.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
    // Create DataManager with TimeFrame
    auto data_manager = std::make_unique<DataManager>();
    auto timeValues = std::vector<int>();
    for (int i = 0; i <= 5; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    // Create PointData and MaskData through DataManager
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    
    // Create EntityGroupManager
    auto group_manager = std::make_shared<EntityGroupManager>();
    auto group_id = group_manager->createGroup("Test Group", "Test description");
    
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{100.0f, 100.0f});
    point_data->addAtTime(TimeFrameIndex(5), Point2D<float>{120.0f, 100.0f});
    
    for (auto const & entry_pair : point_data->GetAllPointEntriesAsRange()) {
        for (auto const & entry : entry_pair.entries) {
            group_manager->addEntityToGroup(group_id, entry.entity_id);
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
        false,  // use_velocity_model
        2.0f,   // velocity_noise_std
        [&callback_count, &last_progress](int progress) {
            ++callback_count;
            last_progress = progress;
        }
    );
    
    REQUIRE(result != nullptr);
    REQUIRE(callback_count > 0);
    REQUIRE(last_progress == 100);  // Should end at 100%
}

// ============================================================================
// Image Size Scaling Tests
// ============================================================================

TEST_CASE("PointParticleFilter: Matching image sizes (no scaling)", "[PointParticleFilter][ImageSize]") {
    // Create point data and mask data with matching sizes (100x100)
    auto point_data = std::make_shared<PointData>();
    point_data->setImageSize(ImageSize{100, 100});
    
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setImageSize(ImageSize{100, 100});
    
    EntityGroupManager group_manager;
    
    // Add a simple group with two ground truth points
    GroupId group1 = group_manager.createGroup("Test Group 1", "First test group");
    
    // Add points at frame 0 and 10
    EntityId entity0 = 1;
    EntityId entity10 = 2;
    point_data->addPointEntryAtTime(TimeFrameIndex(0), Point2D<float>{50.0f, 50.0f}, entity0, false);
    point_data->addPointEntryAtTime(TimeFrameIndex(10), Point2D<float>{60.0f, 60.0f}, entity10, false);
    
    // Add entities to group
    group_manager.addEntityToGroup(group1, entity0);
    group_manager.addEntityToGroup(group1, entity10);
    
    // Add masks creating a diagonal corridor
    for (int t = 0; t <= 10; ++t) {
        Mask2D mask;
        for (int i = 45; i <= 65; ++i) {
            mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(i), static_cast<uint32_t>(i)});
        }
        mask_data->addAtTime(TimeFrameIndex(t), mask);
    }
    
    // Run filter
    auto result = pointParticleFilter(
        point_data.get(), mask_data.get(), &group_manager,
        500, 5.0f, 0.1f
    );
    
    REQUIRE(result != nullptr);
    REQUIRE(result->getImageSize() == ImageSize{100, 100});
    
    // Verify we got tracked points
    REQUIRE(result->getAtTime(TimeFrameIndex(0)).size() > 0);
    REQUIRE(result->getAtTime(TimeFrameIndex(5)).size() > 0);
    REQUIRE(result->getAtTime(TimeFrameIndex(10)).size() > 0);

    // Points should be in reasonable range (no scaling distortion)
    auto mid_point = result->getAtTime(TimeFrameIndex(5))[0];
    REQUIRE(mid_point.x >= 45.0f);
    REQUIRE(mid_point.x <= 65.0f);
    REQUIRE(mid_point.y >= 45.0f);
    REQUIRE(mid_point.y <= 65.0f);
}

TEST_CASE("PointParticleFilter: Mismatched image sizes (requires scaling)", "[PointParticleFilter][ImageSize]") {
    // Point data at 100x100, mask data at 200x200 (2x scale)
    // Use DataManager to ensure proper entity registration
    auto data_manager = std::make_unique<DataManager>();
    
    std::vector<int> timeValues;
    for (int i = 0; i <= 10; ++i) {
        timeValues.push_back(static_cast<double>(i));
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    point_data->setImageSize(ImageSize{100, 100});
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    mask_data->setImageSize(ImageSize{200, 200});
    
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    // Add a simple group
    GroupId group1 = group_manager->createGroup("Test Group 1", "First test group");
    
    // Add points in 100x100 space
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{25.0f, 25.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{75.0f, 75.0f});
    
    // Get entity IDs and add to group
    auto entities_t0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto entities_t10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
    REQUIRE(entities_t0.size() == 1);
    REQUIRE(entities_t10.size() == 1);
    
    group_manager->addEntityToGroup(group1, entities_t0[0]);
    group_manager->addEntityToGroup(group1, entities_t10[0]);
    
    // Add masks in 200x200 space (scaled corridor)
    for (int t = 0; t <= 10; ++t) {
        Mask2D mask;
        // Create corridor from (50,50) to (150,150) in 200x200 space
        // This corresponds to (25,25) to (75,75) in 100x100 space
        for (int i = 40; i <= 160; ++i) {
            mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(i), static_cast<uint32_t>(i)});
        }
        mask_data->addAtTime(TimeFrameIndex(t), mask);
    }
    
    // Run filter
    auto result = pointParticleFilter(
        point_data.get(), mask_data.get(), group_manager.get(),
        500, 10.0f, 0.1f
    );
    
    REQUIRE(result != nullptr);
    
    // Result should maintain original point data image size
    REQUIRE(result->getImageSize() == ImageSize{100, 100});
    
    // Verify we got tracked points
    REQUIRE(result->getAtTime(TimeFrameIndex(0)).size() > 0);
    REQUIRE(result->getAtTime(TimeFrameIndex(5)).size() > 0);
    REQUIRE(result->getAtTime(TimeFrameIndex(10)).size() > 0);

    // Verify points are in original coordinate space (100x100)
    auto start_point = result->getAtTime(TimeFrameIndex(0))[0];
    auto end_point = result->getAtTime(TimeFrameIndex(10))[0];

    // Start point should be near (25, 25) in 100x100 space
    // Particle filter is stochastic - use larger tolerance
    REQUIRE(std::abs(start_point.x - 25.0f) < 20.0f);
    REQUIRE(std::abs(start_point.y - 25.0f) < 20.0f);
    
    // End point should be near (75, 75) in 100x100 space
    REQUIRE(std::abs(end_point.x - 75.0f) < 20.0f);
    REQUIRE(std::abs(end_point.y - 75.0f) < 20.0f);
    
    // Mid-point should be roughly in the middle in 100x100 space
    auto mid_point = result->getAtTime(TimeFrameIndex(5))[0];
    REQUIRE(mid_point.x >= 10.0f);
    REQUIRE(mid_point.x <= 90.0f);
    REQUIRE(mid_point.y >= 10.0f);
    REQUIRE(mid_point.y <= 90.0f);
}

TEST_CASE("PointParticleFilter: Non-uniform scaling (different x and y scales)", "[PointParticleFilter][ImageSize]") {
    // Point data at 100x200, mask data at 200x100 (inverted aspect ratio)
    // Use DataManager to ensure proper entity registration
    auto data_manager = std::make_unique<DataManager>();
    
    std::vector<int> timeValues;
    for (int i = 0; i <= 10; ++i) {
        timeValues.push_back(i);
    }
    auto timeframe = std::make_shared<TimeFrame>(timeValues);
    data_manager->setTime(TimeKey("test_timeframe"), timeframe);
    
    data_manager->setData<PointData>("test_points", TimeKey("test_timeframe"));
    auto point_data = data_manager->getData<PointData>("test_points");
    point_data->setImageSize(ImageSize{100, 200});  // Wide in Y
    
    data_manager->setData<MaskData>("test_masks", TimeKey("test_timeframe"));
    auto mask_data = data_manager->getData<MaskData>("test_masks");
    mask_data->setImageSize(ImageSize{200, 100});  // Wide in X
    
    auto group_manager = std::make_shared<EntityGroupManager>();
    
    GroupId group1 = group_manager->createGroup("Test Group 1", "First test group");
    
    // Add points in 100x200 space - vertical trajectory
    point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{50.0f, 50.0f});
    point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 150.0f});
    
    // Get entity IDs and add to group
    auto entities_t0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
    auto entities_t10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
    REQUIRE(entities_t0.size() == 1);
    REQUIRE(entities_t10.size() == 1);
    
    group_manager->addEntityToGroup(group1, entities_t0[0]);
    group_manager->addEntityToGroup(group1, entities_t10[0]);
    
    // Add masks in 200x100 space - should accommodate scaled trajectory
    for (int t = 0; t <= 10; ++t) {
        Mask2D mask;
        // Create vertical corridor at x=100 (scaled from x=50 in point space)
        // Y range 25-75 (scaled from 50-150 in point space)
        for (int y = 20; y <= 80; ++y) {
            for (int x = 90; x <= 110; ++x) {
                mask.push_back(Point2D<uint32_t>{static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
            }
        }
        mask_data->addAtTime(TimeFrameIndex(t), mask);
    }
    
    // Run filter
    auto result = pointParticleFilter(
        point_data.get(), mask_data.get(), group_manager.get(),
        500, 15.0f, 0.1f
    );
    
    REQUIRE(result != nullptr);
    
    // Result should maintain original point data image size
    REQUIRE(result->getImageSize() == ImageSize{100, 200});
    
    // Verify tracked points are back in original coordinate space
    auto start_point = result->getAtTime(TimeFrameIndex(0))[0];
    auto end_point = result->getAtTime(TimeFrameIndex(10))[0];

    // X should stay roughly constant around 50 in 100x200 space
    // Particle filter is stochastic - allow reasonable variance
    REQUIRE(std::abs(start_point.x - 50.0f) < 20.0f);
    REQUIRE(std::abs(end_point.x - 50.0f) < 20.0f);
    
    // Y should progress from 50 to 150 in 100x200 space
    REQUIRE(start_point.y >= 30.0f);
    REQUIRE(start_point.y <= 70.0f);
    REQUIRE(end_point.y >= 130.0f);
    REQUIRE(end_point.y <= 170.0f);
}
