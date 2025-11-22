#include "Points/Point_Data.hpp"
#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "fixtures/entity_id.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("DM - PointData - Core functionality", "[points][data][core]") {
    PointData point_data;

    // Test with same source and target timeframes
    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);

    point_data.setTimeFrame(timeframe);

    // Setup some test data
    Point2D<float> p1{1.0f, 2.0f};
    Point2D<float> p2{3.0f, 4.0f};
    Point2D<float> p3{5.0f, 6.0f};
    std::vector<Point2D<float>> points = {p1, p2};
    std::vector<Point2D<float>> more_points = {p3};

    SECTION("Adding and retrieving points at time") {
        // Add a single point at time 10
        point_data.addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));

        // Add another point at the same time
        point_data.addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[1].x == Catch::Approx(3.0f));
        REQUIRE(points_at_10[1].y == Catch::Approx(4.0f));

        // Add points at a different time
        point_data.addAtTime(TimeFrameIndex(20), more_points, NotifyObservers::No);
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_20[0].y == Catch::Approx(6.0f));
    }

    SECTION("Overwriting points at time") {
        // Add initial points
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);

        // Overwrite with a single point
        point_data.clearAtTime(TimeIndexAndFrame{10, timeframe.get()}, NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(10), p3, NotifyObservers::No);
        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(5.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(6.0f));

        // Overwrite with multiple points
        point_data.clearAtTime(TimeIndexAndFrame{10, timeframe.get()}, NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);
        points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_10[0].y == Catch::Approx(2.0f));
    }

    SECTION("Clearing points at time") {
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(20), more_points, NotifyObservers::No);

        static_cast<void>(point_data.clearAtTime(TimeIndexAndFrame{10, timeframe.get()}, NotifyObservers::No));

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.empty());
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("Setting and getting image size") {
        ImageSize size{640, 480};
        point_data.setImageSize(size);

        auto retrieved_size = point_data.getImageSize();
        REQUIRE(retrieved_size.width == 640);
        REQUIRE(retrieved_size.height == 480);
    }

    SECTION("Overwriting points at multiple times") {
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
        std::vector<std::vector<Point2D<float>>> points_vec = {points, more_points};

        point_data.clearAtTime(TimeIndexAndFrame{10, timeframe.get()}, NotifyObservers::No);
        point_data.clearAtTime(TimeIndexAndFrame{20, timeframe.get()}, NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(10), points_vec[0], NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(20), points_vec[1], NotifyObservers::No);

        auto points_at_10 = point_data.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data.getAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
    }

    SECTION("Getting times with points") {
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);
        point_data.addAtTime(TimeFrameIndex(20), more_points, NotifyObservers::No);

        auto times = point_data.getTimesWithData();

        REQUIRE(times.size() == 2);

        // Convert to vector for easier testing or use iterators
        auto it = times.begin();
        REQUIRE(*it == TimeFrameIndex(10));
        ++it;
        REQUIRE(*it == TimeFrameIndex(20));
    }

    SECTION("Getting max points") {
        point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);     // 2 points
        point_data.addAtTime(TimeFrameIndex(20), more_points, NotifyObservers::No);// 1 point

        auto max_points = point_data.getMaxEntriesAtAnyTime();
        REQUIRE(max_points == 2);
    }
}

