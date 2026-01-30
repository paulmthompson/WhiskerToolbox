
#include "PointInspector.hpp"
#include "PointTableView.hpp"
#include "PointTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/points.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QComboBox>
#include <QTableView>
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <set>
#include <vector>

namespace {
// Ensures a QApplication exists for the process. Must be called before using any Qt widgets/signals.
// Uses a leaked pointer intentionally - QApplication must outlive all Qt objects in tests.
void ensureQApplication()
{
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());// NOLINT: Intentionally leaked
    }
}
}// namespace

// === PointInspector Tests ===

TEST_CASE("PointInspector construction", "[PointInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        PointInspector inspector(data_manager, nullptr, nullptr);

        // Inspector should be created without crashing
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        PointInspector inspector(data_manager, nullptr, nullptr);

        REQUIRE(inspector.getDataType() == DM_DataType::Points);
        REQUIRE(inspector.getTypeName() == QStringLiteral("Point"));
        REQUIRE(inspector.supportsExport() == true);
    }
}

TEST_CASE("PointInspector has expected UI", "[PointInspector]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Contains group filter combo box") {
        auto data_manager = std::make_shared<DataManager>();
        PointInspector inspector(data_manager, nullptr, nullptr);

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);
        REQUIRE(group_filter_combo->count() == 1);  // Should have "All Groups" initially
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));

        app->processEvents();
    }
}

TEST_CASE("PointInspector group filter updates when groups are added", "[PointInspector][Groups]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Group filter combo updates when groups are created") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        // Initially should have only "All Groups"
        REQUIRE(group_filter_combo->count() == 1);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));

        // Create first group
        int group1_id = group_manager->createGroup("Group A");
        app->processEvents();

        // Combo box should now have "All Groups" and "Group A"
        REQUIRE(group_filter_combo->count() == 2);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));

        // Create second group
        int group2_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Combo box should now have "All Groups", "Group A", and "Group B"
        REQUIRE(group_filter_combo->count() == 3);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));

        // Create third group
        int group3_id = group_manager->createGroup("Group C");
        app->processEvents();

        // Combo box should now have all four items
        REQUIRE(group_filter_combo->count() == 4);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));
        REQUIRE(group_filter_combo->itemText(3) == QStringLiteral("Group C"));
    }

    SECTION("Group filter combo updates when groups are removed") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        // Create groups
        int group1_id = group_manager->createGroup("Group A");
        int group2_id = group_manager->createGroup("Group B");
        int group3_id = group_manager->createGroup("Group C");
        app->processEvents();

        REQUIRE(group_filter_combo->count() == 4);  // "All Groups" + 3 groups

        // Remove middle group
        group_manager->removeGroup(group2_id);
        app->processEvents();

        // Should still have "All Groups" + 2 groups
        REQUIRE(group_filter_combo->count() == 3);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group C"));
    }
}

// === PointTableView Tests ===

TEST_CASE("PointTableView construction", "[PointTableView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with data manager") {
        auto data_manager = std::make_shared<DataManager>();
        PointTableView view(data_manager, nullptr);

        // View should be created without crashing
        app->processEvents();
    }

    SECTION("Returns correct data type") {
        auto data_manager = std::make_shared<DataManager>();
        PointTableView view(data_manager, nullptr);

        REQUIRE(view.getDataType() == DM_DataType::Points);
        REQUIRE(view.getTypeName() == QStringLiteral("Point Table"));
    }

    SECTION("Has table view") {
        auto data_manager = std::make_shared<DataManager>();
        PointTableView view(data_manager, nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        REQUIRE(table_view->model() != nullptr);
    }
}

TEST_CASE("PointTableView displays point data", "[PointTableView]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table shows points from PointData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with some points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points at different frames
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        PointTableView view(data_manager, nullptr);
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Should have 4 rows (4 points)
        REQUIRE(model->rowCount() == 4);

        // Verify frame column data
        REQUIRE(model->data(model->index(0, 0)).toInt() == 0);
        REQUIRE(model->data(model->index(1, 0)).toInt() == 0);
        REQUIRE(model->data(model->index(2, 0)).toInt() == 10);
        REQUIRE(model->data(model->index(3, 0)).toInt() == 20);
    }
}

