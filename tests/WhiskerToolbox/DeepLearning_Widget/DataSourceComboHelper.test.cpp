/// @file DataSourceComboHelper.test.cpp
/// @brief Tests for the DataSourceComboHelper utility.
///
/// Verifies combo population, tracking/untracking, type filtering,
/// and the typesFromHint static utility.

#include <catch2/catch_test_macros.hpp>

#include "DeepLearning_Widget/UI/Helpers/DataSourceComboHelper.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <QApplication>
#include <QComboBox>

#include <memory>

// Ensure a QApplication exists for QWidget-based tests
namespace {
struct QtAppGuard {
    QtAppGuard() {
        if (QApplication::instance() == nullptr) {
            static int argc = 1;
            static char const * argv[] = {"test"};
            static auto app = std::make_unique<QApplication>(argc, const_cast<char **>(argv));
        }
    }
};
QtAppGuard const s_guard;
}// namespace

using dl::widget::DataSourceComboHelper;

// ============================================================================
// typesFromHint — static utility
// ============================================================================

TEST_CASE("typesFromHint converts known type strings",
          "[dl_widget][combo_helper]") {
    CHECK(DataSourceComboHelper::typesFromHint("MediaData").size() == 2);
    CHECK(DataSourceComboHelper::typesFromHint("PointData") == std::vector{DM_DataType::Points});
    CHECK(DataSourceComboHelper::typesFromHint("MaskData") == std::vector{DM_DataType::Mask});
    CHECK(DataSourceComboHelper::typesFromHint("LineData") == std::vector{DM_DataType::Line});
    CHECK(DataSourceComboHelper::typesFromHint("AnalogTimeSeries") == std::vector{DM_DataType::Analog});
    CHECK(DataSourceComboHelper::typesFromHint("TensorData") == std::vector{DM_DataType::Tensor});
}

TEST_CASE("typesFromHint returns empty for unknown strings",
          "[dl_widget][combo_helper]") {
    CHECK(DataSourceComboHelper::typesFromHint("").empty());
    CHECK(DataSourceComboHelper::typesFromHint("FooBar").empty());
}

// ============================================================================
// populateCombo — with empty DataManager
// ============================================================================

TEST_CASE("populateCombo adds (None) sentinel when no matching keys",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    DataSourceComboHelper const helper(dm);

    QComboBox combo;
    // DataManager constructor creates a default "media" key, so filter by
    // a type that has no data to verify the sentinel behaviour.
    helper.populateCombo(&combo, DM_DataType::Points);

    REQUIRE(combo.count() == 1);
    CHECK(combo.itemText(0) == "(None)");
}

// ============================================================================
// populateCombo — with data
// ============================================================================

TEST_CASE("populateCombo populates from DataManager keys",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("points_1", TimeKey("time"));
    dm->setData<PointData>("points_2", TimeKey("time"));
    dm->setData<LineData>("lines_1", TimeKey("time"));

    DataSourceComboHelper const helper(dm);
    QComboBox combo;

    SECTION("all keys (empty filter)") {
        helper.populateCombo(&combo, std::vector<DM_DataType>{});
        // (None) + media (default) + points_1 + points_2 + lines_1 = 5
        CHECK(combo.count() == 5);
    }

    SECTION("filter by PointData") {
        helper.populateCombo(&combo, DM_DataType::Points);
        // (None) + points_1 + points_2 = 3
        CHECK(combo.count() == 3);
    }

    SECTION("filter by LineData") {
        helper.populateCombo(&combo, DM_DataType::Line);
        // (None) + lines_1 = 2
        CHECK(combo.count() == 2);
    }

    SECTION("filter by MaskData yields no data keys") {
        helper.populateCombo(&combo, DM_DataType::Mask);
        CHECK(combo.count() == 1);// Just (None)
    }
}

// ============================================================================
// populateCombo — preserves selection
// ============================================================================

TEST_CASE("populateCombo preserves current selection if still valid",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("A", TimeKey("time"));
    dm->setData<PointData>("B", TimeKey("time"));

    DataSourceComboHelper const helper(dm);
    QComboBox combo;
    helper.populateCombo(&combo, DM_DataType::Points);

    // Select "B"
    combo.setCurrentIndex(combo.findText("B"));
    REQUIRE(combo.currentText() == "B");

    // Re-populate — should keep "B" selected
    helper.populateCombo(&combo, DM_DataType::Points);
    CHECK(combo.currentText() == "B");
}

// ============================================================================
// track / untrack / refreshAll
// ============================================================================

TEST_CASE("track registers combo for refreshAll",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    DataSourceComboHelper helper(dm);

    QComboBox combo;
    helper.track(&combo, DM_DataType::Points);
    CHECK(helper.trackedCount() == 1);

    // Initially empty DM → (None) only
    helper.refreshAll();
    CHECK(combo.count() == 1);

    // Add PointData
    dm->setData<PointData>("pts", TimeKey("time"));
    helper.refreshAll();
    CHECK(combo.count() == 2);// (None) + pts
}

TEST_CASE("untrack removes combo from refreshAll",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts", TimeKey("time"));

    DataSourceComboHelper helper(dm);
    QComboBox combo;

    helper.track(&combo, DM_DataType::Points);
    CHECK(helper.trackedCount() == 1);

    helper.untrack(&combo);
    CHECK(helper.trackedCount() == 0);
}

TEST_CASE("tracked combo auto-untracked on destruction",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    DataSourceComboHelper helper(dm);

    {
        auto * combo = new QComboBox;
        helper.track(combo, DM_DataType::Points);
        CHECK(helper.trackedCount() == 1);

        delete combo;// Should trigger QObject::destroyed → untrack
    }

    CHECK(helper.trackedCount() == 0);
}

TEST_CASE("re-tracking same combo updates filter",
          "[dl_widget][combo_helper]") {
    auto dm = std::make_shared<DataManager>();
    dm->setData<PointData>("pts", TimeKey("time"));
    dm->setData<LineData>("lines", TimeKey("time"));

    DataSourceComboHelper helper(dm);
    QComboBox combo;

    helper.track(&combo, DM_DataType::Points);
    helper.refreshAll();
    CHECK(combo.count() == 2);// (None) + pts

    // Re-track with different filter
    helper.track(&combo, DM_DataType::Line);
    helper.refreshAll();
    CHECK(combo.count() == 2);// (None) + lines

    // Should still be 1 tracked entry, not 2
    CHECK(helper.trackedCount() == 1);
}
