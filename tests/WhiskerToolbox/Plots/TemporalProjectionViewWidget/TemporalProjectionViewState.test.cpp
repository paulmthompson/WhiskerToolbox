/**
 * @file TemporalProjectionViewState.test.cpp
 * @brief Unit tests for TemporalProjectionViewState
 * 
 * Tests the EditorState subclass for TemporalProjectionViewWidget, including:
 * - Typed accessors for all state properties
 * - Signal emission verification
 * - JSON serialization/deserialization round-trip
 * - Data key management
 * - Rendering parameter updates
 * - Selection mode handling
 */

#include "Plots/TemporalProjectionViewWidget/Core/TemporalProjectionViewState.hpp"

#include <catch2/catch_test_macros.hpp>
#include <QSignalSpy>

// ==================== Construction Tests ====================

TEST_CASE("TemporalProjectionViewState construction", "[TemporalProjectionViewState]")
{
    SECTION("default construction creates valid state") {
        TemporalProjectionViewState state;
        
        REQUIRE(state.getTypeName() == "TemporalProjectionView");
        REQUIRE(state.getDisplayName() == "Spatial Overlay");
        REQUIRE_FALSE(state.getInstanceId().isEmpty());
        REQUIRE_FALSE(state.isDirty());
    }
    
    SECTION("instance IDs are unique") {
        TemporalProjectionViewState state1;
        TemporalProjectionViewState state2;
        
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }
    
    SECTION("default values are initialized") {
        TemporalProjectionViewState state;
        
        REQUIRE(state.getPointDataKeys().empty());
        REQUIRE(state.getLineDataKeys().empty());
        REQUIRE(state.getPointSize() == 5.0f);
        REQUIRE(state.getLineWidth() == 2.0f);
        REQUIRE(state.getSelectionMode() == "none");
    }
}

// ==================== Display Name Tests ====================

TEST_CASE("TemporalProjectionViewState display name", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("setDisplayName changes name") {
        state.setDisplayName("My Spatial View");
        REQUIRE(state.getDisplayName() == "My Spatial View");
    }
    
    SECTION("setDisplayName emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::displayNameChanged);
        
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
        QSignalSpy spy(&state, &TemporalProjectionViewState::displayNameChanged);
        
        state.setDisplayName("Test");
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Point Data Key Tests ====================

TEST_CASE("TemporalProjectionViewState point data keys", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("addPointDataKey adds key") {
        state.addPointDataKey("points_1");
        
        auto keys = state.getPointDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "points_1");
    }
    
    SECTION("addPointDataKey emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointDataKeyAdded);
        
        state.addPointDataKey("points_2");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "points_2");
    }
    
    SECTION("addPointDataKey marks dirty") {
        state.markClean();
        state.addPointDataKey("points_3");
        REQUIRE(state.isDirty());
    }
    
    SECTION("addPointDataKey does not add duplicates") {
        state.addPointDataKey("points_dup");
        state.addPointDataKey("points_dup");
        
        auto keys = state.getPointDataKeys();
        REQUIRE(keys.size() == 1);
    }
    
    SECTION("adding duplicate does not emit signal") {
        state.addPointDataKey("points_dup");
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointDataKeyAdded);
        
        state.addPointDataKey("points_dup");
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("removePointDataKey removes key") {
        state.addPointDataKey("points_rem");
        state.removePointDataKey("points_rem");
        
        auto keys = state.getPointDataKeys();
        REQUIRE(keys.empty());
    }
    
    SECTION("removePointDataKey emits signal") {
        state.addPointDataKey("points_rem2");
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointDataKeyRemoved);
        
        state.removePointDataKey("points_rem2");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "points_rem2");
    }
    
    SECTION("removePointDataKey marks dirty") {
        state.addPointDataKey("points_rem3");
        state.markClean();
        state.removePointDataKey("points_rem3");
        REQUIRE(state.isDirty());
    }
    
    SECTION("clearPointDataKeys clears all keys") {
        state.addPointDataKey("p1");
        state.addPointDataKey("p2");
        state.addPointDataKey("p3");
        
        state.clearPointDataKeys();
        
        REQUIRE(state.getPointDataKeys().empty());
    }
    
    SECTION("clearPointDataKeys emits signal") {
        state.addPointDataKey("p1");
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointDataKeysCleared);
        
        state.clearPointDataKeys();
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("clearPointDataKeys on empty list does not emit") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointDataKeysCleared);
        
        state.clearPointDataKeys();
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Line Data Key Tests ====================

