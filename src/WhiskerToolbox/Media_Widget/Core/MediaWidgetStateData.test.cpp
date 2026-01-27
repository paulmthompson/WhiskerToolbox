#include "MediaWidgetStateData.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

using Catch::Approx;

// ==================== TextOverlayData Tests ====================

TEST_CASE("TextOverlayData serialization", "[MediaWidgetStateData]") {
    SECTION("Default values serialize correctly") {
        TextOverlayData overlay;
        overlay.id = 1;
        overlay.text = "Test Label";
        
        auto json = rfl::json::write(overlay);
        auto result = rfl::json::read<TextOverlayData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.id == 1);
        REQUIRE(data.text == "Test Label");
        REQUIRE(data.orientation == TextOverlayOrientation::Horizontal);
        REQUIRE(data.x_position == Approx(0.5f));
        REQUIRE(data.y_position == Approx(0.5f));
        REQUIRE(data.color == "#ffffff");
        REQUIRE(data.font_size == 12);
        REQUIRE(data.enabled == true);
    }
    
    SECTION("TextOverlayOrientation enum serializes as string") {
        TextOverlayData overlay;
        overlay.orientation = TextOverlayOrientation::Vertical;
        
        auto json = rfl::json::write(overlay);
        
        // Check that enum is serialized as string, not integer
        REQUIRE(json.find("\"Vertical\"") != std::string::npos);
        REQUIRE(json.find("\"orientation\":0") == std::string::npos);
        REQUIRE(json.find("\"orientation\":1") == std::string::npos);
    }
    
    SECTION("Full overlay round-trip") {
        TextOverlayData original;
        original.id = 42;
        original.text = "Frame: 123";
        original.orientation = TextOverlayOrientation::Vertical;
        original.x_position = 0.1f;
        original.y_position = 0.9f;
        original.color = "#ff0000";
        original.font_size = 24;
        original.enabled = false;
        
        auto json = rfl::json::write(original);
        auto result = rfl::json::read<TextOverlayData>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.id == 42);
        REQUIRE(data.text == "Frame: 123");
        REQUIRE(data.orientation == TextOverlayOrientation::Vertical);
        REQUIRE(data.x_position == Approx(0.1f));
        REQUIRE(data.y_position == Approx(0.9f));
        REQUIRE(data.color == "#ff0000");
        REQUIRE(data.font_size == 24);
        REQUIRE(data.enabled == false);
    }
}

// ==================== Interaction Preferences Tests ====================

TEST_CASE("LineInteractionPrefs serialization", "[MediaWidgetStateData]") {
    SECTION("Default values") {
        LineInteractionPrefs prefs;
        auto json = rfl::json::write(prefs);
        auto result = rfl::json::read<LineInteractionPrefs>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.smoothing_mode == "SimpleSmooth");
        REQUIRE(data.polynomial_order == 3);
        REQUIRE(data.edge_snapping_enabled == false);
        REQUIRE(data.edge_threshold == 100);
        REQUIRE(data.edge_search_radius == 20);
        REQUIRE(data.eraser_radius == 10);
        REQUIRE(data.selection_threshold == Approx(15.0f));
    }
    
    SECTION("Custom values round-trip") {
        LineInteractionPrefs prefs;
        prefs.smoothing_mode = "PolynomialFit";
        prefs.polynomial_order = 5;
        prefs.edge_snapping_enabled = true;
        prefs.edge_threshold = 150;
        prefs.edge_search_radius = 30;
        prefs.eraser_radius = 15;
        prefs.selection_threshold = 20.0f;
        
        auto json = rfl::json::write(prefs);
        auto result = rfl::json::read<LineInteractionPrefs>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.smoothing_mode == "PolynomialFit");
        REQUIRE(data.polynomial_order == 5);
        REQUIRE(data.edge_snapping_enabled == true);
        REQUIRE(data.edge_threshold == 150);
        REQUIRE(data.edge_search_radius == 30);
        REQUIRE(data.eraser_radius == 15);
        REQUIRE(data.selection_threshold == Approx(20.0f));
    }
}

TEST_CASE("MaskInteractionPrefs serialization", "[MediaWidgetStateData]") {
    MaskInteractionPrefs prefs;
    prefs.brush_size = 25;
    prefs.hover_circle_visible = false;
    prefs.allow_empty_mask = true;
    
    auto json = rfl::json::write(prefs);
    auto result = rfl::json::read<MaskInteractionPrefs>(json);
    
    REQUIRE(result);
    auto const& data = result.value();
    REQUIRE(data.brush_size == 25);
    REQUIRE(data.hover_circle_visible == false);
    REQUIRE(data.allow_empty_mask == true);
}

