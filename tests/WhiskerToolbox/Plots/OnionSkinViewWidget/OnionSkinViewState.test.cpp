/**
 * @file OnionSkinViewState.test.cpp
 * @brief Unit tests for OnionSkinViewState
 *
 * Tests the EditorState subclass for OnionSkinViewWidget, including:
 * - Typed accessors for all state properties
 * - Signal emission verification
 * - JSON serialization/deserialization round-trip
 * - Data key management (point, line, mask)
 * - Temporal window parameters (behind, ahead)
 * - Alpha curve settings (curve type, min/max alpha)
 * - Rendering parameter updates (point size, line width, highlight)
 */

#include "Plots/OnionSkinViewWidget/Core/OnionSkinViewState.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <QSignalSpy>

// ==================== Construction Tests ====================

TEST_CASE("OnionSkinViewState construction", "[OnionSkinViewState]")
{
    SECTION("default construction creates valid state") {
        OnionSkinViewState state;

        REQUIRE(state.getTypeName() == "OnionSkinViewWidget");
        REQUIRE(state.getDisplayName() == "Onion Skin View");
        REQUIRE_FALSE(state.getInstanceId().isEmpty());
        REQUIRE_FALSE(state.isDirty());
    }

    SECTION("instance IDs are unique") {
        OnionSkinViewState state1;
        OnionSkinViewState state2;

        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }

    SECTION("default values are initialized") {
        OnionSkinViewState state;

        REQUIRE(state.getPointDataKeys().empty());
        REQUIRE(state.getLineDataKeys().empty());
        REQUIRE(state.getMaskDataKeys().empty());
        REQUIRE(state.getWindowBehind() == 5);
        REQUIRE(state.getWindowAhead() == 5);
        REQUIRE(state.getAlphaCurve() == "linear");
        REQUIRE(state.getMinAlpha() == 0.1f);
        REQUIRE(state.getMaxAlpha() == 1.0f);
        REQUIRE(state.getHighlightCurrent() == true);
    }
}

// ==================== Display Name Tests ====================

TEST_CASE("OnionSkinViewState display name", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("setDisplayName changes name") {
        state.setDisplayName("My Onion Skin");
        REQUIRE(state.getDisplayName() == "My Onion Skin");
    }

    SECTION("setDisplayName emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::displayNameChanged);

        state.setDisplayName("New Name");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "New Name");
    }

    SECTION("setDisplayName marks dirty") {
        state.markClean();
        REQUIRE_FALSE(state.isDirty());

        state.setDisplayName("Changed");
        REQUIRE(state.isDirty());
    }

    SECTION("setting same name does not emit signal") {
        state.setDisplayName("Test");
        QSignalSpy spy(&state, &OnionSkinViewState::displayNameChanged);

        state.setDisplayName("Test");

        REQUIRE(spy.count() == 0);
    }
}

// ==================== Point Data Key Tests ====================

TEST_CASE("OnionSkinViewState point data keys", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("addPointDataKey adds key") {
        state.addPointDataKey("points_1");

        auto keys = state.getPointDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "points_1");
    }

    SECTION("addPointDataKey emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeyAdded);

        state.addPointDataKey("pts");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "pts");
    }

    SECTION("addPointDataKey rejects duplicates") {
        state.addPointDataKey("pts");
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeyAdded);

        state.addPointDataKey("pts");

        REQUIRE(spy.count() == 0);
        REQUIRE(state.getPointDataKeys().size() == 1);
    }

    SECTION("removePointDataKey removes key") {
        state.addPointDataKey("pts_a");
        state.addPointDataKey("pts_b");

        state.removePointDataKey("pts_a");

        auto keys = state.getPointDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "pts_b");
    }

    SECTION("removePointDataKey emits signal") {
        state.addPointDataKey("pts");
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeyRemoved);

        state.removePointDataKey("pts");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "pts");
    }

    SECTION("removePointDataKey no-op for missing key") {
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeyRemoved);

        state.removePointDataKey("nonexistent");

        REQUIRE(spy.count() == 0);
    }

    SECTION("clearPointDataKeys clears all") {
        state.addPointDataKey("a");
        state.addPointDataKey("b");
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeysCleared);

        state.clearPointDataKeys();

        REQUIRE(state.getPointDataKeys().empty());
        REQUIRE(spy.count() == 1);
    }

    SECTION("clearPointDataKeys no-op when already empty") {
        QSignalSpy spy(&state, &OnionSkinViewState::pointDataKeysCleared);

        state.clearPointDataKeys();

        REQUIRE(spy.count() == 0);
    }

    SECTION("addPointDataKey marks dirty") {
        state.markClean();
        state.addPointDataKey("pts");
        REQUIRE(state.isDirty());
    }
}