TEST_CASE("PointTableView group filtering", "[PointTableView][Groups]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Table filters by group correctly") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points at different frames
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);  // Will be Group A
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);  // Will be Group B
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No); // Will be Group A
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No); // Will be ungrouped

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs for the points
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];  // First point at frame 0
        EntityId entity1 = entity_ids_frame0[1];   // Second point at frame 0
        EntityId entity2 = entity_ids_frame10[0];  // Point at frame 10

        // Create groups and assign entities
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");

        // Assign entities to groups
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity2});
        group_manager->assignEntitiesToGroup(group_b_id, {entity1});

        // Create view and set group manager
        PointTableView view(data_manager, nullptr);
        view.setGroupManager(group_manager.get());
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should show all 4 points
        REQUIRE(model->rowCount() == 4);

        // Filter by Group A
        view.setGroupFilter(group_a_id);
        app->processEvents();

        // Should show only 2 points (entity0 and entity2)
        REQUIRE(model->rowCount() == 2);
        // Verify they are from frames 0 and 10
        std::set<int> frames;
        for (int row = 0; row < model->rowCount(); ++row) {
            int frame = model->data(model->index(row, 0)).toInt();
            frames.insert(frame);
        }
        REQUIRE(frames.count(0) == 1);
        REQUIRE(frames.count(10) == 1);

        // Filter by Group B
        view.setGroupFilter(group_b_id);
        app->processEvents();

        // Should show only 1 point (entity1)
        REQUIRE(model->rowCount() == 1);
        REQUIRE(model->data(model->index(0, 0)).toInt() == 0);

        // Clear filter
        view.clearGroupFilter();
        app->processEvents();

        // Should show all 4 points again
        REQUIRE(model->rowCount() == 4);
    }
}

// === Integration Tests: PointInspector and PointTableView Together ===

