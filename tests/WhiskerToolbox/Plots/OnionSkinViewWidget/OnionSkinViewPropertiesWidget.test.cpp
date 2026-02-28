/**
 * @file OnionSkinViewPropertiesWidget.test.cpp
 * @brief Unit tests for OnionSkinViewPropertiesWidget
 *
 * Tests the properties widget for OnionSkinViewWidget, including:
 * - Combo box population with PointData, LineData, and MaskData keys
 * - DataManager observer callback functionality
 * - Combo box refresh when data is added/removed
 * - Stale key handling when data is deleted
 */

#include "UI/OnionSkinViewPropertiesWidget.hpp"
#include "Core/OnionSkinViewState.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "Observer/Observer_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QComboBox>
#include <QTableWidget>

#include <memory>
#include <string>
#include <vector>

// =============================================================================
// Helpers
// =============================================================================

namespace {

std::shared_ptr<TimeFrame> createTestTimeFrame()
{
    std::vector<int> times;
    times.reserve(100);
    for (int i = 0; i < 100; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

std::shared_ptr<DataManager> createDataManager()
{
    auto dm = std::make_shared<DataManager>();
    auto tf = createTestTimeFrame();
    dm->setTime(TimeKey("time"), tf);
    return dm;
}

void addPointData(DataManager & dm, std::string const & key)
{
    auto tf = dm.getTime(TimeKey("time"));
    std::vector<Point2D<float>> points = {{1.0f, 2.0f}, {3.0f, 4.0f}};
    auto point_data = std::make_shared<PointData>();
    point_data->addAtTime(TimeFrameIndex(0), points, NotifyObservers::No);
    point_data->setTimeFrame(tf);
    dm.setData<PointData>(key, point_data, TimeKey("time"));
}

void addLineData(DataManager & dm, std::string const & key)
{
    auto tf = dm.getTime(TimeKey("time"));
    auto line_data = std::make_shared<LineData>();
    Line2D line = {{0.0f, 0.0f}, {10.0f, 10.0f}};
    line_data->addAtTime(TimeFrameIndex(0), line, NotifyObservers::No);
    line_data->setTimeFrame(tf);
    dm.setData<LineData>(key, line_data, TimeKey("time"));
}

void addMaskData(DataManager & dm, std::string const & key)
{
    auto tf = dm.getTime(TimeKey("time"));
    auto mask_data = std::make_shared<MaskData>();
    mask_data->setTimeFrame(tf);
    dm.setData<MaskData>(key, mask_data, TimeKey("time"));
}

/// Find the point combo box
QComboBox * findPointCombo(OnionSkinViewPropertiesWidget * widget)
{
    return widget->findChild<QComboBox *>("add_point_combo");
}

/// Find the line combo box
QComboBox * findLineCombo(OnionSkinViewPropertiesWidget * widget)
{
    return widget->findChild<QComboBox *>("add_line_combo");
}

/// Find the mask combo box
QComboBox * findMaskCombo(OnionSkinViewPropertiesWidget * widget)
{
    return widget->findChild<QComboBox *>("add_mask_combo");
}

/// Check if combo contains a key
bool comboContainsKey(QComboBox * combo, std::string const & key)
{
    if (!combo) {
        return false;
    }
    return combo->findText(QString::fromStdString(key)) >= 0;
}

}  // namespace

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("OnionSkinViewPropertiesWidget combo box population", "[OnionSkinViewPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("empty combo boxes when no data available")
    {
        auto dm = createDataManager();
        auto state = std::make_shared<OnionSkinViewState>();

        OnionSkinViewPropertiesWidget widget(state, dm);

        auto * point_combo = findPointCombo(&widget);
        auto * line_combo = findLineCombo(&widget);
        auto * mask_combo = findMaskCombo(&widget);

        REQUIRE(point_combo != nullptr);
        REQUIRE(line_combo != nullptr);
        REQUIRE(mask_combo != nullptr);
        REQUIRE(point_combo->count() == 0);
        REQUIRE(line_combo->count() == 0);
        REQUIRE(mask_combo->count() == 0);
    }

    SECTION("combo boxes populated with data keys")
    {
        auto dm = createDataManager();
        addPointData(*dm, "points_1");
        addPointData(*dm, "points_2");
        addLineData(*dm, "lines_1");
        addMaskData(*dm, "masks_1");

        auto state = std::make_shared<OnionSkinViewState>();
        OnionSkinViewPropertiesWidget widget(state, dm);

        auto * point_combo = findPointCombo(&widget);
        auto * line_combo = findLineCombo(&widget);
        auto * mask_combo = findMaskCombo(&widget);

        REQUIRE(point_combo->count() == 2);
        REQUIRE(comboContainsKey(point_combo, "points_1"));
        REQUIRE(comboContainsKey(point_combo, "points_2"));

        REQUIRE(line_combo->count() == 1);
        REQUIRE(comboContainsKey(line_combo, "lines_1"));

        REQUIRE(mask_combo->count() == 1);
        REQUIRE(comboContainsKey(mask_combo, "masks_1"));
    }
}

TEST_CASE("OnionSkinViewPropertiesWidget DataManager observer", "[OnionSkinViewPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("observer refresh on data add")
    {
        auto dm = createDataManager();
        auto state = std::make_shared<OnionSkinViewState>();

        OnionSkinViewPropertiesWidget widget(state, dm);

        auto * point_combo = findPointCombo(&widget);
        REQUIRE(point_combo != nullptr);
        REQUIRE(point_combo->count() == 0);

        // Add data — observer should trigger repopulation
        addPointData(*dm, "new_points");
        QApplication::processEvents();

        REQUIRE(point_combo->count() == 1);
        REQUIRE(comboContainsKey(point_combo, "new_points"));
    }

    SECTION("observer refresh on data remove")
    {
        auto dm = createDataManager();
        addLineData(*dm, "to_remove");
        addLineData(*dm, "to_keep");

        auto state = std::make_shared<OnionSkinViewState>();
        OnionSkinViewPropertiesWidget widget(state, dm);

        auto * line_combo = findLineCombo(&widget);
        REQUIRE(comboContainsKey(line_combo, "to_remove"));
        REQUIRE(comboContainsKey(line_combo, "to_keep"));

        // Remove data
        dm->deleteData("to_remove");
        QApplication::processEvents();

        REQUIRE_FALSE(comboContainsKey(line_combo, "to_remove"));
        REQUIRE(comboContainsKey(line_combo, "to_keep"));
    }

    SECTION("observer cleaned up on destruction - no crash")
    {
        auto dm = createDataManager();

        {
            auto state = std::make_shared<OnionSkinViewState>();
            OnionSkinViewPropertiesWidget widget(state, dm);
            addPointData(*dm, "before_destroy");
            QApplication::processEvents();
        }
        // Widget is destroyed — observer must have been removed

        // Adding more data must NOT crash (no dangling callback)
        addPointData(*dm, "after_destroy");
        QApplication::processEvents();

        REQUIRE(true);  // If we get here without crashing, test passes
    }
}

TEST_CASE("OnionSkinViewPropertiesWidget stale key handling", "[OnionSkinViewPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("point key removed from state when data deleted")
    {
        auto dm = createDataManager();
        addPointData(*dm, "point_key");

        auto state = std::make_shared<OnionSkinViewState>();
        state->addPointDataKey("point_key");

        OnionSkinViewPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        // Verify key is in state
        auto keys = state->getPointDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "point_key");

        // Delete the data
        dm->deleteData("point_key");
        QApplication::processEvents();

        // Key should be removed from state
        keys = state->getPointDataKeys();
        REQUIRE(keys.empty());
    }

    SECTION("line key removed from state when data deleted")
    {
        auto dm = createDataManager();
        addLineData(*dm, "line_key");

        auto state = std::make_shared<OnionSkinViewState>();
        state->addLineDataKey("line_key");

        OnionSkinViewPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        REQUIRE(state->getLineDataKeys().size() == 1);

        // Delete the data
        dm->deleteData("line_key");
        QApplication::processEvents();

        REQUIRE(state->getLineDataKeys().empty());
    }

    SECTION("mask key removed from state when data deleted")
    {
        auto dm = createDataManager();
        addMaskData(*dm, "mask_key");

        auto state = std::make_shared<OnionSkinViewState>();
        state->addMaskDataKey("mask_key");

        OnionSkinViewPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        REQUIRE(state->getMaskDataKeys().size() == 1);

        // Delete the data
        dm->deleteData("mask_key");
        QApplication::processEvents();

        REQUIRE(state->getMaskDataKeys().empty());
    }

    SECTION("other keys preserved when one deleted")
    {
        auto dm = createDataManager();
        addPointData(*dm, "point_1");
        addPointData(*dm, "point_2");
        addLineData(*dm, "line_1");

        auto state = std::make_shared<OnionSkinViewState>();
        state->addPointDataKey("point_1");
        state->addPointDataKey("point_2");
        state->addLineDataKey("line_1");

        OnionSkinViewPropertiesWidget widget(state, dm);
        QApplication::processEvents();

        // Delete only point_1
        dm->deleteData("point_1");
        QApplication::processEvents();

        auto point_keys = state->getPointDataKeys();
        REQUIRE(point_keys.size() == 1);
        REQUIRE(point_keys[0] == "point_2");

        // Line key should be unaffected
        auto line_keys = state->getLineDataKeys();
        REQUIRE(line_keys.size() == 1);
        REQUIRE(line_keys[0] == "line_1");
    }
}

TEST_CASE("OnionSkinViewPropertiesWidget construction", "[OnionSkinViewPropertiesWidget]")
{
    int argc = 0;
    QApplication app(argc, nullptr);

    SECTION("widget constructed with null DataManager does not crash")
    {
        auto state = std::make_shared<OnionSkinViewState>();
        OnionSkinViewPropertiesWidget widget(state, nullptr);

        auto * point_combo = findPointCombo(&widget);
        REQUIRE(point_combo != nullptr);
        REQUIRE(point_combo->count() == 0);
    }
}