// ==================== Line Data Key Tests ====================

TEST_CASE("OnionSkinViewState line data keys", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("addLineDataKey adds key") {
        state.addLineDataKey("lines_1");

        auto keys = state.getLineDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "lines_1");
    }

    SECTION("addLineDataKey emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::lineDataKeyAdded);

        state.addLineDataKey("lns");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "lns");
    }

    SECTION("addLineDataKey rejects duplicates") {
        state.addLineDataKey("lns");
        QSignalSpy spy(&state, &OnionSkinViewState::lineDataKeyAdded);

        state.addLineDataKey("lns");

        REQUIRE(spy.count() == 0);
    }

    SECTION("removeLineDataKey removes key") {
        state.addLineDataKey("lns_a");
        state.addLineDataKey("lns_b");

        state.removeLineDataKey("lns_a");

        auto keys = state.getLineDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "lns_b");
    }

    SECTION("removeLineDataKey emits signal") {
        state.addLineDataKey("lns");
        QSignalSpy spy(&state, &OnionSkinViewState::lineDataKeyRemoved);

        state.removeLineDataKey("lns");

        REQUIRE(spy.count() == 1);
    }

    SECTION("clearLineDataKeys clears all") {
        state.addLineDataKey("a");
        state.addLineDataKey("b");
        QSignalSpy spy(&state, &OnionSkinViewState::lineDataKeysCleared);

        state.clearLineDataKeys();

        REQUIRE(state.getLineDataKeys().empty());
        REQUIRE(spy.count() == 1);
    }
}

// ==================== Per-Key Line Style Tests ====================