TEST_CASE("PointInspector and PointTableView integration with groups", "[PointInspector][PointTableView][Groups]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Group filter combo updates and table filters when groups are added") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should show all points and have only "All Groups" in combo
        REQUIRE(model->rowCount() == 4);
        REQUIRE(group_filter_combo->count() == 1);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));

        // Create groups
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Combo box should update
        REQUIRE(group_filter_combo->count() == 3);  // "All Groups" + 2 groups
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));

        // Assign entities to groups
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity2});
        group_manager->assignEntitiesToGroup(group_b_id, {entity1});
        app->processEvents();

        // Table should still show all points (no filter applied yet)
        REQUIRE(model->rowCount() == 4);

        // Filter by Group A using the combo box (this should trigger _onGroupFilterChanged)
        group_filter_combo->setCurrentIndex(1);
        app->processEvents();

        // Table should now show only 2 points (entity0 and entity2)
        REQUIRE(model->rowCount() == 2);

        // Change filter to Group B using the combo box
        group_filter_combo->setCurrentIndex(2);
        app->processEvents();

        // Table should now show only 1 point (entity1)
        REQUIRE(model->rowCount() == 1);

        // Clear filter by selecting "All Groups" in the combo box
        group_filter_combo->setCurrentIndex(0);
        app->processEvents();

        // Table should show all 4 points again
        REQUIRE(model->rowCount() == 4);
    }

    SECTION("Adding new groups updates combo box while maintaining filter") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Create inspector
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        inspector.setActiveKey("test_points");

        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        // Create first group
        int group_a_id = group_manager->createGroup("Group A");
        app->processEvents();

        REQUIRE(group_filter_combo->count() == 2);  // "All Groups" + "Group A"

        // Select Group A in combo box
        group_filter_combo->setCurrentIndex(1);
        app->processEvents();

        // Create second group
        int group_b_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Combo box should have updated with new group
        REQUIRE(group_filter_combo->count() == 3);
        REQUIRE(group_filter_combo->itemText(0) == QStringLiteral("All Groups"));
        REQUIRE(group_filter_combo->itemText(1) == QStringLiteral("Group A"));
        REQUIRE(group_filter_combo->itemText(2) == QStringLiteral("Group B"));

        // Selection should still be on Group A (index 1)
        REQUIRE(group_filter_combo->currentIndex() == 1);
        REQUIRE(group_filter_combo->currentText() == QStringLiteral("Group A"));
    }

    SECTION("Group filter combo box changes update table with correct filtered rows") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points at different frames
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);  // Will be Group A
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);  // Will be Group B
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No); // Will be Group A
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No); // Will be Group B
        point_data->addAtTime(TimeFrameIndex(30), Point2D<float>{90.0f, 100.0f}, NotifyObservers::No); // Will be ungrouped

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_frame20 = point_data->getEntityIdsAtTime(TimeFrameIndex(20));
        auto entity_ids_frame30 = point_data->getEntityIdsAtTime(TimeFrameIndex(30));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);
        REQUIRE(entity_ids_frame20.size() == 1);
        REQUIRE(entity_ids_frame30.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];  // Group A
        EntityId entity1 = entity_ids_frame0[1];  // Group B
        EntityId entity2 = entity_ids_frame10[0]; // Group A
        EntityId entity3 = entity_ids_frame20[0]; // Group B
        EntityId entity4 = entity_ids_frame30[0]; // Ungrouped

        // Create groups and assign entities
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity2});
        group_manager->assignEntitiesToGroup(group_b_id, {entity1, entity3});
        app->processEvents();

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should show all 5 points
        REQUIRE(model->rowCount() == 5);
        REQUIRE(group_filter_combo->currentIndex() == 0);  // "All Groups"

        // Filter by Group A (index 1)
        group_filter_combo->setCurrentIndex(1);
        app->processEvents();

        // Should show only 2 points (entity0 and entity2)
        REQUIRE(model->rowCount() == 2);
        
        // Verify the filtered rows contain the correct entities
        std::set<EntityId> filtered_entities;
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            filtered_entities.insert(row_data.entity_id);
        }
        REQUIRE(filtered_entities.count(entity0) == 1);
        REQUIRE(filtered_entities.count(entity2) == 1);
        REQUIRE(filtered_entities.count(entity1) == 0);
        REQUIRE(filtered_entities.count(entity3) == 0);
        REQUIRE(filtered_entities.count(entity4) == 0);

        // Filter by Group B (index 2)
        group_filter_combo->setCurrentIndex(2);
        app->processEvents();

        // Should show only 2 points (entity1 and entity3)
        REQUIRE(model->rowCount() == 2);
        
        filtered_entities.clear();
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            filtered_entities.insert(row_data.entity_id);
        }
        REQUIRE(filtered_entities.count(entity1) == 1);
        REQUIRE(filtered_entities.count(entity3) == 1);
        REQUIRE(filtered_entities.count(entity0) == 0);
        REQUIRE(filtered_entities.count(entity2) == 0);
        REQUIRE(filtered_entities.count(entity4) == 0);

        // Clear filter (back to "All Groups")
        group_filter_combo->setCurrentIndex(0);
        app->processEvents();

        // Should show all 5 points again
        REQUIRE(model->rowCount() == 5);
    }

    SECTION("Table automatically updates when new members are added to filtered group") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add initial points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);  // Will be Group A
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No); // Will be Group A
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);  // Will be ungrouped initially

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_frame20 = point_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(entity_ids_frame0.size() == 1);
        REQUIRE(entity_ids_frame10.size() == 1);
        REQUIRE(entity_ids_frame20.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];  // Group A
        EntityId entity1 = entity_ids_frame10[0]; // Group A
        EntityId entity2 = entity_ids_frame20[0]; // Will be added to Group A later

        // Create group and assign initial entities
        int group_a_id = group_manager->createGroup("Group A");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity1});
        app->processEvents();

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * group_filter_combo = inspector.findChild<QComboBox *>("groupFilterCombo");
        REQUIRE(group_filter_combo != nullptr);

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Filter by Group A
        group_filter_combo->setCurrentIndex(1);
        app->processEvents();

        // Initially should show only 2 points (entity0 and entity1)
        REQUIRE(model->rowCount() == 2);
        
        std::set<EntityId> filtered_entities;
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            filtered_entities.insert(row_data.entity_id);
        }
        REQUIRE(filtered_entities.count(entity0) == 1);
        REQUIRE(filtered_entities.count(entity1) == 1);
        REQUIRE(filtered_entities.count(entity2) == 0);

        // Add entity2 to Group A
        group_manager->assignEntitiesToGroup(group_a_id, {entity2});
        app->processEvents();

        // Table should automatically update to show 3 points now (entity0, entity1, entity2)
        // The view listens to groupModified signal and refreshes automatically
        REQUIRE(model->rowCount() == 3);
        
        filtered_entities.clear();
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            filtered_entities.insert(row_data.entity_id);
        }
        REQUIRE(filtered_entities.count(entity0) == 1);
        REQUIRE(filtered_entities.count(entity1) == 1);
        REQUIRE(filtered_entities.count(entity2) == 1);
    }
}

