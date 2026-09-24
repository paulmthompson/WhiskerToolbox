/**
 * @file Media_KeymapActions.test.cpp
 * @brief Unit tests for Media Viewer keymap dispatch helpers
 */

#include <catch2/catch_test_macros.hpp>

#include "Core/MediaWidgetState.hpp"
#include "Core/Media_KeymapActions.hpp"
#include "UI/Tools/MediaToolId.hpp"

#include <QString>

TEST_CASE("cycleLineAppendEndpoint advances Tip Base Nearest Tip", "[MediaKeymapActions]") {
    CHECK(cycleLineAppendEndpoint(LineAppendEndpoint::Tip) == LineAppendEndpoint::Base);
    CHECK(cycleLineAppendEndpoint(LineAppendEndpoint::Base) == LineAppendEndpoint::Nearest);
    CHECK(cycleLineAppendEndpoint(LineAppendEndpoint::Nearest) == LineAppendEndpoint::Tip);
}

TEST_CASE("handleMediaKeyAction switches active media tool", "[MediaKeymapActions]") {
    MediaWidgetState state;
    MediaKeymapContext ctx;
    ctx.state = &state;

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.tool.pen"), ctx));
    CHECK(state.activeMediaTool() == MediaToolId::Pen);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.tool.select"), ctx));
    CHECK(state.activeMediaTool() == MediaToolId::Select);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.tool.eraser"), ctx));
    CHECK(state.activeMediaTool() == MediaToolId::Eraser);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.tool.smooth"), ctx));
    CHECK(state.activeMediaTool() == MediaToolId::Smooth);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.tool.none"), ctx));
    CHECK(state.activeMediaTool() == MediaToolId::None);
}

TEST_CASE("handleMediaKeyAction cycles pen append endpoint only when Pen is active",
          "[MediaKeymapActions]") {
    MediaWidgetState state;
    MediaKeymapContext ctx;
    ctx.state = &state;

    CHECK_FALSE(handleMediaKeyAction(QStringLiteral("media.pen.cycle_append_endpoint"), ctx));
    CHECK(state.linePrefs().append_endpoint == LineAppendEndpoint::Tip);

    state.setActiveMediaTool(MediaToolId::Pen);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.pen.cycle_append_endpoint"), ctx));
    CHECK(state.linePrefs().append_endpoint == LineAppendEndpoint::Base);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.pen.cycle_append_endpoint"), ctx));
    CHECK(state.linePrefs().append_endpoint == LineAppendEndpoint::Nearest);

    REQUIRE(handleMediaKeyAction(QStringLiteral("media.pen.cycle_append_endpoint"), ctx));
    CHECK(state.linePrefs().append_endpoint == LineAppendEndpoint::Tip);
}

TEST_CASE("handleMediaKeyAction ignores unknown actions", "[MediaKeymapActions]") {
    MediaWidgetState state;
    MediaKeymapContext ctx;
    ctx.state = &state;

    CHECK_FALSE(handleMediaKeyAction(QStringLiteral("media.unknown_action"), ctx));
}