TEST_CASE("OnionSkinViewState per-key line style", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("addLineDataKey creates LineStyleState") {
        state.addLineDataKey("lines_1");

        LineStyleState * line_state = state.lineStyleStateForKey("lines_1");
        REQUIRE(line_state != nullptr);
    }

    SECTION("lineStyleStateForKey returns nullptr for unknown key") {
        REQUIRE(state.lineStyleStateForKey("nonexistent") == nullptr);
    }

    SECTION("default line style has expected values") {
        state.addLineDataKey("lines_1");

        auto style = state.getLineKeyLineStyle("lines_1");
        REQUIRE(style.hex_color == "#46b346");
        REQUIRE(style.thickness == 2.0f);
        REQUIRE(style.alpha == 1.0f);
    }

    SECTION("getLineKeyLineStyle returns default for unknown key") {
        auto style = state.getLineKeyLineStyle("nonexistent");
        REQUIRE(style.hex_color == "#46b346");
        REQUIRE(style.thickness == 2.0f);
        REQUIRE(style.alpha == 1.0f);
    }

    SECTION("per-key line style change emits lineStyleChanged") {
        state.addLineDataKey("lines_1");
        QSignalSpy spy(&state, &OnionSkinViewState::lineStyleChanged);

        LineStyleState * line_state = state.lineStyleStateForKey("lines_1");
        line_state->setThickness(4.0f);

        REQUIRE(spy.count() == 1);
    }

    SECTION("per-key line style change emits lineKeyLineStyleChanged with key") {
        state.addLineDataKey("lines_1");
        QSignalSpy spy(&state, &OnionSkinViewState::lineKeyLineStyleChanged);

        LineStyleState * line_state = state.lineStyleStateForKey("lines_1");
        line_state->setHexColor("#ff0000");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "lines_1");
    }

    SECTION("per-key line style change marks dirty") {
        state.addLineDataKey("lines_1");
        state.markClean();

        LineStyleState * line_state = state.lineStyleStateForKey("lines_1");
        line_state->setAlpha(0.5f);

        REQUIRE(state.isDirty());
    }

    SECTION("per-key line style syncs to serializable data") {
        state.addLineDataKey("lines_1");

        LineStyleState * line_state = state.lineStyleStateForKey("lines_1");
        line_state->setHexColor("#ff0000");
        line_state->setThickness(5.0f);
        line_state->setAlpha(0.7f);

        auto style = state.getLineKeyLineStyle("lines_1");
        REQUIRE(style.hex_color == "#ff0000");
        REQUIRE(style.thickness == 5.0f);
        REQUIRE(style.alpha == 0.7f);
    }

    SECTION("per-key line style removed when key is removed") {
        state.addLineDataKey("lines_1");
        REQUIRE(state.lineStyleStateForKey("lines_1") != nullptr);

        state.removeLineDataKey("lines_1");
        REQUIRE(state.lineStyleStateForKey("lines_1") == nullptr);
    }

    SECTION("per-key line styles cleared when clearLineDataKeys is called") {
        state.addLineDataKey("a");
        state.addLineDataKey("b");

        state.clearLineDataKeys();

        REQUIRE(state.lineStyleStateForKey("a") == nullptr);
        REQUIRE(state.lineStyleStateForKey("b") == nullptr);
    }

    SECTION("multiple line keys have independent styles") {
        state.addLineDataKey("lines_a");
        state.addLineDataKey("lines_b");

        LineStyleState * style_a = state.lineStyleStateForKey("lines_a");
        LineStyleState * style_b = state.lineStyleStateForKey("lines_b");
        REQUIRE(style_a != nullptr);
        REQUIRE(style_b != nullptr);
        REQUIRE(style_a != style_b);

        style_a->setHexColor("#ff0000");
        style_b->setHexColor("#00ff00");

        REQUIRE(state.getLineKeyLineStyle("lines_a").hex_color == "#ff0000");
        REQUIRE(state.getLineKeyLineStyle("lines_b").hex_color == "#00ff00");
    }
}

// ==================== Mask Data Key Tests ====================

TEST_CASE("OnionSkinViewState mask data keys", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("addMaskDataKey adds key") {
        state.addMaskDataKey("masks_1");

        auto keys = state.getMaskDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "masks_1");
    }

    SECTION("addMaskDataKey emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::maskDataKeyAdded);

        state.addMaskDataKey("msk");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "msk");
    }

    SECTION("addMaskDataKey rejects duplicates") {
        state.addMaskDataKey("msk");
        QSignalSpy spy(&state, &OnionSkinViewState::maskDataKeyAdded);

        state.addMaskDataKey("msk");

        REQUIRE(spy.count() == 0);
    }

    SECTION("removeMaskDataKey removes key") {
        state.addMaskDataKey("msk_a");
        state.addMaskDataKey("msk_b");

        state.removeMaskDataKey("msk_a");

        auto keys = state.getMaskDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "msk_b");
    }

    SECTION("removeMaskDataKey emits signal") {
        state.addMaskDataKey("msk");
        QSignalSpy spy(&state, &OnionSkinViewState::maskDataKeyRemoved);

        state.removeMaskDataKey("msk");

        REQUIRE(spy.count() == 1);
    }

    SECTION("clearMaskDataKeys clears all") {
        state.addMaskDataKey("a");
        state.addMaskDataKey("b");
        QSignalSpy spy(&state, &OnionSkinViewState::maskDataKeysCleared);

        state.clearMaskDataKeys();

        REQUIRE(state.getMaskDataKeys().empty());
        REQUIRE(spy.count() == 1);
    }

    SECTION("clearMaskDataKeys no-op when empty") {
        QSignalSpy spy(&state, &OnionSkinViewState::maskDataKeysCleared);

        state.clearMaskDataKeys();

        REQUIRE(spy.count() == 0);
    }
}

// ==================== Temporal Window Tests ====================

