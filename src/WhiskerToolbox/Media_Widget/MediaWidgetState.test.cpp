#include "MediaWidgetState.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QSignalSpy>

#include <string>

// === MediaWidgetState Tests ===

TEST_CASE("MediaWidgetState basics", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Instance ID is unique") {
        MediaWidgetState state1;
        MediaWidgetState state2;

        REQUIRE_FALSE(state1.getInstanceId().isEmpty());
        REQUIRE_FALSE(state2.getInstanceId().isEmpty());
        REQUIRE(state1.getInstanceId() != state2.getInstanceId());
    }

    SECTION("Type name is correct") {
        MediaWidgetState state;
        REQUIRE(state.getTypeName() == "MediaWidget");
    }

    SECTION("Display name defaults and can be set") {
        MediaWidgetState state;
        REQUIRE(state.getDisplayName() == "Media Viewer");

        state.setDisplayName("Custom Media");
        REQUIRE(state.getDisplayName() == "Custom Media");
    }

    SECTION("Dirty state tracking") {
        MediaWidgetState state;
        REQUIRE_FALSE(state.isDirty());

        state.setDisplayedDataKey("video_data");
        REQUIRE(state.isDirty());

        state.markClean();
        REQUIRE_FALSE(state.isDirty());
    }

    SECTION("Displayed data key management") {
        MediaWidgetState state;
        REQUIRE(state.displayedDataKey().isEmpty());

        state.setDisplayedDataKey("video_data");
        REQUIRE(state.displayedDataKey() == "video_data");

        state.setDisplayedDataKey("");
        REQUIRE(state.displayedDataKey().isEmpty());
    }
}

TEST_CASE("MediaWidgetState serialization", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Round-trip serialization") {
        MediaWidgetState original;
        original.setDisplayName("Test Media");
        original.setDisplayedDataKey("image_data");

        auto json = original.toJson();

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.displayedDataKey() == "image_data");
        REQUIRE(restored.getDisplayName() == "Test Media");
    }

    SECTION("Instance ID is preserved across serialization") {
        MediaWidgetState original;
        QString original_id = original.getInstanceId();
        original.setDisplayedDataKey("test");

        auto json = original.toJson();

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.getInstanceId() == original_id);
    }

    SECTION("Invalid JSON returns false") {
        MediaWidgetState state;
        REQUIRE_FALSE(state.fromJson("not valid json"));
        REQUIRE_FALSE(state.fromJson("{\"invalid\": \"schema\"}"));
    }

    SECTION("Empty state serializes correctly") {
        MediaWidgetState state;
        auto json = state.toJson();
        REQUIRE_FALSE(json.empty());

        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));
        REQUIRE(restored.displayedDataKey().isEmpty());
        REQUIRE(restored.getDisplayName() == "Media Viewer");
    }
}

TEST_CASE("MediaWidgetState signals", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("stateChanged emitted on modification") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::stateChanged);

        state.setDisplayedDataKey("data1");
        REQUIRE(spy.count() == 1);

        state.setDisplayedDataKey("data2");
        REQUIRE(spy.count() == 2);
    }

    SECTION("displayedDataKeyChanged emitted on key change") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::displayedDataKeyChanged);

        state.setDisplayedDataKey("video1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "video1");

        // Same value should not emit
        state.setDisplayedDataKey("video1");
        REQUIRE(spy.count() == 1);

        state.setDisplayedDataKey("video2");
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toString() == "video2");
    }

    SECTION("dirtyChanged emitted appropriately") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::dirtyChanged);

        state.setDisplayedDataKey("data1");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toBool() == true);

        state.setDisplayedDataKey("data2");// Already dirty, no new signal
        REQUIRE(spy.count() == 1);

        state.markClean();
        REQUIRE(spy.count() == 2);
        REQUIRE(spy.at(1).at(0).toBool() == false);
    }

    SECTION("displayNameChanged emitted") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &EditorState::displayNameChanged);

        state.setDisplayName("New Name");
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "New Name");
    }
}

// === Phase 3: Viewport State Tests ===