// === Move and Copy Tests ===

TEST_CASE("PointInspector and PointTableView move and copy operations", "[PointInspector][PointTableView][MoveCopy]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Move points to target PointData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create source PointData with points
        auto source_point_data = std::make_shared<PointData>();
        source_point_data->setIdentityContext("source_points", data_manager->getEntityRegistry());

        // Add points to source
        source_point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        source_point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        source_point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);
        source_point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        source_point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("source_points", source_point_data, TimeKey("time"));

        // Create target PointData (empty)
        auto target_point_data = std::make_shared<PointData>();
        target_point_data->setIdentityContext("target_points", data_manager->getEntityRegistry());
        data_manager->setData<PointData>("target_points", target_point_data, TimeKey("time"));

        // Get entity IDs from source
        auto entity_ids_frame0 = source_point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = source_point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, nullptr, nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("source_points");
        view.setActiveKey("source_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially source should have 4 points, target should have 0
        REQUIRE(model->rowCount() == 4);
        REQUIRE(target_point_data->getTimesWithData().size() == 0);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity0) != selected_entity_ids.end());
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity1) != selected_entity_ids.end());

        // Emit move signal (simulating context menu selection)
        emit view.movePointsRequested("target_points");
        app->processEvents();

        // Source should now have 2 points (entity2 and the one at frame 20)
        view.updateView();
        app->processEvents();
        REQUIRE(model->rowCount() == 2);

        // Target should have 2 points (entity0 and entity1)
        target_point_data->rebuildAllEntityIds();
        auto target_times = target_point_data->getTimesWithData();
        REQUIRE(target_times.size() == 1);  // Should have data at frame 0
        REQUIRE(target_point_data->getAtTime(TimeFrameIndex(0)).size() == 2);

        // Verify source still has entity2
        auto remaining_entities = view.getSelectedEntityIds();
        // Clear selection first
        selection_model->clearSelection();
        app->processEvents();
        
        // Check that entity2 is still in source
        auto source_entity_ids_frame10 = source_point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(source_entity_ids_frame10.size() == 1);
        REQUIRE(source_entity_ids_frame10[0] == entity2);
    }

    SECTION("Copy points to target PointData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create source PointData with points
        auto source_point_data = std::make_shared<PointData>();
        source_point_data->setIdentityContext("source_points", data_manager->getEntityRegistry());

        // Add points to source
        source_point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        source_point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        source_point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        source_point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("source_points", source_point_data, TimeKey("time"));

        // Create target PointData (empty)
        auto target_point_data = std::make_shared<PointData>();
        target_point_data->setIdentityContext("target_points", data_manager->getEntityRegistry());
        data_manager->setData<PointData>("target_points", target_point_data, TimeKey("time"));

        // Get entity IDs from source
        auto entity_ids_frame0 = source_point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = source_point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, nullptr, nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("source_points");
        view.setActiveKey("source_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially source should have 3 points, target should have 0
        REQUIRE(model->rowCount() == 3);
        REQUIRE(target_point_data->getTimesWithData().size() == 0);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit copy signal (simulating context menu selection)
        emit view.copyPointsRequested("target_points");
        app->processEvents();

        // Source should still have 3 points (unchanged)
        view.updateView();
        app->processEvents();
        REQUIRE(model->rowCount() == 3);

        // Target should have 2 points (copies of entity0 and entity1)
        target_point_data->rebuildAllEntityIds();
        auto target_times = target_point_data->getTimesWithData();
        REQUIRE(target_times.size() == 1);  // Should have data at frame 0
        REQUIRE(target_point_data->getAtTime(TimeFrameIndex(0)).size() == 2);

        // Verify source still has all original points
        REQUIRE(source_point_data->getAtTime(TimeFrameIndex(0)).size() == 2);
        REQUIRE(source_point_data->getAtTime(TimeFrameIndex(10)).size() == 1);
    }
}

// === Group Management Context Menu Tests ===