TEST_CASE("TemporalProjectionViewState line data keys", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("addLineDataKey adds key") {
        state.addLineDataKey("lines_1");
        
        auto keys = state.getLineDataKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "lines_1");
    }
    
    SECTION("addLineDataKey emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::lineDataKeyAdded);
        
        state.addLineDataKey("lines_2");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "lines_2");
    }
    
    SECTION("addLineDataKey marks dirty") {
        state.markClean();
        state.addLineDataKey("lines_3");
        REQUIRE(state.isDirty());
    }
    
    SECTION("addLineDataKey does not add duplicates") {
        state.addLineDataKey("lines_dup");
        state.addLineDataKey("lines_dup");
        
        auto keys = state.getLineDataKeys();
        REQUIRE(keys.size() == 1);
    }
    
    SECTION("removeLineDataKey removes key") {
        state.addLineDataKey("lines_rem");
        state.removeLineDataKey("lines_rem");
        
        auto keys = state.getLineDataKeys();
        REQUIRE(keys.empty());
    }
    
    SECTION("removeLineDataKey emits signal") {
        state.addLineDataKey("lines_rem2");
        QSignalSpy spy(&state, &TemporalProjectionViewState::lineDataKeyRemoved);
        
        state.removeLineDataKey("lines_rem2");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "lines_rem2");
    }
    
    SECTION("clearLineDataKeys clears all keys") {
        state.addLineDataKey("l1");
        state.addLineDataKey("l2");
        state.addLineDataKey("l3");
        
        state.clearLineDataKeys();
        
        REQUIRE(state.getLineDataKeys().empty());
    }
    
    SECTION("clearLineDataKeys emits signal") {
        state.addLineDataKey("l1");
        QSignalSpy spy(&state, &TemporalProjectionViewState::lineDataKeysCleared);
        
        state.clearLineDataKeys();
        
        REQUIRE(spy.count() == 1);
    }
}

// ==================== Rendering Parameter Tests ====================

TEST_CASE("TemporalProjectionViewState rendering parameters", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("setPointSize changes value") {
        state.setPointSize(10.0f);
        REQUIRE(state.getPointSize() == 10.0f);
    }
    
    SECTION("setPointSize emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointSizeChanged);
        
        state.setPointSize(7.5f);
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toFloat() == 7.5f);
    }
    
    SECTION("setPointSize marks dirty") {
        state.markClean();
        state.setPointSize(3.0f);
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same point size does not emit signal") {
        state.setPointSize(5.0f);
        QSignalSpy spy(&state, &TemporalProjectionViewState::pointSizeChanged);
        
        state.setPointSize(5.0f);
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("setLineWidth changes value") {
        state.setLineWidth(4.0f);
        REQUIRE(state.getLineWidth() == 4.0f);
    }
    
    SECTION("setLineWidth emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::lineWidthChanged);
        
        state.setLineWidth(3.5f);
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toFloat() == 3.5f);
    }
    
    SECTION("setLineWidth marks dirty") {
        state.markClean();
        state.setLineWidth(1.5f);
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same line width does not emit signal") {
        state.setLineWidth(2.0f);
        QSignalSpy spy(&state, &TemporalProjectionViewState::lineWidthChanged);
        
        state.setLineWidth(2.0f);
        
        REQUIRE(spy.count() == 0);
    }
}

// ==================== Selection Mode Tests ====================

TEST_CASE("TemporalProjectionViewState selection mode", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("setSelectionMode changes value") {
        state.setSelectionMode("point");
        REQUIRE(state.getSelectionMode() == "point");
    }
    
    SECTION("setSelectionMode emits signal") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::selectionModeChanged);
        
        state.setSelectionMode("line");
        
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.takeFirst().at(0).toString() == "line");
    }
    
    SECTION("setSelectionMode marks dirty") {
        state.markClean();
        state.setSelectionMode("polygon");
        REQUIRE(state.isDirty());
    }
    
    SECTION("setting same mode does not emit signal") {
        state.setSelectionMode("none");
        QSignalSpy spy(&state, &TemporalProjectionViewState::selectionModeChanged);
        
        state.setSelectionMode("none");
        
        REQUIRE(spy.count() == 0);
    }
    
    SECTION("supports all expected selection modes") {
        std::vector<QString> modes = {"none", "point", "line", "polygon"};
        
        for (auto const& mode : modes) {
            state.setSelectionMode(mode);
            REQUIRE(state.getSelectionMode() == mode);
        }
    }
}