TEST_CASE("MediaWidgetState viewport state", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Default viewport values") {
        MediaWidgetState state;
        REQUIRE(state.zoom() == 1.0);
        auto [pan_x, pan_y] = state.pan();
        REQUIRE(pan_x == 0.0);
        REQUIRE(pan_y == 0.0);
        auto [width, height] = state.canvasSize();
        REQUIRE(width == 640);
        REQUIRE(height == 480);
    }

    SECTION("Set and get zoom") {
        MediaWidgetState state;
        QSignalSpy zoom_spy(&state, &MediaWidgetState::zoomChanged);
        QSignalSpy viewport_spy(&state, &MediaWidgetState::viewportChanged);

        state.setZoom(2.5);
        REQUIRE(state.zoom() == 2.5);
        REQUIRE(zoom_spy.count() == 1);
        REQUIRE(zoom_spy.at(0).at(0).toDouble() == 2.5);
        REQUIRE(viewport_spy.count() == 1);

        // Same value should not emit
        state.setZoom(2.5);
        REQUIRE(zoom_spy.count() == 1);
    }

    SECTION("Set and get pan") {
        MediaWidgetState state;
        QSignalSpy pan_spy(&state, &MediaWidgetState::panChanged);
        QSignalSpy viewport_spy(&state, &MediaWidgetState::viewportChanged);

        state.setPan(100.5, -50.25);
        auto [x, y] = state.pan();
        REQUIRE(x == 100.5);
        REQUIRE(y == -50.25);
        REQUIRE(pan_spy.count() == 1);
        REQUIRE(pan_spy.at(0).at(0).toDouble() == 100.5);
        REQUIRE(pan_spy.at(0).at(1).toDouble() == -50.25);
        REQUIRE(viewport_spy.count() == 1);

        // Same values should not emit
        state.setPan(100.5, -50.25);
        REQUIRE(pan_spy.count() == 1);
    }

    SECTION("Set and get canvas size") {
        MediaWidgetState state;
        QSignalSpy size_spy(&state, &MediaWidgetState::canvasSizeChanged);
        QSignalSpy viewport_spy(&state, &MediaWidgetState::viewportChanged);

        state.setCanvasSize(1920, 1080);
        auto [w, h] = state.canvasSize();
        REQUIRE(w == 1920);
        REQUIRE(h == 1080);
        REQUIRE(size_spy.count() == 1);
        REQUIRE(size_spy.at(0).at(0).toInt() == 1920);
        REQUIRE(size_spy.at(0).at(1).toInt() == 1080);
        REQUIRE(viewport_spy.count() == 1);
    }

    SECTION("Set complete viewport state") {
        MediaWidgetState state;
        QSignalSpy zoom_spy(&state, &MediaWidgetState::zoomChanged);
        QSignalSpy pan_spy(&state, &MediaWidgetState::panChanged);
        QSignalSpy size_spy(&state, &MediaWidgetState::canvasSizeChanged);
        QSignalSpy viewport_spy(&state, &MediaWidgetState::viewportChanged);

        ViewportState viewport;
        viewport.zoom = 3.0;
        viewport.pan_x = 200.0;
        viewport.pan_y = 100.0;
        viewport.canvas_width = 800;
        viewport.canvas_height = 600;

        state.setViewport(viewport);
        REQUIRE(state.zoom() == 3.0);
        auto [px, py] = state.pan();
        REQUIRE(px == 200.0);
        REQUIRE(py == 100.0);
        auto [cw, ch] = state.canvasSize();
        REQUIRE(cw == 800);
        REQUIRE(ch == 600);

        // All specific signals should emit
        REQUIRE(zoom_spy.count() == 1);
        REQUIRE(pan_spy.count() == 1);
        REQUIRE(size_spy.count() == 1);
        REQUIRE(viewport_spy.count() == 1);
    }

    SECTION("Viewport state round-trip serialization") {
        MediaWidgetState original;
        original.setZoom(2.0);
        original.setPan(50.0, 75.0);
        original.setCanvasSize(1280, 720);

        auto json = original.toJson();
        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));

        REQUIRE(restored.zoom() == 2.0);
        auto [px, py] = restored.pan();
        REQUIRE(px == 50.0);
        REQUIRE(py == 75.0);
        auto [cw, ch] = restored.canvasSize();
        REQUIRE(cw == 1280);
        REQUIRE(ch == 720);
    }
}

// === Phase 3: Feature Management Tests ===