TEST_CASE("PointInspector and PointTableView group management context menu", "[PointInspector][PointTableView][GroupManagement]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Move points to group via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create groups
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        app->processEvents();

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially, no entities should be in groups
        REQUIRE(group_manager->getEntityGroup(entity0) == -1);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);
        REQUIRE(group_manager->getEntityGroup(entity2) == -1);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit move to group signal (simulating context menu selection)
        emit view.movePointsToGroupRequested(group_a_id);
        app->processEvents();

        // Verify entities are now in Group A
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity2) == -1);  // Not selected, should remain ungrouped

        // Verify table updates to show group names
        view.updateView();
        app->processEvents();
        
        // Check that the group names are updated in the model
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            if (row_data.entity_id == entity0 || row_data.entity_id == entity1) {
                REQUIRE(row_data.group_name == QStringLiteral("Group A"));
            } else if (row_data.entity_id == entity2) {
                REQUIRE(row_data.group_name == QStringLiteral("No Group"));
            }
        }

        // Now select entity2 and move it to Group B
        selection_model->clearSelection();
        selection_model->select(model->index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        emit view.movePointsToGroupRequested(group_b_id);
        app->processEvents();

        // Verify entity2 is now in Group B
        REQUIRE(group_manager->getEntityGroup(entity2) == group_b_id);
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);  // Should still be in Group A
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);  // Should still be in Group A
    }

    SECTION("Remove points from group via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];

        // Create group and assign entities
        int group_a_id = group_manager->createGroup("Group A");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0, entity1, entity2});
        app->processEvents();

        // Verify all entities are in Group A
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity2) == group_a_id);

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);

        // Emit remove from group signal (simulating context menu selection)
        emit view.removePointsFromGroupRequested();
        app->processEvents();

        // Verify selected entities are removed from group
        REQUIRE(group_manager->getEntityGroup(entity0) == -1);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);
        REQUIRE(group_manager->getEntityGroup(entity2) == group_a_id);  // Not selected, should remain in group

        // Verify table updates to show group names
        view.updateView();
        app->processEvents();
        
        // Check that the group names are updated in the model
        for (int row = 0; row < model->rowCount(); ++row) {
            auto row_data = static_cast<PointTableModel *>(model)->getRowData(row);
            if (row_data.entity_id == entity0 || row_data.entity_id == entity1) {
                REQUIRE(row_data.group_name == QStringLiteral("No Group"));
            } else if (row_data.entity_id == entity2) {
                REQUIRE(row_data.group_name == QStringLiteral("Group A"));
            }
        }
    }

    SECTION("Move points from one group to another via context menu") {
        auto data_manager = std::make_shared<DataManager>();
        auto entity_group_manager = std::make_unique<EntityGroupManager>();
        auto group_manager = std::make_unique<GroupManager>(entity_group_manager.get(), data_manager);

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        REQUIRE(entity_ids_frame0.size() == 2);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];

        // Create groups and assign entity0 to Group A
        int group_a_id = group_manager->createGroup("Group A");
        int group_b_id = group_manager->createGroup("Group B");
        group_manager->assignEntitiesToGroup(group_a_id, {entity0});
        app->processEvents();

        // Verify initial group assignment
        REQUIRE(group_manager->getEntityGroup(entity0) == group_a_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, group_manager.get(), nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Select first row (entity0)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Move entity0 from Group A to Group B
        emit view.movePointsToGroupRequested(group_b_id);
        app->processEvents();

        // Verify entity0 is now in Group B (moved from Group A)
        REQUIRE(group_manager->getEntityGroup(entity0) == group_b_id);
        REQUIRE(group_manager->getEntityGroup(entity1) == -1);  // Should remain ungrouped

        // Verify table updates
        view.updateView();
        app->processEvents();
        
        // Check that the group name is updated in the model
        auto row_data = static_cast<PointTableModel *>(model)->getRowData(0);
        REQUIRE(row_data.entity_id == entity0);
        REQUIRE(row_data.group_name == QStringLiteral("Group B"));
    }
}

// === Delete Points Tests ===