// ==================== View State Tests ====================

TEST_CASE("TemporalProjectionViewState view state management", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("setXZoom changes zoom") {
        state.setXZoom(2.0);
        REQUIRE(state.viewState().x_zoom == 2.0);
    }
    
    SECTION("setXZoom emits viewStateChanged") {
        QSignalSpy spy(&state, &TemporalProjectionViewState::viewStateChanged);
        
        state.setXZoom(1.5);
        
        REQUIRE(spy.count() == 1);
    }
    
    SECTION("setYZoom changes zoom") {
        state.setYZoom(3.0);
        REQUIRE(state.viewState().y_zoom == 3.0);
    }
    
    SECTION("setPan changes pan values") {
        state.setPan(10.0, 20.0);
        REQUIRE(state.viewState().x_pan == 10.0);
        REQUIRE(state.viewState().y_pan == 20.0);
    }
    
    SECTION("setXBounds changes bounds") {
        state.setXBounds(-100.0, 100.0);
        REQUIRE(state.viewState().x_min == -100.0);
        REQUIRE(state.viewState().x_max == 100.0);
    }
    
    SECTION("setXBounds syncs horizontal axis") {
        state.setXBounds(0.0, 500.0);
        REQUIRE(state.horizontalAxisState()->getXMin() == 0.0);
        REQUIRE(state.horizontalAxisState()->getXMax() == 500.0);
    }
    
    SECTION("setYBounds changes bounds") {
        state.setYBounds(-50.0, 150.0);
        REQUIRE(state.viewState().y_min == -50.0);
        REQUIRE(state.viewState().y_max == 150.0);
    }
    
    SECTION("setYBounds syncs vertical axis") {
        state.setYBounds(-10.0, 90.0);
        REQUIRE(state.verticalAxisState()->getYMin() == -10.0);
        REQUIRE(state.verticalAxisState()->getYMax() == 90.0);
    }
}

// ==================== Serialization Tests ====================

TEST_CASE("TemporalProjectionViewState serialization", "[TemporalProjectionViewState]")
{
    TemporalProjectionViewState state;
    
    SECTION("round-trip preserves data keys") {
        state.addPointDataKey("p1");
        state.addPointDataKey("p2");
        state.addLineDataKey("l1");
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        auto point_keys = restored.getPointDataKeys();
        auto line_keys = restored.getLineDataKeys();
        
        REQUIRE(point_keys.size() == 2);
        REQUIRE(line_keys.size() == 1);
        REQUIRE(point_keys[0] == "p1");
        REQUIRE(point_keys[1] == "p2");
        REQUIRE(line_keys[0] == "l1");
    }
    
    SECTION("round-trip preserves rendering parameters") {
        state.setPointSize(8.0f);
        state.setLineWidth(3.0f);
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getPointSize() == 8.0f);
        REQUIRE(restored.getLineWidth() == 3.0f);
    }
    
    SECTION("round-trip preserves selection mode") {
        state.setSelectionMode("line");
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getSelectionMode() == "line");
    }
    
    SECTION("round-trip preserves view state") {
        state.setXZoom(2.5);
        state.setYZoom(1.5);
        state.setPan(50.0, -25.0);
        state.setXBounds(-200.0, 200.0);
        state.setYBounds(0.0, 100.0);
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        auto const& vs = restored.viewState();
        REQUIRE(vs.x_zoom == 2.5);
        REQUIRE(vs.y_zoom == 1.5);
        REQUIRE(vs.x_pan == 50.0);
        REQUIRE(vs.y_pan == -25.0);
        REQUIRE(vs.x_min == -200.0);
        REQUIRE(vs.x_max == 200.0);
        REQUIRE(vs.y_min == 0.0);
        REQUIRE(vs.y_max == 100.0);
    }
    
    SECTION("round-trip preserves display name") {
        state.setDisplayName("Custom View");
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getDisplayName() == "Custom View");
    }
    
    SECTION("round-trip preserves instance ID") {
        QString original_id = state.getInstanceId();
        
        std::string json = state.toJson();
        TemporalProjectionViewState restored;
        REQUIRE(restored.fromJson(json));
        
        REQUIRE(restored.getInstanceId() == original_id);
    }
    
    SECTION("fromJson with invalid JSON returns false") {
        TemporalProjectionViewState restored;
        REQUIRE_FALSE(restored.fromJson("{invalid json}"));
    }
}
