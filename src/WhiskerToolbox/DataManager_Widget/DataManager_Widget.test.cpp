#include "DataManager_Widget.hpp"
#include "DataManager.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <QApplication>
#include <memory>

// Simple test fixture for Qt widget testing
class DataManagerWidgetTestFixture {
public:
    DataManagerWidgetTestFixture() {
        // Ensure QApplication exists for widget testing
        if (!QApplication::instance()) {
            int argc = 0;
            char* argv[] = {nullptr};
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        data_manager = std::make_shared<DataManager>();
        time_scrollbar = new TimeScrollBar(nullptr);
        widget = std::make_unique<DataManager_Widget>(data_manager, time_scrollbar);
    }
    
    ~DataManagerWidgetTestFixture() {
        delete time_scrollbar;
    }
    
    std::unique_ptr<QApplication> app;
    std::shared_ptr<DataManager> data_manager;
    TimeScrollBar* time_scrollbar;
    std::unique_ptr<DataManager_Widget> widget;
};

TEST_CASE("DataManager_Widget - Basic construction and public interface", "[DataManager_Widget][construction]") {
    DataManagerWidgetTestFixture fixture;
    
    SECTION("Widget constructs successfully with valid parameters") {
        // Widget should construct without throwing
        REQUIRE(fixture.widget != nullptr);
        REQUIRE(fixture.data_manager != nullptr);
        REQUIRE(fixture.time_scrollbar != nullptr);
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