TEST_CASE("MediaWidgetState feature management", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Set and check feature enabled - line") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::featureEnabledChanged);

        REQUIRE_FALSE(state.isFeatureEnabled("whisker_1", "line"));

        state.setFeatureEnabled("whisker_1", "line", true);
        REQUIRE(state.isFeatureEnabled("whisker_1", "line"));
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "whisker_1");
        REQUIRE(spy.at(0).at(1).toString() == "line");
        REQUIRE(spy.at(0).at(2).toBool() == true);

        state.setFeatureEnabled("whisker_1", "line", false);
        REQUIRE_FALSE(state.isFeatureEnabled("whisker_1", "line"));
        REQUIRE(spy.count() == 2);
    }

    SECTION("Set and check feature enabled - all types") {
        MediaWidgetState state;
        
        // Test all supported types
        std::vector<std::string> types = {"line", "mask", "point", "tensor", "interval", "media"};
        
        for (auto const& type : types) {
            QString qtype = QString::fromStdString(type);
            REQUIRE_FALSE(state.isFeatureEnabled("test_key", qtype));
            
            state.setFeatureEnabled("test_key", qtype, true);
            REQUIRE(state.isFeatureEnabled("test_key", qtype));
            
            state.setFeatureEnabled("test_key", qtype, false);
            REQUIRE_FALSE(state.isFeatureEnabled("test_key", qtype));
        }
    }

    SECTION("Enabled features list") {
        MediaWidgetState state;
        
        state.setFeatureEnabled("line_1", "line", true);
        state.setFeatureEnabled("line_2", "line", true);
        state.setFeatureEnabled("line_3", "line", false);
        state.setFeatureEnabled("mask_1", "mask", true);

        QStringList line_features = state.enabledFeatures("line");
        REQUIRE(line_features.size() == 2);
        REQUIRE(line_features.contains("line_1"));
        REQUIRE(line_features.contains("line_2"));
        REQUIRE_FALSE(line_features.contains("line_3"));

        QStringList mask_features = state.enabledFeatures("mask");
        REQUIRE(mask_features.size() == 1);
        REQUIRE(mask_features.contains("mask_1"));
    }

    SECTION("Unknown type returns false") {
        MediaWidgetState state;
        REQUIRE_FALSE(state.isFeatureEnabled("key", "unknown_type"));
    }
}

// === Phase 3: Display Options Tests ===