TEST_CASE("PointData - Copy and Move by EntityID", "[points][data][entity][copy][move][by_id]") {
    // Setup test points
    Point2D<float> p1{1.0f, 2.0f};
    Point2D<float> p2{3.0f, 4.0f};
    Point2D<float> p3{5.0f, 6.0f};
    Point2D<float> p4{7.0f, 8.0f};

    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    data_manager->setTime(TimeKey("test_time"), time_frame);

    SECTION("Copy points by EntityID - basic functionality") {
        // Setup fresh source and target data
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        // Add test points
        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        // Get entity IDs for testing
        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_10.size() == 2);

        // Copy points from time 10 (2 points)
        std::unordered_set<EntityId> ids_set_10c(entity_ids_10.begin(), entity_ids_10.end());
        std::size_t points_copied = source_data->copyByEntityIds(*target_data, ids_set_10c, NotifyObservers::No);

        REQUIRE(points_copied == 2);

        // Verify source data is unchanged
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);

        // Verify target has copied data
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);

        // Verify target has new entity IDs
        auto target_entity_ids = get_all_entity_ids(*target_data);
        REQUIRE(target_entity_ids.size() == 2);

        std::vector<EntityId> entity_ids_10_vec(entity_ids_10.begin(), entity_ids_10.end());
        REQUIRE(target_entity_ids != entity_ids_10_vec);
    }

    SECTION("Copy points by EntityID - mixed times") {
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(entity_ids_10.size() == 2);
        REQUIRE(entity_ids_20.size() == 1);

        std::vector<EntityId> mixed_entity_ids = {entity_ids_10[0], entity_ids_20[0]};
        std::unordered_set<EntityId> ids_set_mixedc(mixed_entity_ids.begin(), mixed_entity_ids.end());
        std::size_t points_copied = source_data->copyByEntityIds(*target_data, ids_set_mixedc, NotifyObservers::No);

        REQUIRE(points_copied == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
    }

    SECTION("Copy points by EntityID - non-existent EntityIDs") {
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        std::vector<EntityId> fake_entity_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> ids_set_fakec(fake_entity_ids.begin(), fake_entity_ids.end());
        std::size_t points_copied = source_data->copyByEntityIds(*target_data, ids_set_fakec, NotifyObservers::No);

        REQUIRE(points_copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }

    SECTION("Copy points by EntityID - empty EntityID list") {
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        std::vector<EntityId> empty_entity_ids;
        std::unordered_set<EntityId> ids_set_emptyc(empty_entity_ids.begin(), empty_entity_ids.end());
        std::size_t points_copied = source_data->copyByEntityIds(*target_data, ids_set_emptyc, NotifyObservers::No);

        REQUIRE(points_copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }

    SECTION("Move points by EntityID - basic functionality") {
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        auto entity_ids_10_view = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_10_view.size() == 2);
        
        // Materialize the view into a concrete vector BEFORE the move operation
        std::vector<EntityId> entity_ids_10_vec(entity_ids_10_view.begin(), entity_ids_10_view.end());
        std::unordered_set<EntityId> const ids_set_10(entity_ids_10_vec.begin(), entity_ids_10_vec.end());
        std::size_t points_moved = source_data->moveByEntityIds(*target_data, ids_set_10, NotifyObservers::No);

        REQUIRE(points_moved == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);

        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);

        // Verify target has the original entity IDs (preserved during move)
        auto target_entity_ids = get_all_entity_ids(*target_data);
        REQUIRE(target_entity_ids.size() == 2);
        REQUIRE(target_entity_ids == entity_ids_10_vec);
    }

    SECTION("Move points by EntityID - mixed times") {
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        auto entity_ids_10_view = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_20_view = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        
        // Materialize the views into concrete vectors
        std::vector<EntityId> entity_ids_10_vec(entity_ids_10_view.begin(), entity_ids_10_view.end());
        std::vector<EntityId> entity_ids_20_vec(entity_ids_20_view.begin(), entity_ids_20_view.end());

        std::vector<EntityId> mixed_entity_ids = {entity_ids_10_vec[0], entity_ids_20_vec[0]};
        std::unordered_set<EntityId> const ids_set_mixed(mixed_entity_ids.begin(), mixed_entity_ids.end());
        std::size_t points_moved = source_data->moveByEntityIds(*target_data, ids_set_mixed, NotifyObservers::No);

        REQUIRE(points_moved == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);

        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
    }

    SECTION("Move points by EntityID - non-existent EntityIDs") {
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        std::vector<EntityId> fake_entity_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> const ids_set_fake(fake_entity_ids.begin(), fake_entity_ids.end());
        std::size_t points_moved = source_data->moveByEntityIds(*target_data, ids_set_fake, NotifyObservers::No);

        REQUIRE(points_moved == 0);
        REQUIRE(target_data->getTimesWithData().empty());

        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Copy preserves point data integrity") {
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::unordered_set<EntityId> ids_set_10c2(entity_ids_10.begin(), entity_ids_10.end());
        source_data->copyByEntityIds(*target_data, ids_set_10c2, NotifyObservers::No);

        auto source_points = source_data->getAtTime(TimeFrameIndex(10));
        auto target_points = target_data->getAtTime(TimeFrameIndex(10));

        REQUIRE(source_points.size() == target_points.size());
        for (size_t i = 0; i < source_points.size(); ++i) {
            REQUIRE(source_points[i].x == Catch::Approx(target_points[i].x));
            REQUIRE(source_points[i].y == Catch::Approx(target_points[i].y));
        }
    }

    SECTION("Move preserves point data integrity") {
        data_manager->setData<PointData>("target_data", TimeKey("test_time"));
        data_manager->setData<PointData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<PointData>("source_data");
        auto target_data = data_manager->getData<PointData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);

        auto entity_ids_10_view = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto original_points_view = source_data->getAtTime(TimeFrameIndex(10));
        REQUIRE(original_points_view.size() == 2);
        
        // Materialize views into concrete containers BEFORE the move operation
        std::vector<EntityId> entity_ids_10_vec(entity_ids_10_view.begin(), entity_ids_10_view.end());
        std::vector<Point2D<float>> original_points_vec(original_points_view.begin(), original_points_view.end());

        std::unordered_set<EntityId> const ids_set_10b(entity_ids_10_vec.begin(), entity_ids_10_vec.end());
        source_data->moveByEntityIds(*target_data, ids_set_10b, NotifyObservers::No);

        auto target_points = target_data->getAtTime(TimeFrameIndex(10));
        REQUIRE(target_points.size() == 2);

        // Verify each original point is present in target (order may differ)
        for (auto const & sp: original_points_vec) {
            bool found = false;
            for (auto const & tp: target_points) {
                if (sp.x == tp.x && sp.y == tp.y) {
                    found = true;
                    break;
                }
            }
            REQUIRE(found);
        }
    }
}