TEST_CASE("PointInteractionPrefs serialization", "[MediaWidgetStateData]") {
    PointInteractionPrefs prefs;
    prefs.selection_threshold = 15.5f;
    
    auto json = rfl::json::write(prefs);
    auto result = rfl::json::read<PointInteractionPrefs>(json);
    
    REQUIRE(result);
    auto const& data = result.value();
    REQUIRE(data.selection_threshold == Approx(15.5f));
}

// ==================== ViewportState Tests ====================

TEST_CASE("ViewportState serialization", "[MediaWidgetStateData]") {
    SECTION("Default values") {
        ViewportState viewport;
        auto json = rfl::json::write(viewport);
        auto result = rfl::json::read<ViewportState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.zoom == Approx(1.0));
        REQUIRE(data.pan_x == Approx(0.0));
        REQUIRE(data.pan_y == Approx(0.0));
        REQUIRE(data.canvas_width == 640);
        REQUIRE(data.canvas_height == 480);
    }
    
    SECTION("Custom values round-trip") {
        ViewportState viewport;
        viewport.zoom = 2.5;
        viewport.pan_x = 100.5;
        viewport.pan_y = -50.25;
        viewport.canvas_width = 1920;
        viewport.canvas_height = 1080;
        
        auto json = rfl::json::write(viewport);
        auto result = rfl::json::read<ViewportState>(json);
        
        REQUIRE(result);
        auto const& data = result.value();
        REQUIRE(data.zoom == Approx(2.5));
        REQUIRE(data.pan_x == Approx(100.5));
        REQUIRE(data.pan_y == Approx(-50.25));
        REQUIRE(data.canvas_width == 1920);
        REQUIRE(data.canvas_height == 1080);
    }
}

// ==================== Tool Mode Enum Tests ====================

TEST_CASE("Tool mode enums serialize as strings", "[MediaWidgetStateData]") {
    SECTION("LineToolMode") {
        struct TestData {
            LineToolMode mode = LineToolMode::Add;
        };
        
        TestData data;
        auto json = rfl::json::write(data);
        REQUIRE(json.find("\"Add\"") != std::string::npos);
        
        data.mode = LineToolMode::DrawAllFrames;
        json = rfl::json::write(data);
        REQUIRE(json.find("\"DrawAllFrames\"") != std::string::npos);
    }
    
    SECTION("MaskToolMode") {
        struct TestData {
            MaskToolMode mode = MaskToolMode::Brush;
        };
        
        TestData data;
        auto json = rfl::json::write(data);
        REQUIRE(json.find("\"Brush\"") != std::string::npos);
    }
    
    SECTION("PointToolMode") {
        struct TestData {
            PointToolMode mode = PointToolMode::Select;
        };
        
        TestData data;
        auto json = rfl::json::write(data);
        REQUIRE(json.find("\"Select\"") != std::string::npos);
    }
}

// ==================== Main State Structure Tests ====================