TEST_CASE("MediaWidgetState display options", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Line options CRUD") {
        MediaWidgetState state;
        QSignalSpy changed_spy(&state, &MediaWidgetState::displayOptionsChanged);
        QSignalSpy removed_spy(&state, &MediaWidgetState::displayOptionsRemoved);

        // Initially null
        REQUIRE(state.displayOptions().get<LineDisplayOptions>("whisker_1") == nullptr);

        // Set options
        LineDisplayOptions opts;
        opts.hex_color() = "#ff0000";
        opts.line_thickness = 3;
        opts.is_visible() = true;
        
        state.displayOptions().set("whisker_1", opts);
        
        auto* retrieved = state.displayOptions().get<LineDisplayOptions>("whisker_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#ff0000");
        REQUIRE(retrieved->line_thickness == 3);
        REQUIRE(changed_spy.count() == 1);
        REQUIRE(changed_spy.at(0).at(0).toString() == "whisker_1");
        REQUIRE(changed_spy.at(0).at(1).toString() == "line");

        // Remove options
        state.displayOptions().remove<LineDisplayOptions>("whisker_1");
        REQUIRE(state.displayOptions().get<LineDisplayOptions>("whisker_1") == nullptr);
        REQUIRE(removed_spy.count() == 1);
    }

    SECTION("Mask options CRUD") {
        MediaWidgetState state;

        MaskDisplayOptions opts;
        opts.hex_color() = "#00ff00";
        opts.show_bounding_box = true;
        opts.show_outline = true;
        
        state.displayOptions().set("mask_1", opts);
        
        auto* retrieved = state.displayOptions().get<MaskDisplayOptions>("mask_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#00ff00");
        REQUIRE(retrieved->show_bounding_box == true);
        REQUIRE(retrieved->show_outline == true);

        state.displayOptions().remove<MaskDisplayOptions>("mask_1");
        REQUIRE(state.displayOptions().get<MaskDisplayOptions>("mask_1") == nullptr);
    }

    SECTION("Point options CRUD") {
        MediaWidgetState state;

        PointDisplayOptions opts;
        opts.hex_color() = "#0000ff";
        opts.point_size = 10;
        opts.marker_shape = PointMarkerShape::Square;
        
        state.displayOptions().set("point_1", opts);
        
        auto* retrieved = state.displayOptions().get<PointDisplayOptions>("point_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#0000ff");
        REQUIRE(retrieved->point_size == 10);
        REQUIRE(retrieved->marker_shape == PointMarkerShape::Square);

        state.displayOptions().remove<PointDisplayOptions>("point_1");
        REQUIRE(state.displayOptions().get<PointDisplayOptions>("point_1") == nullptr);
    }

    SECTION("Tensor options CRUD") {
        MediaWidgetState state;

        TensorDisplayOptions opts;
        opts.display_channel = 2;
        opts.alpha() = 0.5f;
        
        state.displayOptions().set("tensor_1", opts);
        
        auto* retrieved = state.displayOptions().get<TensorDisplayOptions>("tensor_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->display_channel == 2);
        REQUIRE(retrieved->alpha() == 0.5f);

        state.displayOptions().remove<TensorDisplayOptions>("tensor_1");
        REQUIRE(state.displayOptions().get<TensorDisplayOptions>("tensor_1") == nullptr);
    }

    SECTION("Interval options CRUD") {
        MediaWidgetState state;

        DigitalIntervalDisplayOptions opts;
        opts.plotting_style = IntervalPlottingStyle::Border;
        opts.border_thickness = 10;
        
        state.displayOptions().set("interval_1", opts);
        
        auto* retrieved = state.displayOptions().get<DigitalIntervalDisplayOptions>("interval_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->plotting_style == IntervalPlottingStyle::Border);
        REQUIRE(retrieved->border_thickness == 10);

        state.displayOptions().remove<DigitalIntervalDisplayOptions>("interval_1");
        REQUIRE(state.displayOptions().get<DigitalIntervalDisplayOptions>("interval_1") == nullptr);
    }

    SECTION("Media options CRUD") {
        MediaWidgetState state;

        MediaDisplayOptions opts;
        opts.hex_color() = "#ffffff";
        opts.contrast_options.active = true;
        opts.contrast_options.alpha = 1.5;
        
        state.displayOptions().set("video_1", opts);
        
        auto* retrieved = state.displayOptions().get<MediaDisplayOptions>("video_1");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hex_color() == "#ffffff");
        REQUIRE(retrieved->contrast_options.active == true);
        REQUIRE(retrieved->contrast_options.alpha == 1.5);

        state.displayOptions().remove<MediaDisplayOptions>("video_1");
        REQUIRE(state.displayOptions().get<MediaDisplayOptions>("video_1") == nullptr);
    }

    SECTION("Display options round-trip serialization") {
        MediaWidgetState original;
        
        LineDisplayOptions line_opts;
        line_opts.hex_color() = "#ff0000";
        line_opts.line_thickness = 5;
        line_opts.is_visible() = true;
        original.displayOptions().set("line_1", line_opts);

        MaskDisplayOptions mask_opts;
        mask_opts.hex_color() = "#00ff00";
        mask_opts.show_outline = true;
        original.displayOptions().set("mask_1", mask_opts);

        auto json = original.toJson();
        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));

        auto* restored_line = restored.displayOptions().get<LineDisplayOptions>("line_1");
        REQUIRE(restored_line != nullptr);
        REQUIRE(restored_line->hex_color() == "#ff0000");
        REQUIRE(restored_line->line_thickness == 5);
        REQUIRE(restored_line->is_visible() == true);

        auto* restored_mask = restored.displayOptions().get<MaskDisplayOptions>("mask_1");
        REQUIRE(restored_mask != nullptr);
        REQUIRE(restored_mask->hex_color() == "#00ff00");
        REQUIRE(restored_mask->show_outline == true);
    }
}

// === Phase 3: Interaction Preferences Tests ===