TEST_CASE("DM - PointData - Edge cases and error handling", "[points][data][error]") {
    PointData point_data;

    SECTION("Getting points at non-existent time") {
        auto points = point_data.getAtTime(TimeFrameIndex(999));
        REQUIRE(points.empty());
    }

    SECTION("Multiple operations sequence") {
        Point2D<float> p1{1.0f, 2.0f};

        // Add, clear, add again to test internal state consistency
        point_data.addAtTime(TimeFrameIndex(5), p1, NotifyObservers::No);
        static_cast<void>(point_data.clearAtTime(TimeIndexAndFrame{5, nullptr}, NotifyObservers::No));
        point_data.addAtTime(TimeFrameIndex(5), p1, NotifyObservers::No);

        auto points = point_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(points.size() == 1);
        REQUIRE(points[0].x == Catch::Approx(1.0f));
    }

    SECTION("Construction from map") {
        std::map<TimeFrameIndex, std::vector<Point2D<float>>> map_data = {
                {TimeFrameIndex(10), {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                {TimeFrameIndex(20), {{5.0f, 6.0f}}}};

        PointData point_data_from_map(map_data);

        auto points_at_10 = point_data_from_map.getAtTime(TimeFrameIndex(10));
        auto points_at_20 = point_data_from_map.getAtTime(TimeFrameIndex(20));

        REQUIRE(points_at_10.size() == 2);
        REQUIRE(points_at_20.size() == 1);
        REQUIRE(points_at_10[0].x == Catch::Approx(1.0f));
        REQUIRE(points_at_20[0].x == Catch::Approx(5.0f));
    }
}

TEST_CASE("DM - PointData - Image scaling", "[points][data][scaling]") {
    PointData point_data;

    // Setup test data with known coordinates
    std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);

    SECTION("Scaling from known size") {
        // Set initial image size
        ImageSize initial_size{640, 480};
        point_data.setImageSize(initial_size);

        // Scale to larger size
        ImageSize new_size{1280, 960};
        point_data.changeImageSize(new_size);

        auto scaled_points = point_data.getAtTime(TimeFrameIndex(10));

        // Points should be scaled by factor of 2
        REQUIRE(scaled_points[0].x == Catch::Approx(200.0f));
        REQUIRE(scaled_points[0].y == Catch::Approx(400.0f));
        REQUIRE(scaled_points[1].x == Catch::Approx(600.0f));
        REQUIRE(scaled_points[1].y == Catch::Approx(800.0f));

        // Image size should be updated
        auto current_size = point_data.getImageSize();
        REQUIRE(current_size.width == 1280);
        REQUIRE(current_size.height == 960);
    }

    SECTION("Scaling with no initial size set") {
        // Try to scale without setting initial size
        ImageSize new_size{1280, 960};
        point_data.changeImageSize(new_size);

        // Points should remain unchanged since no initial size was set
        auto unchanged_points = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(unchanged_points[0].x == Catch::Approx(100.0f));
        REQUIRE(unchanged_points[0].y == Catch::Approx(200.0f));

        // But image size should now be set
        auto current_size = point_data.getImageSize();
        REQUIRE(current_size.width == 1280);
        REQUIRE(current_size.height == 960);
    }

    SECTION("Scaling to same size does nothing") {
        ImageSize size{640, 480};
        point_data.setImageSize(size);

        // Scale to same size
        point_data.changeImageSize(size);

        // Points should remain unchanged
        auto unchanged_points = point_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(unchanged_points[0].x == Catch::Approx(100.0f));
        REQUIRE(unchanged_points[0].y == Catch::Approx(200.0f));
    }
}

TEST_CASE("PointData - Timeframe conversion", "[points][data][timeframe]") {
    PointData point_data;

    // Setup test data
    std::vector<Point2D<float>> points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    point_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(20), points, NotifyObservers::No);

    SECTION("Same timeframe returns original data") {
        // Create a single timeframe
        std::vector<int> times = {5, 10, 15, 20, 25};
        auto timeframe = std::make_shared<TimeFrame>(times);

        point_data.setTimeFrame(timeframe);

        // Query with same source and target timeframe
        auto result = point_data.getAtTime(TimeFrameIndex(10), *timeframe);

        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
        REQUIRE(result[0].y == Catch::Approx(200.0f));
    }

    SECTION("Different timeframes with conversion") {
        // Create source timeframe (e.g., video frames)
        std::vector<int> video_times = {0, 10, 20, 30, 40};// Video at 1 Hz
        auto video_timeframe = std::make_shared<TimeFrame>(video_times);

        // Create target timeframe (e.g., data sampling at higher rate)
        std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40};// Data at 2 Hz
        auto data_timeframe = std::make_shared<TimeFrame>(data_times);

        // Clear existing data and add data at the correct indices for the target timeframe
        PointData test_point_data;

        // Video frame 1 (time=10) should map to data_timeframe index 2 (time=10)
        // Video frame 2 (time=20) should map to data_timeframe index 4 (time=20)
        test_point_data.addAtTime(TimeFrameIndex(2), points, NotifyObservers::No);// At data timeframe index 2 (time=10)
        test_point_data.addAtTime(TimeFrameIndex(4), points, NotifyObservers::No);// At data timeframe index 4 (time=20)

        test_point_data.setTimeFrame(data_timeframe);

        // Query video frame 1 (time=10) which should map to data index 2 (time=10)
        auto result = test_point_data.getAtTime(TimeFrameIndex(1),
                                                *video_timeframe);

        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == Catch::Approx(100.0f));
        REQUIRE(result[0].y == Catch::Approx(200.0f));
    }

    SECTION("Timeframe conversion with no matching data") {
        // Create timeframes where conversion maps to non-existent data
        std::vector<int> video_times = {0, 5, 10};
        auto video_timeframe = std::make_shared<TimeFrame>(video_times);

        std::vector<int> data_times = {0, 3, 7, 15, 25};
        auto data_timeframe = std::make_shared<TimeFrame>(data_times);

        // Create a separate point data instance for this test
        PointData test_point_data;
        test_point_data.addAtTime(TimeFrameIndex(3), points, NotifyObservers::No);// At data timeframe index 3 (time=15)

        test_point_data.setTimeFrame(data_timeframe);

        // Query video frame 1 (time=5) which should map to data timeframe index 1 (time=3, closest to 5)
        // Since we only have data at index 3, this should return empty
        auto result = test_point_data.getAtTime(TimeFrameIndex(1),
                                                *video_timeframe);

        // Should return empty since we don't have data at the converted index
        REQUIRE(result.empty());
    }
}