TEST_CASE("MediaWidgetStateData full serialization", "[MediaWidgetStateData]") {
    SECTION("Empty state serializes correctly") {
        MediaWidgetStateData data;
        auto json = rfl::json::write(data);
        REQUIRE_FALSE(json.empty());
        
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        REQUIRE(result);
        auto const & data_out = result.value();
        REQUIRE(data_out.display_name == "Media Viewer");
        REQUIRE(data_out.displayed_media_key.empty());
        REQUIRE(data_out.viewport.zoom == Approx(1.0));
    }
    
    SECTION("Identity and display state") {
        MediaWidgetStateData data;
        data.instance_id = "test-instance-123";
        data.display_name = "Custom Viewer";
        data.displayed_media_key = "video.mp4";
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);
        auto const& data_out = result.value();
        REQUIRE(data_out.instance_id == "test-instance-123");
        REQUIRE(data_out.display_name == "Custom Viewer");
        REQUIRE(data_out.displayed_media_key == "video.mp4");
    }
    
    SECTION("Viewport state nested correctly in JSON") {
        MediaWidgetStateData data;
        data.viewport.zoom = 3.0;
        data.viewport.pan_x = 50.0;
        
        auto json = rfl::json::write(data);
        
        // Viewport should be a nested object
        REQUIRE(json.find("\"viewport\"") != std::string::npos);
        REQUIRE(json.find("\"zoom\":3") != std::string::npos);
    }
    
    SECTION("Display options maps") {
        MediaWidgetStateData data;
        
        // Add line display options
        LineDisplayOptions line_opts;
        line_opts.hex_color() = "#ff0000";
        line_opts.alpha() = 0.8f;
        line_opts.is_visible() = true;
        line_opts.line_thickness = 3;
        line_opts.show_points = true;
        data.line_options["whisker_1"] = line_opts;
        
        // Add mask display options  
        MaskDisplayOptions mask_opts;
        mask_opts.hex_color() = "#00ff00";
        mask_opts.is_visible() = true;
        mask_opts.show_outline = true;
        data.mask_options["roi_mask"] = mask_opts;
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);
        auto const& data_out = result.value();
        REQUIRE(data_out.line_options.size() == 1);
        REQUIRE(data_out.line_options.contains("whisker_1"));
        REQUIRE(data_out.line_options.at("whisker_1").hex_color() == "#ff0000");
        REQUIRE(data_out.line_options.at("whisker_1").alpha() == Approx(0.8f));
        REQUIRE(data_out.line_options.at("whisker_1").is_visible() == true);
        REQUIRE(data_out.line_options.at("whisker_1").line_thickness == 3);
        REQUIRE(data_out.line_options.at("whisker_1").show_points == true);
        
        REQUIRE(data_out.mask_options.size() == 1);
        REQUIRE(data_out.mask_options.contains("roi_mask"));
        REQUIRE(data_out.mask_options.at("roi_mask").hex_color() == "#00ff00");
        REQUIRE(data_out.mask_options.at("roi_mask").show_outline == true);
    }
    
    SECTION("rfl::Flatten in display options produces flat JSON") {
        LineDisplayOptions opts;
        opts.hex_color() = "#00ff00";
        opts.alpha() = 0.5f;
        opts.is_visible() = true;
        opts.line_thickness = 4;
        
        auto json = rfl::json::write(opts);
        
        // Should NOT have nested "common" object
        REQUIRE(json.find("\"common\"") == std::string::npos);
        // Should have flat fields
        REQUIRE(json.find("\"hex_color\":\"#00ff00\"") != std::string::npos);
        REQUIRE(json.find("\"line_thickness\":4") != std::string::npos);
    }
    
    SECTION("Text overlays") {
        MediaWidgetStateData data;
        
        TextOverlayData overlay1;
        overlay1.id = 0;
        overlay1.text = "Frame: 100";
        overlay1.x_position = 0.1f;
        overlay1.y_position = 0.1f;
        
        TextOverlayData overlay2;
        overlay2.id = 1;
        overlay2.text = "Trial: 5";
        overlay2.orientation = TextOverlayOrientation::Vertical;
        overlay2.x_position = 0.9f;
        overlay2.y_position = 0.5f;
        
        data.text_overlays.push_back(overlay1);
        data.text_overlays.push_back(overlay2);
        data.next_overlay_id = 2;
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);
        auto const& data_out = result.value();
        REQUIRE(data_out.text_overlays.size() == 2);
        REQUIRE(data_out.text_overlays[0].text == "Frame: 100");
        REQUIRE(data_out.text_overlays[1].text == "Trial: 5");
        REQUIRE(data_out.text_overlays[1].orientation == TextOverlayOrientation::Vertical);
        REQUIRE(data_out.next_overlay_id == 2);
    }
    
    SECTION("Interaction preferences") {
        MediaWidgetStateData data;
        
        data.line_prefs.smoothing_mode = "PolynomialFit";
        data.line_prefs.polynomial_order = 5;
        data.line_prefs.edge_snapping_enabled = true;
        
        data.mask_prefs.brush_size = 30;
        data.mask_prefs.hover_circle_visible = false;
        
        data.point_prefs.selection_threshold = 20.0f;
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);
        auto const& data_out = result.value();
        REQUIRE(data_out.line_prefs.smoothing_mode == "PolynomialFit");
        REQUIRE(data_out.line_prefs.polynomial_order == 5);
        REQUIRE(data_out.line_prefs.edge_snapping_enabled == true);
        REQUIRE(data_out.mask_prefs.brush_size == 30);
        REQUIRE(data_out.mask_prefs.hover_circle_visible == false);
        REQUIRE(data_out.point_prefs.selection_threshold == Approx(20.0f));
    }
    
    SECTION("Active tool modes") {
        MediaWidgetStateData data;
        
        data.active_line_mode = LineToolMode::Add;
        data.active_mask_mode = MaskToolMode::Brush;
        data.active_point_mode = PointToolMode::Select;
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);
        auto const& data_out = result.value();
        REQUIRE(data_out.active_line_mode == LineToolMode::Add);
        REQUIRE(data_out.active_mask_mode == MaskToolMode::Brush);
        REQUIRE(data_out.active_point_mode == PointToolMode::Select);
    }
    
    SECTION("Complex state with all fields populated") {
        MediaWidgetStateData data;
        
        // Identity
        data.instance_id = "media-widget-001";
        data.display_name = "Whisker Analysis View";
        data.displayed_media_key = "recording_001.mp4";
        
        // Viewport
        data.viewport.zoom = 2.5;
        data.viewport.pan_x = 100.0;
        data.viewport.pan_y = 50.0;
        data.viewport.canvas_width = 1920;
        data.viewport.canvas_height = 1080;
        
        // Display options for multiple features
        LineDisplayOptions line1;
        line1.hex_color() = "#ff0000";
        line1.line_thickness = 3;
        data.line_options["whisker_1"] = line1;
        
        LineDisplayOptions line2;
        line2.hex_color() = "#00ff00";
        line2.line_thickness = 2;
        data.line_options["whisker_2"] = line2;
        
        PointDisplayOptions point1;
        point1.hex_color() = "#0000ff";
        point1.point_size = 8;
        point1.marker_shape = PointMarkerShape::Diamond;
        data.point_options["nose_tip"] = point1;
        
        // Preferences
        data.line_prefs.edge_snapping_enabled = true;
        data.mask_prefs.brush_size = 25;
        
        // Text overlays
        TextOverlayData overlay;
        overlay.id = 0;
        overlay.text = "Subject: Mouse_A";
        overlay.x_position = 0.05f;
        overlay.y_position = 0.95f;
        data.text_overlays.push_back(overlay);
        data.next_overlay_id = 1;
        
        // Tool modes
        data.active_line_mode = LineToolMode::Add;
        
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<MediaWidgetStateData>(json);
        
        REQUIRE(result);

        auto const& data_out = result.value();
        
        // Verify all fields preserved
        REQUIRE(data_out.instance_id == "media-widget-001");
        REQUIRE(data_out.display_name == "Whisker Analysis View");
        REQUIRE(data_out.displayed_media_key == "recording_001.mp4");
        REQUIRE(data_out.viewport.zoom == Approx(2.5));
        REQUIRE(data_out.viewport.canvas_width == 1920);
        REQUIRE(data_out.line_options.size() == 2);
        REQUIRE(data_out.line_options.at("whisker_1").line_thickness == 3);
        REQUIRE(data_out.line_options.at("whisker_2").hex_color() == "#00ff00");
        REQUIRE(data_out.point_options.at("nose_tip").marker_shape == PointMarkerShape::Diamond);
        REQUIRE(data_out.line_prefs.edge_snapping_enabled == true);
        REQUIRE(data_out.mask_prefs.brush_size == 25);
        REQUIRE(data_out.text_overlays.size() == 1);
        REQUIRE(data_out.text_overlays[0].text == "Subject: Mouse_A");
        REQUIRE(data_out.active_line_mode == LineToolMode::Add);
    }
}

TEST_CASE("MediaWidgetStateData JSON structure", "[MediaWidgetStateData]") {
    SECTION("Nested vs flat structure") {
        MediaWidgetStateData data;
        data.viewport.zoom = 2.0;
        
        LineDisplayOptions line_opts;
        line_opts.hex_color() = "#ff0000";
        data.line_options["test_line"] = line_opts;
        
        auto json = rfl::json::write(data);
        
        // viewport should be nested
        REQUIRE(json.find("\"viewport\":{") != std::string::npos);
        
        // line_options should contain nested objects
        REQUIRE(json.find("\"line_options\":{") != std::string::npos);
        REQUIRE(json.find("\"test_line\":{") != std::string::npos);
        
        // Within line_options, common fields should be FLAT (no "common" key)
        // The hex_color should appear directly in the line options object
        REQUIRE(json.find("\"common\"") == std::string::npos);
    }
}
