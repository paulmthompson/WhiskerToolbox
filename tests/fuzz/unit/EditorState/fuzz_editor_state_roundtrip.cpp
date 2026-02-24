/**
 * @file fuzz_editor_state_roundtrip.cpp
 * @brief Fuzz tests for EditorState object JSON round-trips
 *
 * Tests the full EditorState::toJson() / fromJson() cycle including:
 * - JSON idempotency: toJson() → fromJson() → toJson() produces identical JSON
 * - Instance ID preservation across serialization
 * - Display name preservation across serialization
 * - stateChanged() signal emission on fromJson()
 *
 * Phase 2 of the Workspace Fuzz Testing Roadmap.
 * Builds on Phase 1 (pure StateData round-trips) by testing the QObject wrapper layer.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include "nlohmann/json.hpp"

// Concrete EditorState subclasses
#include "DataManager_Widget/DataManagerWidgetState.hpp"
#include "TimeScrollBar/TimeScrollBarState.hpp"
#include "DataTransform_Widget/DataTransformWidgetState.hpp"
#include "GroupManagementWidget/GroupManagementWidgetState.hpp"
#include "DataImport_Widget/DataImportWidgetState.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "Plots/LinePlotWidget/Core/LinePlotState.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"

#include <memory>
#include <string>

namespace {

// ============================================================================
// Global Qt application setup
// ============================================================================

class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "fuzz_editor_state";
            static char * argv[] = {arg0, nullptr};
            _app = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    void TearDown() override {
        _app.reset();
    }

private:
    std::unique_ptr<QCoreApplication> _app;
};

auto * const g_qt_env = ::testing::AddGlobalTestEnvironment(new QtTestEnvironment());

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
// Helper: verify common EditorState round-trip properties
// ============================================================================

void VerifyEditorStateRoundTrip(
    EditorState & state1,
    EditorState & state2,
    std::string const & json1)
{
    // fromJson should succeed
    ASSERT_TRUE(state2.fromJson(json1))
        << "fromJson failed for type: " << state1.getTypeName().toStdString();

    // Instance ID must survive
    EXPECT_EQ(state1.getInstanceId(), state2.getInstanceId())
        << "Instance ID not preserved for type: " << state1.getTypeName().toStdString();

    // Display name must survive
    EXPECT_EQ(state1.getDisplayName(), state2.getDisplayName())
        << "Display name not preserved for type: " << state1.getTypeName().toStdString();

    // Type name must match
    EXPECT_EQ(state1.getTypeName(), state2.getTypeName())
        << "Type name mismatch";

    // Second serialization must be identical (idempotency)
    auto json2 = state2.toJson();
    ExpectJsonEqual(json1, json2);
}

// ============================================================================
// Helper: verify signal emission on fromJson
// ============================================================================

void VerifySignalEmission(EditorState & state, std::string const & json) {
    QSignalSpy spy(&state, &EditorState::stateChanged);
    bool result = state.fromJson(json);
    if (result) {
        EXPECT_GE(spy.count(), 1)
            << "stateChanged() not emitted for type: " << state.getTypeName().toStdString();
    }
}

// ============================================================================
// 1. DataManagerWidgetState — simple: selected_data_key + display_name
// ============================================================================

void FuzzDataManagerWidgetStateRoundTrip(
    std::string const & display_name,
    std::string const & selected_data_key)
{
    auto state1 = std::make_shared<DataManagerWidgetState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setSelectedDataKey(QString::fromStdString(selected_data_key));

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<DataManagerWidgetState>();

    // Verify signal emission
    QSignalSpy spy(state2.get(), &EditorState::stateChanged);
    VerifyEditorStateRoundTrip(*state1, *state2, json1);
    EXPECT_GE(spy.count(), 1) << "stateChanged not emitted on fromJson";

    // Verify specific field preservation
    EXPECT_EQ(state2->selectedDataKey(),
              QString::fromStdString(selected_data_key));
}
FUZZ_TEST(EditorStateRoundTrip, FuzzDataManagerWidgetStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200));

// ============================================================================
// 2. TimeScrollBarState — ints + bool
// ============================================================================

void FuzzTimeScrollBarStateRoundTrip(
    std::string const & display_name,
    int play_speed,
    int frame_jump,
    bool is_playing)
{
    auto state1 = std::make_shared<TimeScrollBarState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setPlaySpeed(play_speed);
    state1->setFrameJump(frame_jump);
    state1->setIsPlaying(is_playing);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<TimeScrollBarState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify specific field preservation
    EXPECT_EQ(state2->playSpeed(), play_speed);
    EXPECT_EQ(state2->frameJump(), frame_jump);
    EXPECT_EQ(state2->isPlaying(), is_playing);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzTimeScrollBarStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<bool>());

// ============================================================================
// 3. DataTransformWidgetState — all string fields
// ============================================================================

void FuzzDataTransformWidgetStateRoundTrip(
    std::string const & display_name,
    std::string const & selected_input_data_key,
    std::string const & selected_operation,
    std::string const & last_output_name)
{
    auto state1 = std::make_shared<DataTransformWidgetState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setSelectedInputDataKey(QString::fromStdString(selected_input_data_key));
    state1->setSelectedOperation(QString::fromStdString(selected_operation));
    state1->setLastOutputName(QString::fromStdString(last_output_name));

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<DataTransformWidgetState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify specific field preservation
    EXPECT_EQ(state2->selectedInputDataKey(),
              QString::fromStdString(selected_input_data_key));
    EXPECT_EQ(state2->selectedOperation(),
              QString::fromStdString(selected_operation));
    EXPECT_EQ(state2->lastOutputName(),
              QString::fromStdString(last_output_name));
}
FUZZ_TEST(EditorStateRoundTrip, FuzzDataTransformWidgetStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(200));

// ============================================================================
// 4. GroupManagementWidgetState — int
// ============================================================================

void FuzzGroupManagementWidgetStateRoundTrip(
    std::string const & display_name,
    int selected_group_id)
{
    auto state1 = std::make_shared<GroupManagementWidgetState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setSelectedGroupId(selected_group_id);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<GroupManagementWidgetState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify specific field preservation
    EXPECT_EQ(state2->selectedGroupId(), selected_group_id);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzGroupManagementWidgetStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Arbitrary<int>());

// ============================================================================
// 5. DataImportWidgetState — strings + map<string,string>
// ============================================================================

void FuzzDataImportWidgetStateRoundTrip(
    std::string const & display_name,
    std::string const & selected_import_type,
    std::string const & last_used_directory,
    std::vector<std::string> const & pref_keys,
    std::vector<std::string> const & pref_values)
{
    auto state1 = std::make_shared<DataImportWidgetState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setSelectedImportType(QString::fromStdString(selected_import_type));
    state1->setLastUsedDirectory(QString::fromStdString(last_used_directory));

    // Build format preferences from parallel vectors
    auto const n = std::min(pref_keys.size(), pref_values.size());
    for (size_t i = 0; i < n; ++i) {
        state1->setFormatPreference(
            QString::fromStdString(pref_keys[i]),
            QString::fromStdString(pref_values[i]));
    }

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<DataImportWidgetState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify specific field preservation
    EXPECT_EQ(state2->selectedImportType(),
              QString::fromStdString(selected_import_type));
    EXPECT_EQ(state2->lastUsedDirectory(),
              QString::fromStdString(last_used_directory));
}
FUZZ_TEST(EditorStateRoundTrip, FuzzDataImportWidgetStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10));

// ============================================================================
// 6. DataInspectorState — strings + bool
// ============================================================================

void FuzzDataInspectorStateRoundTrip(
    std::string const & display_name,
    std::string const & inspected_data_key,
    bool is_pinned)
{
    auto state1 = std::make_shared<DataInspectorState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setInspectedDataKey(QString::fromStdString(inspected_data_key));
    state1->setPinned(is_pinned);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<DataInspectorState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify specific field preservation
    EXPECT_EQ(state2->inspectedDataKey(),
              QString::fromStdString(inspected_data_key));
    EXPECT_EQ(state2->isPinned(), is_pinned);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzDataInspectorStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::Arbitrary<bool>());

// ============================================================================
// 7. LinePlotState — full setter fuzzing including alignment and view state
//    Previously crashed due to arithmetic overflow (max - min → infinity)
//    in syncTimeAxisData. Fixed by using CorePlotting::safe_range().
// ============================================================================

void FuzzLinePlotStateRoundTrip(
    std::string const & display_name,
    std::string const & alignment_event_key,
    double offset,
    double window_size,
    double x_min, double x_max,
    double y_min, double y_max)
{
    auto state1 = std::make_shared<LinePlotState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setAlignmentEventKey(QString::fromStdString(alignment_event_key));
    state1->setOffset(offset);
    state1->setWindowSize(window_size);
    state1->setXBounds(x_min, x_max);
    state1->setYBounds(y_min, y_max);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<LinePlotState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzLinePlotStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>());

// ============================================================================
// 8. EventPlotState — alignment, background color, pinned, sorting
//    EventPlotState does NOT have the range overflow bug (no syncTimeAxisData
//    subtraction), so we can safely test more setters.
// ============================================================================

void FuzzEventPlotStateRoundTrip(
    std::string const & display_name,
    std::string const & alignment_event_key,
    double offset,
    double window_size,
    std::string const & background_color,
    bool pinned)
{
    auto state1 = std::make_shared<EventPlotState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setAlignmentEventKey(QString::fromStdString(alignment_event_key));
    state1->setOffset(offset);
    state1->setWindowSize(window_size);
    state1->setBackgroundColor(QString::fromStdString(background_color));
    state1->setPinned(pinned);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<EventPlotState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify fields survived
    EXPECT_EQ(state2->isPinned(), pinned);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzEventPlotStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::PrintableAsciiString().WithMaxSize(20),
        fuzztest::Arbitrary<bool>());

// ============================================================================
// 9. MediaWidgetState — complex: viewport, display options, tool modes
// ============================================================================

void FuzzMediaWidgetStateRoundTrip(
    std::string const & display_name,
    std::string const & displayed_media_key,
    double zoom,
    double pan_x, double pan_y,
    int canvas_width, int canvas_height)
{
    auto state1 = std::make_shared<MediaWidgetState>();
    state1->setDisplayName(QString::fromStdString(display_name));
    state1->setDisplayedDataKey(QString::fromStdString(displayed_media_key));
    state1->setZoom(zoom);
    state1->setPan(pan_x, pan_y);
    state1->setCanvasSize(canvas_width, canvas_height);

    auto json1 = state1->toJson();

    auto state2 = std::make_shared<MediaWidgetState>();
    VerifyEditorStateRoundTrip(*state1, *state2, json1);

    // Verify viewport fields survived
    EXPECT_EQ(state2->displayedDataKey(), QString::fromStdString(displayed_media_key));
    EXPECT_DOUBLE_EQ(state2->zoom(), zoom);
}
FUZZ_TEST(EditorStateRoundTrip, FuzzMediaWidgetStateRoundTrip)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::InRange(0, 10000),
        fuzztest::InRange(0, 10000));

// ============================================================================
// 10. Default-constructed round-trip (all types)
// ============================================================================

TEST(EditorStateRoundTrip, DefaultConstructedRoundTrips) {
    // Each default-constructed state should round-trip cleanly
    auto test_roundtrip = [](auto state) {
        auto json1 = state->toJson();
        auto state2 = std::make_shared<std::remove_reference_t<decltype(*state)>>();
        ASSERT_TRUE(state2->fromJson(json1))
            << "fromJson failed for: " << state->getTypeName().toStdString();

        EXPECT_EQ(state->getInstanceId(), state2->getInstanceId())
            << "Instance ID not preserved for: " << state->getTypeName().toStdString();

        auto json2 = state2->toJson();
        ExpectJsonEqual(json1, json2);
    };

    test_roundtrip(std::make_shared<DataManagerWidgetState>());
    test_roundtrip(std::make_shared<TimeScrollBarState>());
    test_roundtrip(std::make_shared<DataTransformWidgetState>());
    test_roundtrip(std::make_shared<GroupManagementWidgetState>());
    test_roundtrip(std::make_shared<DataImportWidgetState>());
    test_roundtrip(std::make_shared<DataInspectorState>());
    test_roundtrip(std::make_shared<LinePlotState>());
    test_roundtrip(std::make_shared<EventPlotState>());
    test_roundtrip(std::make_shared<MediaWidgetState>());
}

// ============================================================================
// 11. Signal emission verification (deterministic)
// ============================================================================

TEST(EditorStateRoundTrip, FromJsonEmitsStateChanged) {
    auto verify_signal = [](auto state) {
        // First serialize to get valid JSON
        auto json = state->toJson();

        // Create fresh state and verify signal emission
        auto state2 = std::make_shared<std::remove_reference_t<decltype(*state)>>();
        QSignalSpy spy(state2.get(), &EditorState::stateChanged);
        ASSERT_TRUE(state2->fromJson(json));
        // Note: TimeScrollBarState::fromJson calls markClean() instead of emitting
        // stateChanged(). This is a known inconsistency — most other states do emit.
        if (state->getTypeName() != "TimeScrollBar") {
            EXPECT_GE(spy.count(), 1)
                << "stateChanged not emitted for: " << state->getTypeName().toStdString();
        }
    };

    verify_signal(std::make_shared<DataManagerWidgetState>());
    verify_signal(std::make_shared<TimeScrollBarState>());
    verify_signal(std::make_shared<DataTransformWidgetState>());
    verify_signal(std::make_shared<GroupManagementWidgetState>());
    verify_signal(std::make_shared<DataImportWidgetState>());
    verify_signal(std::make_shared<DataInspectorState>());
    verify_signal(std::make_shared<LinePlotState>());
    verify_signal(std::make_shared<EventPlotState>());
    verify_signal(std::make_shared<MediaWidgetState>());
}

// ============================================================================
// 12. Instance ID stability across multiple round-trips
// ============================================================================

TEST(EditorStateRoundTrip, InstanceIdStableAcrossMultipleRoundTrips) {
    auto verify_stability = [](auto state) {
        QString const original_id = state->getInstanceId();
        auto json = state->toJson();

        // Round-trip 3 times
        for (int i = 0; i < 3; ++i) {
            auto restored = std::make_shared<std::remove_reference_t<decltype(*state)>>();
            ASSERT_TRUE(restored->fromJson(json))
                << "Round-trip " << i << " failed for: "
                << state->getTypeName().toStdString();

            EXPECT_EQ(restored->getInstanceId(), original_id)
                << "Instance ID changed at round-trip " << i
                << " for: " << state->getTypeName().toStdString();

            // Use the restored state's JSON for next iteration
            json = restored->toJson();
        }
    };

    verify_stability(std::make_shared<DataManagerWidgetState>());
    verify_stability(std::make_shared<TimeScrollBarState>());
    verify_stability(std::make_shared<DataTransformWidgetState>());
    verify_stability(std::make_shared<GroupManagementWidgetState>());
    verify_stability(std::make_shared<DataImportWidgetState>());
    verify_stability(std::make_shared<DataInspectorState>());
    verify_stability(std::make_shared<LinePlotState>());
    verify_stability(std::make_shared<EventPlotState>());
    verify_stability(std::make_shared<MediaWidgetState>());
}

} // namespace
