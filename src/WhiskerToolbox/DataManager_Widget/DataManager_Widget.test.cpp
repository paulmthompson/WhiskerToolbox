#include "DataManager_Widget.hpp"
#include "DataManager.hpp"
#include "NewDataWidget/NewDataWidget.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QApplication>
#include <QComboBox>
#include <memory>

// Simple test fixture for Qt widget testing
class DataManagerWidgetTestFixture {
public:
    DataManagerWidgetTestFixture() {
        // Ensure QApplication exists for widget testing
        if (!QApplication::instance()) {
            int argc = 0;
            char * argv[] = {nullptr};
            app = std::make_unique<QApplication>(argc, argv);
        }

        data_manager = std::make_shared<DataManager>();
        widget = std::make_unique<DataManager_Widget>(data_manager);
    }

    ~DataManagerWidgetTestFixture() = default;

    std::unique_ptr<QApplication> app;
    std::shared_ptr<DataManager> data_manager;
    std::unique_ptr<DataManager_Widget> widget;
};

TEST_CASE("DataManager_Widget - Basic construction and public interface", "[DataManager_Widget][construction]") {
    DataManagerWidgetTestFixture fixture;

    SECTION("Widget constructs successfully with valid parameters") {
        // Widget should construct without throwing
        REQUIRE(fixture.widget != nullptr);
        REQUIRE(fixture.data_manager != nullptr);
    }

    SECTION("openWidget method executes without error") {
        // This mainly tests that the method doesn't crash
        REQUIRE_NOTHROW(fixture.widget->openWidget());
    }

    SECTION("clearFeatureSelection method executes without error") {
        // This tests that the public clearFeatureSelection method works
        REQUIRE_NOTHROW(fixture.widget->clearFeatureSelection());
    }
}

TEST_CASE("DataManager_Widget - Feature selection interface", "[DataManager_Widget][feature_selection]") {
    DataManagerWidgetTestFixture fixture;

    SECTION("Multiple calls to clearFeatureSelection are safe") {
        // Multiple calls should not cause issues
        REQUIRE_NOTHROW(fixture.widget->clearFeatureSelection());
        REQUIRE_NOTHROW(fixture.widget->clearFeatureSelection());
        REQUIRE_NOTHROW(fixture.widget->clearFeatureSelection());
    }

    SECTION("Widget methods handle empty data manager gracefully") {
        // Test that the widget works with an empty data manager
        REQUIRE_NOTHROW(fixture.widget->openWidget());
        REQUIRE_NOTHROW(fixture.widget->clearFeatureSelection());
    }
}

TEST_CASE("DataManager_Widget - NewDataWidget timeframe combobox sync", "[DataManager_Widget][timeframe_combo]") {
    DataManagerWidgetTestFixture fixture;

    auto * new_data_widget = fixture.widget->findChild<NewDataWidget *>("new_data_widget");
    REQUIRE(new_data_widget != nullptr);

    auto * timeframe_combo = new_data_widget->findChild<QComboBox *>("timeframe_combo");
    REQUIRE(timeframe_combo != nullptr);

    SECTION("Initial population shows default time clock") {
        REQUIRE(timeframe_combo->count() == 1);
        REQUIRE(timeframe_combo->findText("time") >= 0);
    }

    SECTION("Combobox refreshes when DataManager registers new timeframes") {
        REQUIRE(timeframe_combo->count() == 1);

        fixture.data_manager->setTime(TimeKey("master"), std::make_shared<TimeFrame>());

        REQUIRE(timeframe_combo->count() == 2);
        REQUIRE(timeframe_combo->findText("master") >= 0);
        REQUIRE(timeframe_combo->findText("time") >= 0);
    }
}