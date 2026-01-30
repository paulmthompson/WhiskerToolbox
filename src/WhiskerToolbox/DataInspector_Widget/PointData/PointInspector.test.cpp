
#include "PointInspector.hpp"
#include "PointTableView.hpp"
#include "PointTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "Entity/EntityGroupManager.hpp"
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