TEST_CASE("MediaWidgetState interaction preferences", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Line preferences") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::interactionPrefsChanged);

        // Check defaults
        auto const& initial = state.linePrefs();
        REQUIRE(initial.smoothing_mode == "SimpleSmooth");
        REQUIRE(initial.edge_snapping_enabled == false);

        // Set new preferences
        LineInteractionPrefs prefs;
        prefs.smoothing_mode = "PolynomialFit";
        prefs.polynomial_order = 5;
        prefs.edge_snapping_enabled = true;
        prefs.edge_threshold = 150;
        
        state.setLinePrefs(prefs);
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "line");
        
        auto const& updated = state.linePrefs();
        REQUIRE(updated.smoothing_mode == "PolynomialFit");
        REQUIRE(updated.polynomial_order == 5);
        REQUIRE(updated.edge_snapping_enabled == true);
        REQUIRE(updated.edge_threshold == 150);
    }

    SECTION("Mask preferences") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::interactionPrefsChanged);

        MaskInteractionPrefs prefs;
        prefs.brush_size = 25;
        prefs.hover_circle_visible = false;
        prefs.allow_empty_mask = true;
        
        state.setMaskPrefs(prefs);
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "mask");
        
        auto const& updated = state.maskPrefs();
        REQUIRE(updated.brush_size == 25);
        REQUIRE(updated.hover_circle_visible == false);
        REQUIRE(updated.allow_empty_mask == true);
    }

    SECTION("Point preferences") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::interactionPrefsChanged);

        PointInteractionPrefs prefs;
        prefs.selection_threshold = 20.0f;
        
        state.setPointPrefs(prefs);
        REQUIRE(spy.count() == 1);
        REQUIRE(spy.at(0).at(0).toString() == "point");

        auto const& updated = state.pointPrefs();
        REQUIRE(updated.selection_threshold == 20.0f);
    }

    SECTION("Preferences round-trip serialization") {
        MediaWidgetState original;
        
        LineInteractionPrefs line_prefs;
        line_prefs.smoothing_mode = "PolynomialFit";
        line_prefs.polynomial_order = 7;
        original.setLinePrefs(line_prefs);

        MaskInteractionPrefs mask_prefs;
        mask_prefs.brush_size = 30;
        original.setMaskPrefs(mask_prefs);

        auto json = original.toJson();
        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));

        REQUIRE(restored.linePrefs().smoothing_mode == "PolynomialFit");
        REQUIRE(restored.linePrefs().polynomial_order == 7);
        REQUIRE(restored.maskPrefs().brush_size == 30);
    }
}

// === Phase 3: Text Overlay Tests ===