TEST_CASE("OnionSkinViewState temporal window parameters", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("setWindowBehind changes value") {
        state.setWindowBehind(10);
        REQUIRE(state.getWindowBehind() == 10);
    }

    SECTION("setWindowBehind emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::windowBehindChanged);

        state.setWindowBehind(3);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toInt() == 3);
    }

    SECTION("setWindowBehind marks dirty") {
        state.markClean();
        state.setWindowBehind(7);
        REQUIRE(state.isDirty());
    }

    SECTION("setWindowBehind same value no-op") {
        state.setWindowBehind(5); // default
        state.markClean();
        QSignalSpy spy(&state, &OnionSkinViewState::windowBehindChanged);

        state.setWindowBehind(5);

        REQUIRE(spy.count() == 0);
        REQUIRE_FALSE(state.isDirty());
    }

    SECTION("setWindowBehind clamps negative to zero") {
        state.setWindowBehind(-3);
        REQUIRE(state.getWindowBehind() == 0);
    }

    SECTION("setWindowAhead changes value") {
        state.setWindowAhead(15);
        REQUIRE(state.getWindowAhead() == 15);
    }

    SECTION("setWindowAhead emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::windowAheadChanged);

        state.setWindowAhead(8);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toInt() == 8);
    }

    SECTION("setWindowAhead clamps negative to zero") {
        state.setWindowAhead(-1);
        REQUIRE(state.getWindowAhead() == 0);
    }
}

// ==================== Alpha Curve Tests ====================

TEST_CASE("OnionSkinViewState alpha curve settings", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("setAlphaCurve changes value") {
        state.setAlphaCurve("exponential");
        REQUIRE(state.getAlphaCurve() == "exponential");
    }

    SECTION("setAlphaCurve emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::alphaCurveChanged);

        state.setAlphaCurve("gaussian");

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "gaussian");
    }

    SECTION("setAlphaCurve marks dirty") {
        state.markClean();
        state.setAlphaCurve("exponential");
        REQUIRE(state.isDirty());
    }

    SECTION("setAlphaCurve same value no-op") {
        QSignalSpy spy(&state, &OnionSkinViewState::alphaCurveChanged);

        state.setAlphaCurve("linear"); // default value

        REQUIRE(spy.count() == 0);
    }

    SECTION("setMinAlpha changes value") {
        state.setMinAlpha(0.3f);
        REQUIRE(state.getMinAlpha() == 0.3f);
    }

    SECTION("setMinAlpha emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::minAlphaChanged);

        state.setMinAlpha(0.5f);

        REQUIRE(spy.count() == 1);
    }

    SECTION("setMinAlpha clamps to [0, 1]") {
        state.setMinAlpha(-0.5f);
        REQUIRE(state.getMinAlpha() == 0.0f);

        state.setMinAlpha(1.5f);
        REQUIRE(state.getMaxAlpha() == 1.0f);
    }

    SECTION("setMaxAlpha changes value") {
        state.setMaxAlpha(0.8f);
        REQUIRE(state.getMaxAlpha() == 0.8f);
    }

    SECTION("setMaxAlpha emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::maxAlphaChanged);

        state.setMaxAlpha(0.7f);

        REQUIRE(spy.count() == 1);
    }

    SECTION("setMaxAlpha clamps to [0, 1]") {
        state.setMaxAlpha(-0.1f);
        REQUIRE(state.getMaxAlpha() == 0.0f);

        state.setMaxAlpha(2.0f);
        REQUIRE(state.getMaxAlpha() == 1.0f);
    }
}

// ==================== Rendering Parameter Tests ====================