TEST_CASE("PointInspector and PointTableView delete points", "[PointInspector][PointTableView][Delete]") {
    ensureQApplication();

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Delete selected points via context menu") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{50.0f, 60.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(20), Point2D<float>{70.0f, 80.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Get entity IDs
        auto entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        auto entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_frame20 = point_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(entity_ids_frame0.size() == 2);
        REQUIRE(entity_ids_frame10.size() == 1);
        REQUIRE(entity_ids_frame20.size() == 1);

        EntityId entity0 = entity_ids_frame0[0];
        EntityId entity1 = entity_ids_frame0[1];
        EntityId entity2 = entity_ids_frame10[0];
        EntityId entity3 = entity_ids_frame20[0];

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, nullptr, nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should have 4 points
        REQUIRE(model->rowCount() == 4);
        REQUIRE(point_data->getAtTime(TimeFrameIndex(0)).size() == 2);
        REQUIRE(point_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(point_data->getAtTime(TimeFrameIndex(20)).size() == 1);

        // Select first two rows (entity0 and entity1)
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Verify selection
        auto selected_entity_ids = view.getSelectedEntityIds();
        REQUIRE(selected_entity_ids.size() == 2);
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity0) != selected_entity_ids.end());
        REQUIRE(std::find(selected_entity_ids.begin(), selected_entity_ids.end(), entity1) != selected_entity_ids.end());

        // Emit delete signal (simulating context menu selection)
        emit view.deletePointsRequested();
        app->processEvents();

        // Verify points were deleted
        view.updateView();
        app->processEvents();
        
        // Should now have 2 points (entity2 and entity3)
        REQUIRE(model->rowCount() == 2);
        REQUIRE(point_data->getAtTime(TimeFrameIndex(0)).size() == 0);  // Both points at frame 0 deleted
        REQUIRE(point_data->getAtTime(TimeFrameIndex(10)).size() == 1);  // entity2 still exists
        REQUIRE(point_data->getAtTime(TimeFrameIndex(20)).size() == 1);  // entity3 still exists

        // Verify entity0 and entity1 are gone
        auto remaining_entity_ids_frame0 = point_data->getEntityIdsAtTime(TimeFrameIndex(0));
        REQUIRE(remaining_entity_ids_frame0.size() == 0);
        
        // Verify entity2 and entity3 still exist
        auto remaining_entity_ids_frame10 = point_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto remaining_entity_ids_frame20 = point_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(remaining_entity_ids_frame10.size() == 1);
        REQUIRE(remaining_entity_ids_frame20.size() == 1);
        REQUIRE(remaining_entity_ids_frame10[0] == entity2);
        REQUIRE(remaining_entity_ids_frame20[0] == entity3);
    }

    SECTION("Delete all points leaves empty PointData") {
        auto data_manager = std::make_shared<DataManager>();

        // Create timeframe
        constexpr int kNumTimes = 100;
        std::vector<int> t(kNumTimes);
        std::iota(t.begin(), t.end(), 0);
        auto tf = std::make_shared<TimeFrame>(t);
        data_manager->setTime(TimeKey("time"), tf);

        // Create PointData with points
        auto point_data = std::make_shared<PointData>();
        point_data->setIdentityContext("test_points", data_manager->getEntityRegistry());

        // Add points
        point_data->addAtTime(TimeFrameIndex(0), Point2D<float>{10.0f, 20.0f}, NotifyObservers::No);
        point_data->addAtTime(TimeFrameIndex(10), Point2D<float>{30.0f, 40.0f}, NotifyObservers::No);

        // Rebuild entity IDs
        point_data->rebuildAllEntityIds();

        data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

        // Create inspector and view, and connect them
        PointInspector inspector(data_manager, nullptr, nullptr);
        PointTableView view(data_manager, nullptr);
        inspector.setTableView(&view);

        inspector.setActiveKey("test_points");
        view.setActiveKey("test_points");

        app->processEvents();

        auto * table_view = view.tableView();
        REQUIRE(table_view != nullptr);
        auto * model = table_view->model();
        REQUIRE(model != nullptr);

        // Initially should have 2 points
        REQUIRE(model->rowCount() == 2);

        // Select all rows
        auto * selection_model = table_view->selectionModel();
        REQUIRE(selection_model != nullptr);
        selection_model->select(model->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        selection_model->select(model->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        app->processEvents();

        // Emit delete signal
        emit view.deletePointsRequested();
        app->processEvents();

        // Verify all points were deleted
        view.updateView();
        app->processEvents();
        
        REQUIRE(model->rowCount() == 0);
        REQUIRE(point_data->getTimesWithData().size() == 0);
    }
}