TEST_CASE("MediaWidgetState text overlays", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Add text overlay") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::textOverlaysChanged);

        TextOverlayData overlay;
        overlay.text = "Frame: 100";
        overlay.x_position = 0.1f;
        overlay.y_position = 0.1f;
        overlay.color = "#ff0000";
        overlay.font_size = 16;

        int id = state.addTextOverlay(overlay);
        REQUIRE(id >= 0);
        REQUIRE(spy.count() == 1);

        REQUIRE(state.textOverlays().size() == 1);
        REQUIRE(state.textOverlays()[0].text == "Frame: 100");
    }

    SECTION("Remove text overlay") {
        MediaWidgetState state;

        TextOverlayData overlay;
        overlay.text = "Test";
        int id = state.addTextOverlay(overlay);

        QSignalSpy spy(&state, &MediaWidgetState::textOverlaysChanged);

        REQUIRE(state.removeTextOverlay(id) == true);
        REQUIRE(spy.count() == 1);
        REQUIRE(state.textOverlays().empty());

        // Removing non-existent returns false
        REQUIRE(state.removeTextOverlay(id) == false);
    }

    SECTION("Update text overlay") {
        MediaWidgetState state;

        TextOverlayData overlay;
        overlay.text = "Original";
        int id = state.addTextOverlay(overlay);

        QSignalSpy spy(&state, &MediaWidgetState::textOverlaysChanged);

        TextOverlayData updated;
        updated.text = "Updated";
        updated.font_size = 24;

        REQUIRE(state.updateTextOverlay(id, updated) == true);
        REQUIRE(spy.count() == 1);

        auto* retrieved = state.getTextOverlay(id);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->text == "Updated");
        REQUIRE(retrieved->font_size == 24);
        REQUIRE(retrieved->id == id); // ID preserved

        // Updating non-existent returns false
        REQUIRE(state.updateTextOverlay(9999, updated) == false);
    }

    SECTION("Clear text overlays") {
        MediaWidgetState state;

        state.addTextOverlay(TextOverlayData{.text = "One"});
        state.addTextOverlay(TextOverlayData{.text = "Two"});
        REQUIRE(state.textOverlays().size() == 2);

        QSignalSpy spy(&state, &MediaWidgetState::textOverlaysChanged);

        state.clearTextOverlays();
        REQUIRE(spy.count() == 1);
        REQUIRE(state.textOverlays().empty());

        // Clear on empty does nothing
        state.clearTextOverlays();
        REQUIRE(spy.count() == 1); // No additional signal
    }

    SECTION("Get text overlay by ID") {
        MediaWidgetState state;

        int id1 = state.addTextOverlay(TextOverlayData{.text = "First"});
        int id2 = state.addTextOverlay(TextOverlayData{.text = "Second"});

        auto* first = state.getTextOverlay(id1);
        REQUIRE(first != nullptr);
        REQUIRE(first->text == "First");

        auto* second = state.getTextOverlay(id2);
        REQUIRE(second != nullptr);
        REQUIRE(second->text == "Second");

        auto* nonexistent = state.getTextOverlay(9999);
        REQUIRE(nonexistent == nullptr);
    }

    SECTION("Text overlays round-trip serialization") {
        MediaWidgetState original;

        TextOverlayData overlay1;
        overlay1.text = "Label 1";
        overlay1.x_position = 0.2f;
        overlay1.y_position = 0.3f;
        overlay1.color = "#00ff00";
        overlay1.font_size = 18;
        overlay1.orientation = TextOverlayOrientation::Vertical;
        original.addTextOverlay(overlay1);

        TextOverlayData overlay2;
        overlay2.text = "Label 2";
        overlay2.enabled = false;
        original.addTextOverlay(overlay2);

        auto json = original.toJson();
        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));

        REQUIRE(restored.textOverlays().size() == 2);
        
        auto const& r1 = restored.textOverlays()[0];
        REQUIRE(r1.text == "Label 1");
        REQUIRE(r1.x_position == 0.2f);
        REQUIRE(r1.y_position == 0.3f);
        REQUIRE(r1.color == "#00ff00");
        REQUIRE(r1.font_size == 18);
        REQUIRE(r1.orientation == TextOverlayOrientation::Vertical);

        auto const& r2 = restored.textOverlays()[1];
        REQUIRE(r2.text == "Label 2");
        REQUIRE(r2.enabled == false);
    }
}

// === Phase 3: Tool Mode Tests ===

TEST_CASE("MediaWidgetState tool modes", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("Line tool mode") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::toolModesChanged);

        REQUIRE(state.activeLineMode() == LineToolMode::None);

        state.setActiveLineMode(LineToolMode::Add);
        REQUIRE(state.activeLineMode() == LineToolMode::Add);
        REQUIRE(spy.count() == 1);

        state.setActiveLineMode(LineToolMode::Erase);
        REQUIRE(state.activeLineMode() == LineToolMode::Erase);
        REQUIRE(spy.count() == 2);

        state.setActiveLineMode(LineToolMode::Select);
        REQUIRE(state.activeLineMode() == LineToolMode::Select);

        state.setActiveLineMode(LineToolMode::DrawAllFrames);
        REQUIRE(state.activeLineMode() == LineToolMode::DrawAllFrames);

        // Same value should not emit
        state.setActiveLineMode(LineToolMode::DrawAllFrames);
        REQUIRE(spy.count() == 4);
    }

    SECTION("Mask tool mode") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::toolModesChanged);

        REQUIRE(state.activeMaskMode() == MaskToolMode::None);

        state.setActiveMaskMode(MaskToolMode::Brush);
        REQUIRE(state.activeMaskMode() == MaskToolMode::Brush);
        REQUIRE(spy.count() == 1);
    }

    SECTION("Point tool mode") {
        MediaWidgetState state;
        QSignalSpy spy(&state, &MediaWidgetState::toolModesChanged);

        REQUIRE(state.activePointMode() == PointToolMode::None);

        state.setActivePointMode(PointToolMode::Select);
        REQUIRE(state.activePointMode() == PointToolMode::Select);
        REQUIRE(spy.count() == 1);
    }

    SECTION("Tool modes round-trip serialization") {
        MediaWidgetState original;
        original.setActiveLineMode(LineToolMode::Erase);
        original.setActiveMaskMode(MaskToolMode::Brush);
        original.setActivePointMode(PointToolMode::Select);

        auto json = original.toJson();
        MediaWidgetState restored;
        REQUIRE(restored.fromJson(json));

        REQUIRE(restored.activeLineMode() == LineToolMode::Erase);
        REQUIRE(restored.activeMaskMode() == MaskToolMode::Brush);
        REQUIRE(restored.activePointMode() == PointToolMode::Select);
    }
}

