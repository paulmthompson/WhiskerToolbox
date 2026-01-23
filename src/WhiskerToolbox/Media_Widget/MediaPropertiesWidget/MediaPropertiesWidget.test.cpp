#include "MediaPropertiesWidget/MediaPropertiesWidget.hpp"
#include "MediaWidgetState.hpp"
#include "DataManager/DataManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QLabel>
#include <QScrollArea>

#include <array>
#include <memory>

// === MediaPropertiesWidget Tests ===

TEST_CASE("MediaPropertiesWidget construction", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists (same pattern as Media_Widget.test.cpp)
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    auto * app = QApplication::instance();
    REQUIRE(app != nullptr);

    SECTION("Constructs with state and data manager") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }

    SECTION("Constructs with nullptr state") {
        auto dm = std::make_shared<DataManager>();

        // Should not crash with nullptr state
        MediaPropertiesWidget widget(nullptr, dm);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }

    SECTION("Constructs with nullptr data manager") {
        auto state = std::make_shared<MediaWidgetState>();

        // Should not crash with nullptr data manager
        MediaPropertiesWidget widget(state, nullptr);

        REQUIRE(widget.getMediaWindow() == nullptr);
    }
}

TEST_CASE("MediaPropertiesWidget setMediaWindow", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    SECTION("Can set and get Media_Window") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Initially nullptr
        REQUIRE(widget.getMediaWindow() == nullptr);

        // Note: We can't easily create a real Media_Window in tests
        // but we can verify the setter/getter work with nullptr
        widget.setMediaWindow(nullptr);
        REQUIRE(widget.getMediaWindow() == nullptr);
    }
}

TEST_CASE("MediaPropertiesWidget has expected UI", "[MediaPropertiesWidget]") {
    // Ensure a Qt application exists
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }

    SECTION("Contains scroll area") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the scroll area
        auto * scrollArea = widget.findChild<QScrollArea *>("scrollArea");
        REQUIRE(scrollArea != nullptr);
        REQUIRE(scrollArea->widgetResizable() == true);
    }

    SECTION("Contains placeholder label") {
        auto state = std::make_shared<MediaWidgetState>();
        auto dm = std::make_shared<DataManager>();

        MediaPropertiesWidget widget(state, dm);

        // Find the placeholder label
        auto * label = widget.findChild<QLabel *>("placeholderLabel");
        REQUIRE(label != nullptr);
        REQUIRE(label->text().contains("Media Properties"));
    }
}
