/**
 * @file fuzz_state_data_roundtrip.cpp
 * @brief Fuzz tests for EditorState data struct JSON round-trips
 *
 * Tests the core property that for every StateData struct:
 *   rfl::json::write(data) → rfl::json::read<T>() → rfl::json::write() produces identical JSON.
 *
 * This exercises reflect-cpp serialization for all field types used in
 * editor states: strings, ints, doubles, bools, vectors, maps, optionals,
 * nested structs, and enums.
 *
 * Phase 1 of the Workspace Fuzz Testing Roadmap.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include "nlohmann/json.hpp"

#include <algorithm>
#include <limits>

// StateData headers (pure data — no Qt dependency)
#include "Test_Widget/TestWidgetStateData.hpp"
#include "DataManager_Widget/DataManagerWidgetStateData.hpp"
#include "TimeScrollBar/TimeScrollBarStateData.hpp"
#include "DataTransform_Widget/DataTransformWidgetStateData.hpp"
#include "GroupManagementWidget/GroupManagementWidgetStateData.hpp"
#include "DataImport_Widget/DataImportWidgetStateData.hpp"
#include "DataInspector_Widget/DataInspectorStateData.hpp"
#include "Plots/LinePlotWidget/Core/LinePlotStateData.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotStateData.hpp"

namespace {

// ============================================================================
// Helper: compare JSON structurally (order-independent key comparison)
// ============================================================================

void ExpectJsonEqual(std::string const & json1, std::string const & json2) {
    auto parsed1 = nlohmann::json::parse(json1);
    auto parsed2 = nlohmann::json::parse(json2);
    EXPECT_EQ(parsed1, parsed2)
        << "JSON mismatch:\n  First:  " << json1.substr(0, 500)
        << "\n  Second: " << json2.substr(0, 500);
}

// ============================================================================
// 1. TestWidgetStateData — booleans, strings, double, int
// ============================================================================

void FuzzTestWidgetStateDataRoundTrip(
    std::string const & display_name,
    bool show_grid,
    bool show_crosshair,
    bool enable_animation,
    std::string const & highlight_color,
    double zoom_level,
    int grid_spacing,
    std::string const & label_text)
{
    TestWidgetStateData data;
    data.display_name = display_name;
    data.show_grid = show_grid;
    data.show_crosshair = show_crosshair;
    data.enable_animation = enable_animation;
    data.highlight_color = highlight_color;
    data.zoom_level = zoom_level;
    data.grid_spacing = grid_spacing;
    data.label_text = label_text;
    data.instance_id = "fuzz-test-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<TestWidgetStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize TestWidgetStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    // Verify key field preservation
    EXPECT_EQ((*result).show_grid, show_grid);
    EXPECT_EQ((*result).show_crosshair, show_crosshair);
    EXPECT_EQ((*result).enable_animation, enable_animation);
    EXPECT_EQ((*result).grid_spacing, grid_spacing);
    EXPECT_EQ((*result).label_text, label_text);
    EXPECT_EQ((*result).instance_id, "fuzz-test-id");
}
FUZZ_TEST(StateDataRoundTrip, FuzzTestWidgetStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),   // display_name
        fuzztest::Arbitrary<bool>(),                            // show_grid
        fuzztest::Arbitrary<bool>(),                            // show_crosshair
        fuzztest::Arbitrary<bool>(),                            // enable_animation
        fuzztest::PrintableAsciiString().WithMaxSize(50),    // highlight_color
        fuzztest::Finite<double>(),                             // zoom_level
        fuzztest::Arbitrary<int>(),                             // grid_spacing
        fuzztest::PrintableAsciiString().WithMaxSize(200)    // label_text
    );

// ============================================================================
// 2. DataManagerWidgetStateData — minimal struct (3 fields)
// ============================================================================

void FuzzDataManagerWidgetStateDataRoundTrip(
    std::string const & selected_data_key,
    std::string const & display_name)
{
    DataManagerWidgetStateData data;
    data.selected_data_key = selected_data_key;
    data.display_name = display_name;
    data.instance_id = "dm-fuzz-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<DataManagerWidgetStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize DataManagerWidgetStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).selected_data_key, selected_data_key);
    EXPECT_EQ((*result).display_name, display_name);
}
FUZZ_TEST(StateDataRoundTrip, FuzzDataManagerWidgetStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200)
    );

// ============================================================================
// 3. TimeScrollBarStateData — ints + bool
// ============================================================================

void FuzzTimeScrollBarStateDataRoundTrip(
    int play_speed,
    int frame_jump,
    bool is_playing,
    std::string const & display_name)
{
    TimeScrollBarStateData data;
    data.play_speed = play_speed;
    data.frame_jump = frame_jump;
    data.is_playing = is_playing;
    data.display_name = display_name;
    data.instance_id = "tsb-fuzz-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<TimeScrollBarStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize TimeScrollBarStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).play_speed, play_speed);
    EXPECT_EQ((*result).frame_jump, frame_jump);
    EXPECT_EQ((*result).is_playing, is_playing);
}
FUZZ_TEST(StateDataRoundTrip, FuzzTimeScrollBarStateDataRoundTrip)
    .WithDomains(
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::PrintableAsciiString().WithMaxSize(100)
    );

// ============================================================================
// 4. DataTransformWidgetStateData — all string fields
// ============================================================================

void FuzzDataTransformWidgetStateDataRoundTrip(
    std::string const & selected_input_data_key,
    std::string const & selected_operation,
    std::string const & last_output_name,
    std::string const & display_name)
{
    DataTransformWidgetStateData data;
    data.selected_input_data_key = selected_input_data_key;
    data.selected_operation = selected_operation;
    data.last_output_name = last_output_name;
    data.display_name = display_name;
    data.instance_id = "dt-fuzz-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<DataTransformWidgetStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize DataTransformWidgetStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).selected_input_data_key, selected_input_data_key);
    EXPECT_EQ((*result).selected_operation, selected_operation);
    EXPECT_EQ((*result).last_output_name, last_output_name);
}
FUZZ_TEST(StateDataRoundTrip, FuzzDataTransformWidgetStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200)
    );

// ============================================================================
// 5. GroupManagementWidgetStateData — has vector<int>
// ============================================================================

void FuzzGroupManagementWidgetStateDataRoundTrip(
    int selected_group_id,
    std::vector<int> const & expanded_groups,
    std::string const & display_name)
{
    GroupManagementWidgetStateData data;
    data.selected_group_id = selected_group_id;
    data.expanded_groups = expanded_groups;
    data.display_name = display_name;
    data.instance_id = "gm-fuzz-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<GroupManagementWidgetStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize GroupManagementWidgetStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).selected_group_id, selected_group_id);
    EXPECT_EQ((*result).expanded_groups, expanded_groups);
}
FUZZ_TEST(StateDataRoundTrip, FuzzGroupManagementWidgetStateDataRoundTrip)
    .WithDomains(
        fuzztest::Arbitrary<int>(),
        fuzztest::VectorOf(fuzztest::Arbitrary<int>()).WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(100)
    );

// ============================================================================
// 6. DataImportWidgetStateData — has map<string, string>
// ============================================================================

void FuzzDataImportWidgetStateDataRoundTrip(
    std::string const & selected_import_type,
    std::string const & last_used_directory,
    std::vector<std::string> const & pref_keys,
    std::vector<std::string> const & pref_values,
    std::string const & display_name)
{
    DataImportWidgetStateData data;
    data.selected_import_type = selected_import_type;
    data.last_used_directory = last_used_directory;
    data.display_name = display_name;
    data.instance_id = "di-fuzz-id";

    // Build map from parallel vectors
    auto const n = std::min(pref_keys.size(), pref_values.size());
    for (size_t i = 0; i < n; ++i) {
        data.format_preferences[pref_keys[i]] = pref_values[i];
    }

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<DataImportWidgetStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize DataImportWidgetStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).selected_import_type, selected_import_type);
    EXPECT_EQ((*result).last_used_directory, last_used_directory);
    EXPECT_EQ((*result).format_preferences, data.format_preferences);
}
FUZZ_TEST(StateDataRoundTrip, FuzzDataImportWidgetStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::PrintableAsciiString().WithMaxSize(100)
    );

// ============================================================================
// 7. DataInspectorStateData — has vector<string> + embedded JSON string
// ============================================================================

void FuzzDataInspectorStateDataRoundTrip(
    std::string const & inspected_data_key,
    bool is_pinned,
    std::vector<std::string> const & collapsed_sections,
    std::string const & ui_state_json,
    std::string const & display_name)
{
    DataInspectorStateData data;
    data.inspected_data_key = inspected_data_key;
    data.is_pinned = is_pinned;
    data.collapsed_sections = collapsed_sections;
    data.ui_state_json = ui_state_json;
    data.display_name = display_name;
    data.instance_id = "dins-fuzz-id";

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<DataInspectorStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize DataInspectorStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    EXPECT_EQ((*result).inspected_data_key, inspected_data_key);
    EXPECT_EQ((*result).is_pinned, is_pinned);
    EXPECT_EQ((*result).collapsed_sections, collapsed_sections);
    EXPECT_EQ((*result).ui_state_json, ui_state_json);
}
FUZZ_TEST(StateDataRoundTrip, FuzzDataInspectorStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::Arbitrary<bool>(),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::PrintableAsciiString().WithMaxSize(500),  // ui_state_json — arbitrary string
        fuzztest::PrintableAsciiString().WithMaxSize(100)
    );

// ============================================================================
// 8. LinePlotStateData — nested structs, map<string, struct>
// ============================================================================

void FuzzLinePlotStateDataRoundTrip(
    std::string const & alignment_event_key,
    double offset,
    double window_size,
    double x_min, double x_max,
    double y_min, double y_max,
    double time_min, double time_max,
    double vert_y_min, double vert_y_max,
    std::vector<std::string> const & series_keys,
    std::vector<double> const & line_thicknesses)
{
    LinePlotStateData data;
    data.instance_id = "lp-fuzz-id";
    data.display_name = "Fuzz Line Plot";

    // Alignment
    data.alignment.alignment_event_key = alignment_event_key;
    data.alignment.offset = offset;
    data.alignment.window_size = window_size;

    // View state
    data.view_state.x_min = x_min;
    data.view_state.x_max = x_max;
    data.view_state.y_min = y_min;
    data.view_state.y_max = y_max;

    // Time axis
    data.time_axis.min_range = time_min;
    data.time_axis.max_range = time_max;

    // Vertical axis
    data.vertical_axis.y_min = vert_y_min;
    data.vertical_axis.y_max = vert_y_max;

    // Plot series
    auto const n = std::min(series_keys.size(), line_thicknesses.size());
    for (size_t i = 0; i < std::min(n, size_t{10}); ++i) {
        LinePlotOptions opts;
        opts.series_key = series_keys[i];
        opts.line_thickness = line_thicknesses[i];
        opts.hex_color = "#FF0000";
        data.plot_series["series_" + std::to_string(i)] = opts;
    }

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<LinePlotStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize LinePlotStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    // Verify nested struct fields survived
    EXPECT_EQ((*result).alignment.alignment_event_key, alignment_event_key);
    EXPECT_EQ((*result).plot_series.size(), data.plot_series.size());
}
FUZZ_TEST(StateDataRoundTrip, FuzzLinePlotStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),    // alignment_event_key
        fuzztest::Finite<double>(),                              // offset
        fuzztest::Finite<double>(),                              // window_size
        fuzztest::Finite<double>(),                              // x_min
        fuzztest::Finite<double>(),                              // x_max
        fuzztest::Finite<double>(),                              // y_min
        fuzztest::Finite<double>(),                              // y_max
        fuzztest::Finite<double>(),                              // time_min
        fuzztest::Finite<double>(),                              // time_max
        fuzztest::Finite<double>(),                              // vert_y_min
        fuzztest::Finite<double>(),                              // vert_y_max
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::Finite<double>()).WithMaxSize(10)
    );

// ============================================================================
// 9. EventPlotStateData — nested structs, enums, map<string, struct>
// ============================================================================

void FuzzEventPlotStateDataRoundTrip(
    std::string const & alignment_event_key,
    int interval_alignment_type,
    double offset,
    double window_size,
    double x_min, double x_max,
    std::string const & background_color,
    bool pinned,
    int sorting_mode,
    std::string const & x_label,
    bool show_grid,
    std::vector<std::string> const & event_keys,
    std::vector<double> const & tick_thicknesses,
    std::vector<int> const & glyph_types)
{
    EventPlotStateData data;
    data.instance_id = "ep-fuzz-id";
    data.display_name = "Fuzz Event Plot";

    // Alignment
    data.alignment.alignment_event_key = alignment_event_key;
    data.alignment.interval_alignment_type =
        (interval_alignment_type % 2 == 0)
            ? IntervalAlignmentType::Beginning
            : IntervalAlignmentType::End;
    data.alignment.offset = offset;
    data.alignment.window_size = window_size;

    // View state
    data.view_state.x_min = x_min;
    data.view_state.x_max = x_max;

    // Axis options
    data.axis_options.x_label = x_label;
    data.axis_options.show_grid = show_grid;

    // Top-level fields
    data.background_color = background_color;
    data.pinned = pinned;
    data.sorting_mode = static_cast<TrialSortMode>(static_cast<unsigned>(sorting_mode) % 3u);

    // Plot events
    auto const n = std::min({event_keys.size(), tick_thicknesses.size(), glyph_types.size()});
    for (size_t i = 0; i < std::min(n, size_t{10}); ++i) {
        EventPlotOptions opts;
        opts.event_key = event_keys[i];
        std::string event_name = "event_" + std::to_string(i);
        data.plot_events[event_name] = opts;

        // Per-key glyph style in the separate map
        CorePlotting::GlyphStyleData glyph;
        auto const clamped = std::clamp(tick_thicknesses[i],
                                         static_cast<double>(-std::numeric_limits<float>::max()),
                                         static_cast<double>(std::numeric_limits<float>::max()));
        glyph.size = static_cast<float>(clamped);
        // Use unsigned cast to avoid std::abs(INT_MIN) UB
        glyph.glyph_type = static_cast<CorePlotting::GlyphType>(static_cast<unsigned>(glyph_types[i]) % 6u);
        glyph.hex_color = "#0000FF";
        data.event_glyph_styles[event_name] = glyph;
    }

    auto json1 = rfl::json::write(data);
    auto result = rfl::json::read<EventPlotStateData>(json1);
    ASSERT_TRUE(result) << "Failed to deserialize EventPlotStateData";

    auto json2 = rfl::json::write(*result);
    ExpectJsonEqual(json1, json2);

    // Verify nested struct and enum fields survived
    EXPECT_EQ((*result).alignment.alignment_event_key, alignment_event_key);
    EXPECT_EQ((*result).pinned, pinned);
    EXPECT_EQ((*result).plot_events.size(), data.plot_events.size());
    EXPECT_EQ((*result).axis_options.show_grid, show_grid);
}
FUZZ_TEST(StateDataRoundTrip, FuzzEventPlotStateDataRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),    // alignment_event_key
        fuzztest::Arbitrary<int>(),                              // interval_alignment_type
        fuzztest::Finite<double>(),                              // offset
        fuzztest::Finite<double>(),                              // window_size
        fuzztest::Finite<double>(),                              // x_min
        fuzztest::Finite<double>(),                              // x_max
        fuzztest::PrintableAsciiString().WithMaxSize(20),     // background_color
        fuzztest::Arbitrary<bool>(),                             // pinned
        fuzztest::Arbitrary<int>(),                              // sorting_mode
        fuzztest::PrintableAsciiString().WithMaxSize(50),     // x_label
        fuzztest::Arbitrary<bool>(),                             // show_grid
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::Finite<double>()).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::Arbitrary<int>()).WithMaxSize(10)
    );

// ============================================================================
// 10. Default-constructed round-trip (all types survive empty serialization)
// ============================================================================

TEST(StateDataRoundTrip, DefaultConstructedRoundTrips) {
    // Each default-constructed struct should round-trip cleanly
    {
        TestWidgetStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<TestWidgetStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        DataManagerWidgetStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<DataManagerWidgetStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        TimeScrollBarStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<TimeScrollBarStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        DataTransformWidgetStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<DataTransformWidgetStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        GroupManagementWidgetStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<GroupManagementWidgetStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        DataImportWidgetStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<DataImportWidgetStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        DataInspectorStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<DataInspectorStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        LinePlotStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<LinePlotStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
    {
        EventPlotStateData data;
        auto json = rfl::json::write(data);
        auto result = rfl::json::read<EventPlotStateData>(json);
        ASSERT_TRUE(result);
        ExpectJsonEqual(json, rfl::json::write(*result));
    }
}

} // namespace