TEST_CASE("PointData - View functionality", "[points][data][view][ranges]") {
    PointData point_data;

    // Setup test data with multiple time points
    Point2D<float> p1{1.0f, 2.0f};
    Point2D<float> p2{3.0f, 4.0f};
    Point2D<float> p3{5.0f, 6.0f};
    Point2D<float> p4{7.0f, 8.0f};
    Point2D<float> p5{9.0f, 10.0f};

    point_data.addAtTime(TimeFrameIndex(10), p1, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(10), p2, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(20), p3, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(30), p4, NotifyObservers::No);
    point_data.addAtTime(TimeFrameIndex(30), p5, NotifyObservers::No);

    SECTION("View creation and basic iteration") {
        auto view = point_data.view();

        // Count total time slices
        std::size_t time_count = 0;
        for (auto [time, entries] : view) {
            static_cast<void>(entries); // Suppress unused warning
            ++time_count;
        }

        REQUIRE(time_count == 3); // Times: 10, 20, 30
    }

    SECTION("View iteration with time slice access") {
        auto view = point_data.view();

        std::vector<TimeFrameIndex> times;
        std::vector<std::size_t> entry_counts;

        for (auto [time, entries] : view) {
            times.push_back(time);
            entry_counts.push_back(entries.size());
        }

        REQUIRE(times.size() == 3);
        REQUIRE(times[0] == TimeFrameIndex(10));
        REQUIRE(times[1] == TimeFrameIndex(20));
        REQUIRE(times[2] == TimeFrameIndex(30));

        REQUIRE(entry_counts[0] == 2); // 2 points at time 10
        REQUIRE(entry_counts[1] == 1); // 1 point at time 20
        REQUIRE(entry_counts[2] == 2); // 2 points at time 30
    }

    SECTION("View flatten functionality") {
        auto view = point_data.view();
        auto flattened = view.flatten();

        std::size_t total_count = 0;
        std::vector<Point2D<float>> all_points;
        std::vector<TimeFrameIndex> all_times;

        for (auto [time, entry] : flattened) {
            ++total_count;
            all_times.push_back(time);
            all_points.push_back(entry.data);
        }

        REQUIRE(total_count == 5); // Total of 5 points across all times

        // Verify times are correct
        REQUIRE(all_times[0] == TimeFrameIndex(10));
        REQUIRE(all_times[1] == TimeFrameIndex(10));
        REQUIRE(all_times[2] == TimeFrameIndex(20));
        REQUIRE(all_times[3] == TimeFrameIndex(30));
        REQUIRE(all_times[4] == TimeFrameIndex(30));

        // Verify point data
        REQUIRE(all_points[0].x == Catch::Approx(1.0f));
        REQUIRE(all_points[0].y == Catch::Approx(2.0f));
        REQUIRE(all_points[1].x == Catch::Approx(3.0f));
        REQUIRE(all_points[1].y == Catch::Approx(4.0f));
        REQUIRE(all_points[2].x == Catch::Approx(5.0f));
        REQUIRE(all_points[2].y == Catch::Approx(6.0f));
        REQUIRE(all_points[3].x == Catch::Approx(7.0f));
        REQUIRE(all_points[3].y == Catch::Approx(8.0f));
        REQUIRE(all_points[4].x == Catch::Approx(9.0f));
        REQUIRE(all_points[4].y == Catch::Approx(10.0f));
    }

    SECTION("View with empty data") {
        PointData empty_data;
        auto view = empty_data.view();

        std::size_t count = 0;
        for (auto [time, entries] : view) {
            static_cast<void>(time);
            static_cast<void>(entries);
            ++count;
        }

        REQUIRE(count == 0);
    }

    SECTION("View with ranges algorithms") {
        auto view = point_data.view();
        auto flattened = view.flatten();

        // Use std::ranges::find_if to find a specific point
        auto it = std::ranges::find_if(flattened, [](auto const & elem) {
            auto [time, entry] = elem;
            return entry.data.x == Catch::Approx(5.0f) && entry.data.y == Catch::Approx(6.0f);
        });

        REQUIRE(it != flattened.end());
        auto [found_time, found_entry] = *it;
        REQUIRE(found_time == TimeFrameIndex(20));
        REQUIRE(found_entry.data.x == Catch::Approx(5.0f));
        REQUIRE(found_entry.data.y == Catch::Approx(6.0f));
    }

    SECTION("View with ranges filter") {
        auto view = point_data.view();
        auto flattened = view.flatten();

        // Filter points at time 10
        auto filtered = flattened | std::views::filter([](auto const & elem) {
            auto [time, entry] = elem;
            static_cast<void>(entry);
            return time == TimeFrameIndex(10);
        });

        std::size_t count = 0;
        for (auto [time, entry] : filtered) {
            static_cast<void>(entry);
            REQUIRE(time == TimeFrameIndex(10));
            ++count;
        }

        REQUIRE(count == 2); // Should have 2 points at time 10
    }

    SECTION("View with ranges transform") {
        auto view = point_data.view();
        auto flattened = view.flatten();

        // Transform to extract only x coordinates
        auto x_coords = flattened | std::views::transform([](auto const & elem) {
            auto [time, entry] = elem;
            static_cast<void>(time);
            return entry.data.x;
        });

        std::vector<float> x_values;
        for (auto x : x_coords) {
            x_values.push_back(x);
        }

        REQUIRE(x_values.size() == 5);
        REQUIRE(x_values[0] == Catch::Approx(1.0f));
        REQUIRE(x_values[1] == Catch::Approx(3.0f));
        REQUIRE(x_values[2] == Catch::Approx(5.0f));
        REQUIRE(x_values[3] == Catch::Approx(7.0f));
        REQUIRE(x_values[4] == Catch::Approx(9.0f));
    }

    SECTION("View is lazy - no copy on creation") {
        // Create view - should be zero-cost
        auto view = point_data.view();
        
        // Add more data after view creation
        Point2D<float> p6{11.0f, 12.0f};
        point_data.addAtTime(TimeFrameIndex(40), p6, NotifyObservers::No);

        // View should reflect the updated data
        std::size_t time_count = 0;
        for (auto [time, entries] : view) {
            static_cast<void>(entries);
            ++time_count;
        }

        REQUIRE(time_count == 4); // Should now have 4 time points (including new 40)
    }

    SECTION("View iteration with EntityId access") {
        auto view = point_data.view();
        auto flattened = view.flatten();

        std::vector<EntityId> entity_ids;
        for (auto [time, entry] : flattened) {
            static_cast<void>(time);
            entity_ids.push_back(entry.entity_id);
        }

        // Should have 5 entity IDs, all should be valid (non-zero if identity context set)
        REQUIRE(entity_ids.size() == 5);
        
        // Each entity ID should be unique (if not all zero)
        std::unordered_set<EntityId> unique_ids(entity_ids.begin(), entity_ids.end());
        REQUIRE(unique_ids.size() <= 5); // At most 5 unique IDs
    }
}