TEST_CASE("OnionSkinViewState rendering parameters", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("per-key glyph style state is created when key is added") {
        state.addPointDataKey("pts");
        GlyphStyleState * glyph_state = state.glyphStyleStateForKey("pts");
        REQUIRE(glyph_state != nullptr);
        // Default size
        REQUIRE(glyph_state->size() == 8.0f);
    }

    SECTION("glyphStyleStateForKey returns nullptr for unknown key") {
        REQUIRE(state.glyphStyleStateForKey("nonexistent") == nullptr);
    }

    SECTION("getPointKeyGlyphStyle returns default for unknown key") {
        auto style = state.getPointKeyGlyphStyle("nonexistent");
        REQUIRE(style.size == 8.0f);
    }

    SECTION("per-key glyph style change emits glyphStyleChanged") {
        state.addPointDataKey("pts");
        QSignalSpy spy(&state, &OnionSkinViewState::glyphStyleChanged);

        GlyphStyleState * glyph_state = state.glyphStyleStateForKey("pts");
        REQUIRE(glyph_state != nullptr);
        glyph_state->setSize(12.0f);

        REQUIRE(spy.count() == 1);
    }

    SECTION("per-key glyph style change emits pointKeyGlyphStyleChanged") {
        state.addPointDataKey("pts");
        QSignalSpy spy(&state, &OnionSkinViewState::pointKeyGlyphStyleChanged);

        GlyphStyleState * glyph_state = state.glyphStyleStateForKey("pts");
        REQUIRE(glyph_state != nullptr);
        glyph_state->setSize(15.0f);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "pts");
    }

    SECTION("per-key glyph style removed when key is removed") {
        state.addPointDataKey("pts");
        REQUIRE(state.glyphStyleStateForKey("pts") != nullptr);

        state.removePointDataKey("pts");
        REQUIRE(state.glyphStyleStateForKey("pts") == nullptr);
    }

    SECTION("per-key glyph styles cleared when clearPointDataKeys is called") {
        state.addPointDataKey("a");
        state.addPointDataKey("b");

        state.clearPointDataKeys();

        REQUIRE(state.glyphStyleStateForKey("a") == nullptr);
        REQUIRE(state.glyphStyleStateForKey("b") == nullptr);
    }

    SECTION("per-key glyph style change marks dirty") {
        state.addPointDataKey("pts");
        state.markClean();

        GlyphStyleState * glyph_state = state.glyphStyleStateForKey("pts");
        glyph_state->setSize(14.0f);

        REQUIRE(state.isDirty());
    }

    SECTION("setHighlightCurrent changes value") {
        state.setHighlightCurrent(false);
        REQUIRE(state.getHighlightCurrent() == false);
    }

    SECTION("setHighlightCurrent emits signal") {
        QSignalSpy spy(&state, &OnionSkinViewState::highlightCurrentChanged);

        state.setHighlightCurrent(false);

        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toBool() == false);
    }

    SECTION("setHighlightCurrent same value no-op") {
        QSignalSpy spy(&state, &OnionSkinViewState::highlightCurrentChanged);

        state.setHighlightCurrent(true); // default

        REQUIRE(spy.count() == 0);
    }
}

// ==================== View State Tests ====================

TEST_CASE("OnionSkinViewState view state", "[OnionSkinViewState]")
{
    OnionSkinViewState state;

    SECTION("setXZoom changes value") {
        QSignalSpy spy(&state, &OnionSkinViewState::viewStateChanged);
        state.setXZoom(2.0);
        REQUIRE(state.viewState().x_zoom == 2.0);
        REQUIRE(spy.count() == 1);
    }

    SECTION("setYZoom changes value") {
        QSignalSpy spy(&state, &OnionSkinViewState::viewStateChanged);
        state.setYZoom(3.0);
        REQUIRE(state.viewState().y_zoom == 3.0);
        REQUIRE(spy.count() == 1);
    }

    SECTION("setPan changes value") {
        QSignalSpy spy(&state, &OnionSkinViewState::viewStateChanged);
        state.setPan(10.0, 20.0);
        REQUIRE(state.viewState().x_pan == 10.0);
        REQUIRE(state.viewState().y_pan == 20.0);
        REQUIRE(spy.count() == 1);
    }

    SECTION("setXBounds updates axis state") {
        state.setXBounds(0.0, 640.0);
        REQUIRE(state.viewState().x_min == 0.0);
        REQUIRE(state.viewState().x_max == 640.0);
        REQUIRE(state.horizontalAxisState()->getXMin() == 0.0);
        REQUIRE(state.horizontalAxisState()->getXMax() == 640.0);
    }

    SECTION("setYBounds updates axis state") {
        state.setYBounds(0.0, 480.0);
        REQUIRE(state.viewState().y_min == 0.0);
        REQUIRE(state.viewState().y_max == 480.0);
    }
}

// ==================== Serialization Tests ====================