// === Phase 3: Direct Data Access Tests ===

TEST_CASE("MediaWidgetState direct data access", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("data() returns const reference") {
        MediaWidgetState state;
        state.setDisplayedDataKey("test_key");
        state.setZoom(2.0);

        auto const& data = state.data();
        REQUIRE(data.displayed_media_key == "test_key");
        REQUIRE(data.viewport.zoom == 2.0);
    }

    SECTION("viewport() returns const reference") {
        MediaWidgetState state;
        state.setZoom(1.5);
        state.setPan(10.0, 20.0);

        auto const& viewport = state.viewport();
        REQUIRE(viewport.zoom == 1.5);
        REQUIRE(viewport.pan_x == 10.0);
        REQUIRE(viewport.pan_y == 20.0);
    }
}

// === Phase 3: Complex State Round-Trip Test ===

TEST_CASE("MediaWidgetState complex state round-trip", "[MediaWidgetState]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    MediaWidgetState original;
    
    // Set all state properties
    original.setDisplayName("Complex Test");
    original.setDisplayedDataKey("video.mp4");
    
    // Viewport
    original.setZoom(2.5);
    original.setPan(100.0, 200.0);
    original.setCanvasSize(1920, 1080);
    
    // Display options
    LineDisplayOptions line_opts;
    line_opts.hex_color() = "#ff0000";
    line_opts.line_thickness = 4;
    line_opts.is_visible() = true;
    original.displayOptions().set("whisker_1", line_opts);
    
    MaskDisplayOptions mask_opts;
    mask_opts.hex_color() = "#00ff00";
    mask_opts.show_outline = true;
    mask_opts.is_visible() = true;
    original.displayOptions().set("mask_1", mask_opts);
    
    // Interaction preferences
    LineInteractionPrefs line_prefs;
    line_prefs.smoothing_mode = "PolynomialFit";
    line_prefs.polynomial_order = 5;
    original.setLinePrefs(line_prefs);
    
    // Text overlays
    TextOverlayData overlay;
    overlay.text = "Test Overlay";
    overlay.font_size = 20;
    overlay.color = "#ffffff";
    original.addTextOverlay(overlay);
    
    // Tool modes
    original.setActiveLineMode(LineToolMode::Add);
    original.setActiveMaskMode(MaskToolMode::Brush);
    
    // Serialize
    auto json = original.toJson();
    REQUIRE_FALSE(json.empty());
    
    // Restore
    MediaWidgetState restored;
    REQUIRE(restored.fromJson(json));
    
    // Verify all state
    REQUIRE(restored.getDisplayName() == "Complex Test");
    REQUIRE(restored.displayedDataKey() == "video.mp4");
    REQUIRE(restored.getInstanceId() == original.getInstanceId());
    
    // Viewport
    REQUIRE(restored.zoom() == 2.5);
    auto [px, py] = restored.pan();
    REQUIRE(px == 100.0);
    REQUIRE(py == 200.0);
    auto [cw, ch] = restored.canvasSize();
    REQUIRE(cw == 1920);
    REQUIRE(ch == 1080);
    
    // Display options
    auto* r_line = restored.displayOptions().get<LineDisplayOptions>("whisker_1");
    REQUIRE(r_line != nullptr);
    REQUIRE(r_line->hex_color() == "#ff0000");
    REQUIRE(r_line->line_thickness == 4);
    REQUIRE(r_line->is_visible() == true);
    
    auto* r_mask = restored.displayOptions().get<MaskDisplayOptions>("mask_1");
    REQUIRE(r_mask != nullptr);
    REQUIRE(r_mask->hex_color() == "#00ff00");
    REQUIRE(r_mask->show_outline == true);
    
    // Interaction preferences
    REQUIRE(restored.linePrefs().smoothing_mode == "PolynomialFit");
    REQUIRE(restored.linePrefs().polynomial_order == 5);
    
    // Text overlays
    REQUIRE(restored.textOverlays().size() == 1);
    REQUIRE(restored.textOverlays()[0].text == "Test Overlay");
    REQUIRE(restored.textOverlays()[0].font_size == 20);
    
    // Tool modes
    REQUIRE(restored.activeLineMode() == LineToolMode::Add);
    REQUIRE(restored.activeMaskMode() == MaskToolMode::Brush);
}

