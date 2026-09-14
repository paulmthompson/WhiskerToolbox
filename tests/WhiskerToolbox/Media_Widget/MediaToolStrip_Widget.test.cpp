#include <catch2/catch_test_macros.hpp>

#include "Media_Widget/UI/Tools/MediaToolStrip_Widget.hpp"

#include <QApplication>

#include <array>

namespace {

void ensureQtApplication() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
}

}// namespace

TEST_CASE("MediaToolStrip_Widget selects Select tool by default", "[MediaToolStrip]") {
    ensureQtApplication();

    MediaToolStrip_Widget strip;
    REQUIRE(strip.activeTool() == MediaToolId::Select);
}

TEST_CASE("MediaToolStrip_Widget emits activeToolChanged when selection changes", "[MediaToolStrip]") {
    ensureQtApplication();

    MediaToolStrip_Widget strip;
    int change_count = 0;
    MediaToolId last_tool = MediaToolId::Select;

    QObject::connect(&strip, &MediaToolStrip_Widget::activeToolChanged, [&](MediaToolId tool) {
        ++change_count;
        last_tool = tool;
    });

    strip.setActiveTool(MediaToolId::Select);
    REQUIRE(change_count == 0);

    strip.setActiveTool(MediaToolId::Select);
    REQUIRE(change_count == 0);
    REQUIRE(last_tool == MediaToolId::Select);
}

TEST_CASE("MediaToolStrip_Widget deactivates Select when clicked again", "[MediaToolStrip]") {
    ensureQtApplication();

    MediaToolStrip_Widget strip;
    REQUIRE(strip.activeTool() == MediaToolId::Select);

    strip.setActiveTool(MediaToolId::None);
    REQUIRE(strip.activeTool() == MediaToolId::None);

    strip.setActiveTool(MediaToolId::Select);
    REQUIRE(strip.activeTool() == MediaToolId::Select);
}

TEST_CASE("MediaToolStrip_Widget selects Pen tool", "[MediaToolStrip]") {
    ensureQtApplication();

    MediaToolStrip_Widget strip;
    strip.setActiveTool(MediaToolId::Pen);
    REQUIRE(strip.activeTool() == MediaToolId::Pen);
}