TEST_CASE("OnionSkinViewState serialization", "[OnionSkinViewState]")
{
    SECTION("round-trip preserves data keys") {
        OnionSkinViewState state;
        state.addPointDataKey("whisker_tips");
        state.addLineDataKey("whisker_traces");
        state.addMaskDataKey("face_mask");

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));

        auto pt_keys = restored.getPointDataKeys();
        REQUIRE(pt_keys.size() == 1);
        REQUIRE(pt_keys[0] == "whisker_tips");

        auto ln_keys = restored.getLineDataKeys();
        REQUIRE(ln_keys.size() == 1);
        REQUIRE(ln_keys[0] == "whisker_traces");

        auto mk_keys = restored.getMaskDataKeys();
        REQUIRE(mk_keys.size() == 1);
        REQUIRE(mk_keys[0] == "face_mask");
    }

    SECTION("round-trip preserves temporal window parameters") {
        OnionSkinViewState state;
        state.setWindowBehind(10);
        state.setWindowAhead(15);

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getWindowBehind() == 10);
        REQUIRE(restored.getWindowAhead() == 15);
    }

    SECTION("round-trip preserves alpha curve settings") {
        OnionSkinViewState state;
        state.setAlphaCurve("gaussian");
        state.setMinAlpha(0.2f);
        state.setMaxAlpha(0.9f);

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getAlphaCurve() == "gaussian");
        REQUIRE(restored.getMinAlpha() == 0.2f);
        REQUIRE(restored.getMaxAlpha() == 0.9f);
    }

    SECTION("round-trip preserves rendering parameters") {
        OnionSkinViewState state;
        state.addPointDataKey("pts");
        state.glyphStyleStateForKey("pts")->setSize(12.0f);
        state.setHighlightCurrent(false);

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        // Per-key glyph style should be restored
        REQUIRE(restored.getPointKeyGlyphStyle("pts").size == 12.0f);
        REQUIRE(restored.getHighlightCurrent() == false);
    }

    SECTION("round-trip preserves per-key line styles") {
        OnionSkinViewState state;
        state.addLineDataKey("whisker_traces");
        state.lineStyleStateForKey("whisker_traces")->setHexColor("#ff0000");
        state.lineStyleStateForKey("whisker_traces")->setThickness(4.5f);
        state.lineStyleStateForKey("whisker_traces")->setAlpha(0.8f);

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));

        auto style = restored.getLineKeyLineStyle("whisker_traces");
        REQUIRE(style.hex_color == "#ff0000");
        REQUIRE(style.thickness == 4.5f);
        REQUIRE(style.alpha == 0.8f);

        // Live state should also be available
        LineStyleState * line_state = restored.lineStyleStateForKey("whisker_traces");
        REQUIRE(line_state != nullptr);
        REQUIRE(line_state->hexColor() == "#ff0000");
        REQUIRE(line_state->thickness() == 4.5f);
        REQUIRE(line_state->alpha() == 0.8f);
    }

    SECTION("round-trip preserves display name") {
        OnionSkinViewState state;
        state.setDisplayName("Custom Onion Skin");

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getDisplayName() == "Custom Onion Skin");
    }

    SECTION("round-trip preserves instance ID") {
        OnionSkinViewState state;
        QString original_id = state.getInstanceId();

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getInstanceId() == original_id);
    }

    SECTION("round-trip preserves view state") {
        OnionSkinViewState state;
        state.setXBounds(0.0, 640.0);
        state.setYBounds(0.0, 480.0);
        state.setXZoom(2.0);
        state.setYZoom(1.5);
        state.setPan(10.0, 20.0);

        std::string json = state.toJson();

        OnionSkinViewState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.viewState().x_zoom == 2.0);
        REQUIRE(restored.viewState().y_zoom == 1.5);
        REQUIRE(restored.viewState().x_pan == 10.0);
        REQUIRE(restored.viewState().y_pan == 20.0);
    }

    SECTION("invalid JSON returns false") {
        OnionSkinViewState state;
        REQUIRE_FALSE(state.fromJson("{not valid json}"));
    }

    SECTION("empty JSON returns false") {
        OnionSkinViewState state;
        REQUIRE_FALSE(state.fromJson(""));
    }
}