// === Phase 4B: Consolidated Signal Tests ===

TEST_CASE("MediaWidgetState consolidated signals", "[MediaWidgetState][Phase4B]") {
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    SECTION("interactionPrefsChanged emitted for line prefs") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::interactionPrefsChanged);

        LineInteractionPrefs prefs;
        prefs.smoothing_mode = "NewMode";
        state.setLinePrefs(prefs);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "line");
    }

    SECTION("interactionPrefsChanged emitted for mask prefs") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::interactionPrefsChanged);

        MaskInteractionPrefs prefs;
        prefs.brush_size = 30;
        state.setMaskPrefs(prefs);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "mask");
    }

    SECTION("interactionPrefsChanged emitted for point prefs") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::interactionPrefsChanged);

        PointInteractionPrefs prefs;
        prefs.selection_threshold = 25.0f;
        state.setPointPrefs(prefs);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "point");
    }

    SECTION("textOverlaysChanged emitted on add") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::textOverlaysChanged);

        TextOverlayData overlay;
        overlay.text = "Test";
        state.addTextOverlay(overlay);

        REQUIRE(consolidated_spy.count() == 1);
    }

    SECTION("textOverlaysChanged emitted on remove") {
        MediaWidgetState state;
        TextOverlayData overlay;
        overlay.text = "Test";
        int id = state.addTextOverlay(overlay);

        QSignalSpy consolidated_spy(&state, &MediaWidgetState::textOverlaysChanged);

        state.removeTextOverlay(id);

        REQUIRE(consolidated_spy.count() == 1);
    }

    SECTION("textOverlaysChanged emitted on update") {
        MediaWidgetState state;
        TextOverlayData overlay;
        overlay.text = "Original";
        int id = state.addTextOverlay(overlay);

        QSignalSpy consolidated_spy(&state, &MediaWidgetState::textOverlaysChanged);

        overlay.text = "Updated";
        state.updateTextOverlay(id, overlay);


        REQUIRE(consolidated_spy.count() == 1);
    }

    SECTION("textOverlaysChanged emitted on clear") {
        MediaWidgetState state;
        state.addTextOverlay(TextOverlayData{.text = "One"});
        state.addTextOverlay(TextOverlayData{.text = "Two"});

        QSignalSpy consolidated_spy(&state, &MediaWidgetState::textOverlaysChanged);

        state.clearTextOverlays();

        REQUIRE(consolidated_spy.count() == 1);
    }

    SECTION("toolModesChanged emitted for line mode") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::toolModesChanged);

        state.setActiveLineMode(LineToolMode::Add);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "line");
    }

    SECTION("toolModesChanged emitted for mask mode") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::toolModesChanged);

        state.setActiveMaskMode(MaskToolMode::Brush);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "mask");
    }

    SECTION("toolModesChanged emitted for point mode") {
        MediaWidgetState state;
        QSignalSpy consolidated_spy(&state, &MediaWidgetState::toolModesChanged);

        state.setActivePointMode(PointToolMode::Select);

        REQUIRE(consolidated_spy.count() == 1);
        REQUIRE(consolidated_spy.at(0).at(0).toString() == "point");
    }

    SECTION("toolModesChanged not emitted when same mode set") {
        MediaWidgetState state;
        state.setActiveLineMode(LineToolMode::Add);

        QSignalSpy consolidated_spy(&state, &MediaWidgetState::toolModesChanged);

        // Setting same mode should not emit
        state.setActiveLineMode(LineToolMode::Add);
        REQUIRE(consolidated_spy.count() == 0);

        // Setting different mode should emit
        state.setActiveLineMode(LineToolMode::Erase);
        REQUIRE(consolidated_spy.count() == 1);
    }
}
