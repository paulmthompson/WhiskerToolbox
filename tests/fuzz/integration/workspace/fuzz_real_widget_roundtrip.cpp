/**
 * @file fuzz_real_widget_roundtrip.cpp
 * @brief Fuzz tests for real widget type placement → serialize → restore → verify
 *
 * Phase 6 of the Workspace Fuzz Testing Roadmap.
 *
 * Tests the same round-trip invariants as Phase 4 (fuzz_workspace_placement.cpp)
 * but using real production EditorState subclasses instead of mock types.
 * This exercises actual serialization code paths with fuzz-generated state values.
 *
 * Currently covers CorpusLevel::Core (TestWidget, DataInspector, DataImport, DataTransform).
 * New widget types are added by extending WidgetCorpus.hpp/cpp.
 *
 * Requires QApplication with QT_QPA_PLATFORM=offscreen.
 */

#include "FuzzDockingHarness.hpp"
#include "MockEditorTypes.hpp"
#include "WidgetCorpus.hpp"

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QApplication>
#include <QSignalSpy>

#include <DockWidget.h>

#include "nlohmann/json.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "Test_Widget/TestWidgetState.hpp"
#include "DataInspector_Widget/DataInspectorState.hpp"
#include "DataImport_Widget/DataImportWidgetState.hpp"
#include "DataTransform_Widget/DataTransformWidgetState.hpp"
#include "DataViewer_Widget/Core/DataViewerState.hpp"
#include "Media_Widget/Core/MediaWidgetState.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

// ============================================================================
// Global QApplication setup
// ============================================================================

class QtWidgetTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "fuzz_real_widget_roundtrip";
            static char * argv[] = {arg0, nullptr};
            _app = std::make_unique<QApplication>(argc, argv);
        }
    }
    void TearDown() override {
        _app.reset();
    }

private:
    std::unique_ptr<QApplication> _app;
};

auto * const g_qt_env = ::testing::AddGlobalTestEnvironment(new QtWidgetTestEnvironment());

// ============================================================================
// Helper: compare JSON structurally (order-independent)
// ============================================================================

void ExpectJsonEqual(std::string const & json1, std::string const & json2) {
    auto parsed1 = nlohmann::json::parse(json1);
    auto parsed2 = nlohmann::json::parse(json2);
    EXPECT_EQ(parsed1, parsed2)
        << "JSON mismatch:\n  First:  " << json1.substr(0, 500)
        << "\n  Second: " << json2.substr(0, 500);
}

// ============================================================================
// Helper: extract sorted state info from a registry
// ============================================================================

struct StateInfo {
    std::string instance_id;
    std::string type_name;
    std::string display_name;

    bool operator==(StateInfo const &) const = default;
    auto operator<=>(StateInfo const &) const = default;
};

std::vector<StateInfo> extractStateInfo(EditorRegistry const & registry) {
    std::vector<StateInfo> infos;
    for (auto const & s : registry.allStates()) {
        infos.push_back({
            .instance_id = s->getInstanceId().toStdString(),
            .type_name = s->getTypeName().toStdString(),
            .display_name = s->getDisplayName().toStdString(),
        });
    }
    std::sort(infos.begin(), infos.end());
    return infos;
}

// ============================================================================
// Shared corpus state
// ============================================================================

static auto const kCorpusLevel = CorpusLevel::Media;

static std::vector<CorpusEntry> const & corpusEntriesRef() {
    static auto const entries = corpusEntries(kCorpusLevel);
    return entries;
}

static std::vector<std::string> corpusTypeIds() {
    std::vector<std::string> ids;
    for (auto const & e : corpusEntriesRef()) {
        ids.push_back(e.type_id);
    }
    return ids;
}

static std::vector<Zone> const kAllZones = {
    Zone::Left, Zone::Center, Zone::Right, Zone::Bottom};

// ============================================================================
// Helper: create a harness with corpus types registered
// ============================================================================

struct CorpusHarness {
    FuzzDockingHarness harness;
    std::shared_ptr<DataManager> dm;

    CorpusHarness() : dm(std::make_shared<DataManager>()) {
        registerCorpusTypes(harness.registry(), kCorpusLevel, dm);
    }
};

// ============================================================================
// Fuzz Tests
// ============================================================================

// ---- FuzzRealWidgetPlacement ----
// Create random real editors, serialize, restore, verify state round-trip.

void FuzzRealWidgetPlacement(
    std::vector<int> const & type_indices,
    float left_ratio,
    float center_ratio,
    float right_ratio,
    float bottom_ratio) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const & type_ids = corpusTypeIds();
    if (type_ids.empty()) {
        return;
    }

    // Apply random zone ratios
    harness.zoneManager()->setZoneWidthRatios(
        std::clamp(left_ratio, 0.05f, 0.5f),
        std::clamp(center_ratio, 0.1f, 0.8f),
        std::clamp(right_ratio, 0.05f, 0.5f));
    harness.zoneManager()->setBottomHeightRatio(
        std::clamp(bottom_ratio, 0.05f, 0.5f));

    std::set<std::string> created_single_types;
    int expected_count = 0;

    int const n = std::min(static_cast<int>(type_indices.size()), 20);
    for (int i = 0; i < n; ++i) {
        auto const & type_id = type_ids[
            static_cast<size_t>(std::abs(type_indices[i])) % type_ids.size()];

        auto const & entries = corpusEntriesRef();
        auto const it = std::find_if(entries.begin(), entries.end(),
            [&](auto const & e) { return e.type_id == type_id; });
        bool const allow_multiple = (it != entries.end()) && it->allow_multiple;

        if (!allow_multiple && created_single_types.contains(type_id)) {
            continue;
        }

        auto placed = harness.createAndPlace(type_id);
        if (placed.isValid()) {
            ++expected_count;
            if (!allow_multiple) {
                created_single_types.insert(type_id);
            }
        }
    }

    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), expected_count);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    // Restore into a fresh harness
    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);

    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzRealWidgetPlacement)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange(0, 10)).WithMaxSize(20),
        fuzztest::InRange(0.05f, 0.5f),
        fuzztest::InRange(0.1f, 0.8f),
        fuzztest::InRange(0.05f, 0.5f),
        fuzztest::InRange(0.05f, 0.5f));

// ---- FuzzRealWidgetWithZoneOverride ----
// Place real widgets in explicitly chosen zones.

void FuzzRealWidgetWithZoneOverride(
    std::vector<int> const & type_indices,
    std::vector<int> const & zone_choices) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const & type_ids = corpusTypeIds();
    if (type_ids.empty()) {
        return;
    }

    std::set<std::string> created_single_types;
    int expected_count = 0;

    int const n = std::min({
        static_cast<int>(type_indices.size()),
        static_cast<int>(zone_choices.size()),
        20});

    for (int i = 0; i < n; ++i) {
        auto const & type_id = type_ids[
            static_cast<size_t>(std::abs(type_indices[i])) % type_ids.size()];

        auto const & entries = corpusEntriesRef();
        auto const it = std::find_if(entries.begin(), entries.end(),
            [&](auto const & e) { return e.type_id == type_id; });
        bool const allow_multiple = (it != entries.end()) && it->allow_multiple;

        if (!allow_multiple && created_single_types.contains(type_id)) {
            continue;
        }

        Zone const zone = kAllZones[
            static_cast<size_t>(std::abs(zone_choices[i])) % kAllZones.size()];

        auto placed = harness.createAndPlaceInZone(type_id, zone);
        if (placed.isValid()) {
            ++expected_count;
            if (!allow_multiple) {
                created_single_types.insert(type_id);
            }
        }
    }

    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), expected_count);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);

    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzRealWidgetWithZoneOverride)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange(0, 10)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::InRange(0, 3)).WithMaxSize(20));

// ---- FuzzRealWidgetStateValues ----
// Create a TestWidget, randomize its state fields, then round-trip.
// This exercises the real TestWidgetState serialization with fuzz values.

void FuzzRealWidgetStateValues(
    bool show_grid,
    bool show_crosshair,
    bool enable_animation,
    double zoom_level,
    int grid_spacing,
    std::string const & label_text,
    std::string const & highlight_color) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("TestWidget");
    ASSERT_TRUE(placed.isValid());

    // Get the state and apply fuzz-generated values
    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * test_state = dynamic_cast<TestWidgetState *>(states[0].get());
    ASSERT_NE(test_state, nullptr);

    test_state->setShowGrid(show_grid);
    test_state->setShowCrosshair(show_crosshair);
    test_state->setEnableAnimation(enable_animation);
    test_state->setZoomLevel(zoom_level);     // State clamps to [0.1, 5.0]
    test_state->setGridSpacing(grid_spacing); // State clamps to [10, 200]
    test_state->setLabelText(QString::fromStdString(label_text));
    test_state->setHighlightColor(QString::fromStdString(highlight_color));

    // Capture state after clamping
    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    // Restore into a fresh harness
    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    // Verify state values round-tripped
    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * test_state2 = dynamic_cast<TestWidgetState *>(states2[0].get());
    ASSERT_NE(test_state2, nullptr);

    EXPECT_EQ(test_state2->showGrid(), test_state->showGrid());
    EXPECT_EQ(test_state2->showCrosshair(), test_state->showCrosshair());
    EXPECT_EQ(test_state2->enableAnimation(), test_state->enableAnimation());
    EXPECT_DOUBLE_EQ(test_state2->zoomLevel(), test_state->zoomLevel());
    EXPECT_EQ(test_state2->gridSpacing(), test_state->gridSpacing());
    EXPECT_EQ(test_state2->labelText(), test_state->labelText());
    EXPECT_EQ(test_state2->highlightColorHex(), test_state->highlightColorHex());

    // JSON idempotency
    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzRealWidgetStateValues)
    .WithDomains(
        fuzztest::Arbitrary<bool>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::Finite<double>(),
        fuzztest::Arbitrary<int>(),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(20));

// ---- FuzzRealWidgetDiversePlacement ----
// Exercises float, close-reopen actions on real widgets.

void FuzzRealWidgetDiversePlacement(
    std::vector<int> const & type_indices,
    std::vector<int> const & placement_actions) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const & type_ids = corpusTypeIds();
    if (type_ids.empty()) {
        return;
    }

    std::set<std::string> created_single_types;
    int expected_count = 0;

    int const n = std::min({
        static_cast<int>(type_indices.size()),
        static_cast<int>(placement_actions.size()),
        15});

    for (int i = 0; i < n; ++i) {
        auto const & type_id = type_ids[
            static_cast<size_t>(std::abs(type_indices[i])) % type_ids.size()];

        auto const & entries = corpusEntriesRef();
        auto const it = std::find_if(entries.begin(), entries.end(),
            [&](auto const & e) { return e.type_id == type_id; });
        bool const allow_multiple = (it != entries.end()) && it->allow_multiple;

        if (!allow_multiple && created_single_types.contains(type_id)) {
            continue;
        }

        auto placed = harness.createAndPlace(type_id);
        if (!placed.isValid()) {
            continue;
        }

        ++expected_count;
        if (!allow_multiple) {
            created_single_types.insert(type_id);
        }

        int const action = std::abs(placement_actions[i]) % 3;
        switch (action) {
        case 1: // Float
            if (placed.view_dock) {
                placed.view_dock->setFloating();
            }
            break;
        case 2: // Close and re-open
            // Closing may remove the state from the registry for some widget
            // types (e.g., if the controller's cleanup signal fires). This is
            // expected behavior — adjust expected_count accordingly.
            if (placed.view_dock) {
                auto const count_before = harness.registry()->stateCount();
                placed.view_dock->closeDockWidget();
                placed.view_dock->toggleView(true);
                auto const count_after = harness.registry()->stateCount();
                if (count_after < count_before) {
                    // State was removed on close — adjust expectation
                    expected_count -= static_cast<int>(count_before - count_after);
                }
            }
            break;
        default: // 0 = Tab (default placement)
            break;
        }
    }

    // State count should match what we expect after accounting for close removals
    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), expected_count);

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzRealWidgetDiversePlacement)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange(0, 10)).WithMaxSize(15),
        fuzztest::VectorOf(fuzztest::InRange(0, 10)).WithMaxSize(15));

// ---- FuzzRealWidgetFromGarbageJson ----
// Create a TestWidget, then attempt to restore from garbage JSON.
// The real EditorState::fromJson() must not crash.

void FuzzRealWidgetFromGarbageJson(std::string const & garbage) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("TestWidget");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    // Save original state
    auto const original_json = states[0]->toJson();
    auto const original_instance_id = states[0]->getInstanceId();

    // Attempt to load garbage — should return false and not crash
    bool const loaded = states[0]->fromJson(garbage);
    if (!loaded) {
        // Failed load should preserve instance ID
        EXPECT_EQ(states[0]->getInstanceId(), original_instance_id);
    }
    // Either way, no crash is the primary success criterion
}

FUZZ_TEST(RealWidgetFuzz, FuzzRealWidgetFromGarbageJson)
    .WithDomains(fuzztest::Arbitrary<std::string>());

// ============================================================================
// Deterministic Tests
// ============================================================================

// ---- TestWidgetRoundTrip ----
// Basic deterministic round-trip with default TestWidget state.

TEST(RealWidgetDeterministic, TestWidgetRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("TestWidget");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

// ---- TestWidgetStateFieldPreservation ----
// Set specific values and verify all fields survive round-trip.

TEST(RealWidgetDeterministic, TestWidgetStateFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("TestWidget");
    ASSERT_TRUE(placed.isValid());

    auto * test_state = dynamic_cast<TestWidgetState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(test_state, nullptr);

    // Set non-default values
    test_state->setShowGrid(false);
    test_state->setShowCrosshair(true);
    test_state->setEnableAnimation(true);
    test_state->setZoomLevel(2.5);
    test_state->setGridSpacing(75);
    test_state->setLabelText("Custom Label");
    test_state->setHighlightColor(QString("#00FF00"));

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * test_state2 = dynamic_cast<TestWidgetState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(test_state2, nullptr);

    EXPECT_EQ(test_state2->showGrid(), false);
    EXPECT_EQ(test_state2->showCrosshair(), true);
    EXPECT_EQ(test_state2->enableAnimation(), true);
    EXPECT_DOUBLE_EQ(test_state2->zoomLevel(), 2.5);
    EXPECT_EQ(test_state2->gridSpacing(), 75);
    EXPECT_EQ(test_state2->labelText(), "Custom Label");
    // QColor::name() normalizes to lowercase hex
    EXPECT_EQ(test_state2->highlightColorHex(), test_state->highlightColorHex());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

// ---- TestWidgetTripleRoundTrip ----
// Serialize → restore → serialize → restore → serialize, compare all three.

TEST(RealWidgetDeterministic, TestWidgetTripleRoundTrip) {
    CorpusHarness ch1;
    ch1.harness.createAndPlace("TestWidget");

    auto * state1 = dynamic_cast<TestWidgetState *>(
        ch1.harness.registry()->allStates()[0].get());
    state1->setShowGrid(false);
    state1->setZoomLevel(3.14);
    state1->setLabelText("Triple Test");

    auto const json1 = ch1.harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));
    auto const json2 = ch2.harness.captureState();

    CorpusHarness ch3;
    ASSERT_TRUE(ch3.harness.restoreState(json2));
    auto const json3 = ch3.harness.captureState();

    ExpectJsonEqual(json1, json2);
    ExpectJsonEqual(json2, json3);
}

// ---- TestWidgetSignalEmission ----
// Verify stateChanged() fires on fromJson().

TEST(RealWidgetDeterministic, TestWidgetSignalEmission) {
    auto dm = std::make_shared<DataManager>();
    auto state = std::make_shared<TestWidgetState>(dm);

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    // Serialize default, then restore — should emit stateChanged
    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

// ---- TestWidgetWithMocksMixed ----
// Real TestWidget alongside mock types in the same registry.

TEST(RealWidgetDeterministic, TestWidgetWithMocksMixed) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    // Also register mock types
    registerMockTypes(harness.registry(), 3, /*with_widget_factories=*/true);

    // Create one of each
    harness.createAndPlace("TestWidget");
    harness.createAndPlace("MockTypeA");
    harness.createAndPlace("MockTypeB");
    harness.createAndPlace("MockTypeC");

    EXPECT_EQ(harness.registry()->stateCount(), 4u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    // Restore into fresh harness with both real and mock types
    CorpusHarness ch2;
    registerMockTypes(ch2.harness.registry(), 3, /*with_widget_factories=*/true);
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 4u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

// ---- TestWidgetFromInvalidJson ----
// fromJson with invalid data should return false and not crash.

TEST(RealWidgetDeterministic, TestWidgetFromInvalidJson) {
    auto dm = std::make_shared<DataManager>();
    auto state = std::make_shared<TestWidgetState>(dm);

    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    // Instance ID should be preserved after failed loads
    EXPECT_EQ(state->getInstanceId(), original_id);
}

// ---- EmptyCorpusRoundTrip ----
// No editors created, just verify empty workspace round-trips cleanly.

TEST(RealWidgetDeterministic, EmptyCorpusRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    EXPECT_EQ(harness.registry()->stateCount(), 0u);

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));
    EXPECT_EQ(ch2.harness.registry()->stateCount(), 0u);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

// ---- TestWidgetInAllZones ----
// Place TestWidget in each zone and verify round-trip.

TEST(RealWidgetDeterministic, TestWidgetInEachZone) {
    for (auto const zone : kAllZones) {
        CorpusHarness ch;
        auto & harness = ch.harness;

        auto placed = harness.createAndPlaceInZone("TestWidget", zone);
        ASSERT_TRUE(placed.isValid()) << "Failed to place in zone " << static_cast<int>(zone);

        EXPECT_EQ(harness.registry()->stateCount(), 1u);
        EXPECT_TRUE(harness.verifyAllDocked());

        auto const json1 = harness.captureState();

        CorpusHarness ch2;
        ASSERT_TRUE(ch2.harness.restoreState(json1));

        EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
        EXPECT_TRUE(ch2.harness.verifyAllDocked());

        auto const json2 = ch2.harness.captureState();
        ExpectJsonEqual(json1, json2);
    }
}

// ============================================================================
// Fuzz Tests — Core Level (DataInspector, DataImport, DataTransform)
// ============================================================================

// ---- FuzzDataInspectorStateValues ----
void FuzzDataInspectorStateValues(
    std::string const & inspected_key,
    bool is_pinned,
    std::string const & section1,
    std::string const & section2,
    std::string const & ui_json_value) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataInspector");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * state = dynamic_cast<DataInspectorState *>(states[0].get());
    ASSERT_NE(state, nullptr);

    state->setInspectedDataKey(QString::fromStdString(inspected_key));
    state->setPinned(is_pinned);
    if (!section1.empty()) {
        state->setSectionCollapsed(QString::fromStdString(section1), true);
    }
    if (!section2.empty()) {
        state->setSectionCollapsed(QString::fromStdString(section2), true);
    }

    // Only set ui_state_json if it parses as valid JSON
    try {
        auto parsed = nlohmann::json::parse(ui_json_value);
        state->setTypeSpecificState(parsed);
    } catch (...) {
        // Invalid JSON — skip, don't set
    }

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * state2 = dynamic_cast<DataInspectorState *>(states2[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->inspectedDataKey(), state->inspectedDataKey());
    EXPECT_EQ(state2->isPinned(), state->isPinned());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzDataInspectorStateValues)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Arbitrary<bool>(),
        fuzztest::PrintableAsciiString().WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(200));

// ---- FuzzDataImportStateValues ----
void FuzzDataImportStateValues(
    std::string const & import_type,
    std::string const & last_dir,
    std::string const & pref_key,
    std::string const & pref_value) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataImportWidget");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * state = dynamic_cast<DataImportWidgetState *>(states[0].get());
    ASSERT_NE(state, nullptr);

    state->setSelectedImportType(QString::fromStdString(import_type));
    state->setLastUsedDirectory(QString::fromStdString(last_dir));
    if (!pref_key.empty()) {
        state->setFormatPreference(
            QString::fromStdString(pref_key),
            QString::fromStdString(pref_value));
    }

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * state2 = dynamic_cast<DataImportWidgetState *>(states2[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->selectedImportType(), state->selectedImportType());
    EXPECT_EQ(state2->lastUsedDirectory(), state->lastUsedDirectory());
    if (!pref_key.empty()) {
        EXPECT_EQ(state2->formatPreference(QString::fromStdString(pref_key)),
                  state->formatPreference(QString::fromStdString(pref_key)));
    }

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzDataImportStateValues)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::PrintableAsciiString().WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(50));

// ---- FuzzDataTransformStateValues ----
void FuzzDataTransformStateValues(
    std::string const & input_key,
    std::string const & operation,
    std::string const & output_name) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataTransformWidget");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * state = dynamic_cast<DataTransformWidgetState *>(states[0].get());
    ASSERT_NE(state, nullptr);

    state->setSelectedInputDataKey(QString::fromStdString(input_key));
    state->setSelectedOperation(QString::fromStdString(operation));
    state->setLastOutputName(QString::fromStdString(output_name));

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * state2 = dynamic_cast<DataTransformWidgetState *>(states2[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->selectedInputDataKey(), state->selectedInputDataKey());
    EXPECT_EQ(state2->selectedOperation(), state->selectedOperation());
    EXPECT_EQ(state2->lastOutputName(), state->lastOutputName());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzDataTransformStateValues)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(100));

// ---- FuzzMultipleDataInspectors ----
// DataInspector allows multiple instances — stress test that.
void FuzzMultipleDataInspectors(int count) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const n = std::clamp(count, 0, 10);
    for (int i = 0; i < n; ++i) {
        auto placed = harness.createAndPlace("DataInspector");
        ASSERT_TRUE(placed.isValid()) << "Failed to create inspector " << i;
    }

    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), n);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzMultipleDataInspectors)
    .WithDomains(fuzztest::InRange(0, 10));

// ---- FuzzDataViewerStateValues ----
void FuzzDataViewerStateValues(
    int64_t time_start,
    int64_t time_end,
    float y_min,
    float y_max,
    float global_zoom,
    bool grid_enabled,
    int grid_spacing,
    std::string const & bg_color,
    std::string const & analog_key,
    std::string const & analog_color) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataViewerWidget");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * state = dynamic_cast<DataViewerState *>(states[0].get());
    ASSERT_NE(state, nullptr);

    // Set view state
    state->setTimeWindow(time_start, time_end);
    if (std::isfinite(y_min) && std::isfinite(y_max)) {
        state->setYBounds(y_min, y_max);
    }
    if (std::isfinite(global_zoom) && global_zoom > 0.0f) {
        state->setGlobalZoom(global_zoom);
    }

    // Set grid
    state->setGridEnabled(grid_enabled);
    state->setGridSpacing(grid_spacing);

    // Set background color
    if (!bg_color.empty()) {
        state->setBackgroundColor(QString::fromStdString(bg_color));
    }

    // Set analog series options via registry
    if (!analog_key.empty()) {
        AnalogSeriesOptionsData opts;
        opts.hex_color() = analog_color.empty() ? "#ff0000" : analog_color;
        opts.user_scale_factor = 1.0f;
        state->seriesOptions().set(QString::fromStdString(analog_key), opts);
    }

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * state2 = dynamic_cast<DataViewerState *>(states2[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->timeWindow(), state->timeWindow());
    EXPECT_EQ(state2->gridEnabled(), state->gridEnabled());
    EXPECT_EQ(state2->gridSpacing(), state->gridSpacing());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzDataViewerStateValues)
    .WithDomains(
        fuzztest::Arbitrary<int64_t>(),
        fuzztest::Arbitrary<int64_t>(),
        fuzztest::Finite<float>(),
        fuzztest::Finite<float>(),
        fuzztest::Finite<float>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::InRange(1, 10000),
        fuzztest::PrintableAsciiString().WithMaxSize(20),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(20));

// ---- FuzzMultipleDataViewers ----
// DataViewerWidget allows multiple instances — stress test that.
void FuzzMultipleDataViewers(int count) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const n = std::clamp(count, 0, 10);
    for (int i = 0; i < n; ++i) {
        auto placed = harness.createAndPlace("DataViewerWidget");
        ASSERT_TRUE(placed.isValid()) << "Failed to create viewer " << i;
    }

    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), n);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzMultipleDataViewers)
    .WithDomains(fuzztest::InRange(0, 10));

// ---- FuzzMediaWidgetStateValues ----
void FuzzMediaWidgetStateValues(
    std::string const & media_key,
    double zoom,
    double pan_x,
    double pan_y,
    int canvas_w,
    int canvas_h,
    std::string const & line_key,
    std::string const & line_color,
    int line_thickness,
    int brush_size,
    float selection_threshold,
    int tool_mode_idx) {

    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("MediaWidget");
    ASSERT_TRUE(placed.isValid());

    auto const & states = harness.registry()->allStates();
    ASSERT_EQ(states.size(), 1u);

    auto * state = dynamic_cast<MediaWidgetState *>(states[0].get());
    ASSERT_NE(state, nullptr);

    // Set displayed data key
    if (!media_key.empty()) {
        state->setDisplayedDataKey(QString::fromStdString(media_key));
    }

    // Set viewport
    if (std::isfinite(zoom) && zoom > 0.0) {
        state->setZoom(zoom);
    }
    if (std::isfinite(pan_x) && std::isfinite(pan_y)) {
        state->setPan(pan_x, pan_y);
    }
    if (canvas_w > 0 && canvas_h > 0) {
        state->setCanvasSize(canvas_w, canvas_h);
    }

    // Set line display options via registry
    if (!line_key.empty()) {
        LineDisplayOptions line_opts;
        line_opts.hex_color() = line_color.empty() ? "#ff0000" : line_color;
        line_opts.line_thickness = std::clamp(line_thickness, 1, 20);
        state->displayOptions().set(QString::fromStdString(line_key), line_opts);
    }

    // Set interaction preferences
    MaskInteractionPrefs mask_prefs;
    mask_prefs.brush_size = std::clamp(brush_size, 1, 200);
    state->setMaskPrefs(mask_prefs);

    PointInteractionPrefs point_prefs;
    if (std::isfinite(selection_threshold) && selection_threshold > 0.0f) {
        point_prefs.selection_threshold = selection_threshold;
    }
    state->setPointPrefs(point_prefs);

    // Set tool mode
    auto const line_mode = static_cast<LineToolMode>(
        std::abs(tool_mode_idx) % 5);
    state->setActiveLineMode(line_mode);

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const & states2 = ch2.harness.registry()->allStates();
    ASSERT_EQ(states2.size(), 1u);

    auto * state2 = dynamic_cast<MediaWidgetState *>(states2[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->displayedDataKey(), state->displayedDataKey());
    EXPECT_EQ(state2->activeLineMode(), state->activeLineMode());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzMediaWidgetStateValues)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::Finite<double>(),
        fuzztest::InRange(1, 8192),
        fuzztest::InRange(1, 8192),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(20),
        fuzztest::InRange(1, 20),
        fuzztest::InRange(1, 200),
        fuzztest::Finite<float>(),
        fuzztest::InRange(0, 4));

// ---- FuzzMultipleMediaWidgets ----
// MediaWidget allows multiple instances — stress test that.
void FuzzMultipleMediaWidgets(int count) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto const n = std::clamp(count, 0, 10);
    for (int i = 0; i < n; ++i) {
        auto placed = harness.createAndPlace("MediaWidget");
        ASSERT_TRUE(placed.isValid()) << "Failed to create media widget " << i;
    }

    EXPECT_EQ(static_cast<int>(harness.registry()->stateCount()), n);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();
    ASSERT_FALSE(json1.empty());

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), harness.registry()->stateCount());
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

FUZZ_TEST(RealWidgetFuzz, FuzzMultipleMediaWidgets)
    .WithDomains(fuzztest::InRange(0, 10));

// ============================================================================
// Deterministic Tests — Core Level
// ============================================================================

TEST(RealWidgetDeterministic, DataInspectorRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataInspector");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataInspectorFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataInspector");
    ASSERT_TRUE(placed.isValid());

    auto * state = dynamic_cast<DataInspectorState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(state, nullptr);

    state->setInspectedDataKey("my_analog_data");
    state->setPinned(true);
    state->setSectionCollapsed("properties", true);
    state->setSectionCollapsed("export", true);

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * state2 = dynamic_cast<DataInspectorState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->inspectedDataKey(), "my_analog_data");
    EXPECT_TRUE(state2->isPinned());
    EXPECT_TRUE(state2->isSectionCollapsed("properties"));
    EXPECT_TRUE(state2->isSectionCollapsed("export"));
    EXPECT_FALSE(state2->isSectionCollapsed("other"));

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataImportRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataImportWidget");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataImportFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataImportWidget");
    ASSERT_TRUE(placed.isValid());

    auto * state = dynamic_cast<DataImportWidgetState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(state, nullptr);

    state->setSelectedImportType("LineData");
    state->setLastUsedDirectory("/home/user/data");
    state->setFormatPreference("LineData", "CSV");
    state->setFormatPreference("MaskData", "HDF5");

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * state2 = dynamic_cast<DataImportWidgetState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->selectedImportType(), "LineData");
    EXPECT_EQ(state2->lastUsedDirectory(), "/home/user/data");
    EXPECT_EQ(state2->formatPreference("LineData"), "CSV");
    EXPECT_EQ(state2->formatPreference("MaskData"), "HDF5");
    EXPECT_EQ(state2->formatPreference("Unknown"), "");

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataTransformRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataTransformWidget");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataTransformFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataTransformWidget");
    ASSERT_TRUE(placed.isValid());

    auto * state = dynamic_cast<DataTransformWidgetState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(state, nullptr);

    state->setSelectedInputDataKey("my_mask_data");
    state->setSelectedOperation("Calculate Area");
    state->setLastOutputName("mask_area_result");

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * state2 = dynamic_cast<DataTransformWidgetState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->selectedInputDataKey(), "my_mask_data");
    EXPECT_EQ(state2->selectedOperation(), "Calculate Area");
    EXPECT_EQ(state2->lastOutputName(), "mask_area_result");

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataInspectorSignalEmission) {
    auto state = std::make_shared<DataInspectorState>();

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

TEST(RealWidgetDeterministic, DataImportSignalEmission) {
    auto state = std::make_shared<DataImportWidgetState>();

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

TEST(RealWidgetDeterministic, DataTransformSignalEmission) {
    auto state = std::make_shared<DataTransformWidgetState>();

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

TEST(RealWidgetDeterministic, DataInspectorFromInvalidJson) {
    auto state = std::make_shared<DataInspectorState>();
    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    EXPECT_EQ(state->getInstanceId(), original_id);
}

TEST(RealWidgetDeterministic, DataImportFromInvalidJson) {
    auto state = std::make_shared<DataImportWidgetState>();
    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    EXPECT_EQ(state->getInstanceId(), original_id);
}

TEST(RealWidgetDeterministic, DataTransformFromInvalidJson) {
    auto state = std::make_shared<DataTransformWidgetState>();
    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    EXPECT_EQ(state->getInstanceId(), original_id);
}

// ============================================================================
// Deterministic Tests — Visualization Level (DataViewer)
// ============================================================================

TEST(RealWidgetDeterministic, DataViewerRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataViewerWidget");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataViewerFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("DataViewerWidget");
    ASSERT_TRUE(placed.isValid());

    auto * state = dynamic_cast<DataViewerState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(state, nullptr);

    state->setTimeWindow(100, 5000);
    state->setYBounds(-2.0f, 2.0f);
    state->setGlobalZoom(1.5f);
    state->setGridEnabled(true);
    state->setGridSpacing(250);
    state->setTheme(DataViewerTheme::Light);
    state->setBackgroundColor("#ffffff");
    state->setAxisColor("#333333");
    state->setZoomScalingMode(DataViewerZoomScalingMode::Fixed);
    state->setInteractionMode(DataViewerInteractionMode::CreateInterval);

    // Add series options
    AnalogSeriesOptionsData analog_opts;
    analog_opts.hex_color() = "#0000ff";
    analog_opts.user_scale_factor = 2.5f;
    analog_opts.y_offset = 0.1f;
    state->seriesOptions().set("channel_1", analog_opts);

    DigitalEventSeriesOptionsData event_opts;
    event_opts.hex_color() = "#ff0000";
    event_opts.plotting_mode = EventPlottingModeData::Stacked;
    state->seriesOptions().set("spikes", event_opts);

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * state2 = dynamic_cast<DataViewerState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(state2, nullptr);

    EXPECT_EQ(state2->timeWindow(), std::make_pair(int64_t{100}, int64_t{5000}));
    EXPECT_FLOAT_EQ(state2->yBounds().first, -2.0f);
    EXPECT_FLOAT_EQ(state2->yBounds().second, 2.0f);
    EXPECT_TRUE(state2->gridEnabled());
    EXPECT_EQ(state2->gridSpacing(), 250);

    // Verify series options survived
    auto const * restored_analog = state2->seriesOptions().get<AnalogSeriesOptionsData>("channel_1");
    ASSERT_NE(restored_analog, nullptr);
    EXPECT_EQ(restored_analog->hex_color(), "#0000ff");
    EXPECT_FLOAT_EQ(restored_analog->user_scale_factor, 2.5f);

    auto const * restored_events = state2->seriesOptions().get<DigitalEventSeriesOptionsData>("spikes");
    ASSERT_NE(restored_events, nullptr);
    EXPECT_EQ(restored_events->hex_color(), "#ff0000");
    EXPECT_EQ(restored_events->plotting_mode, EventPlottingModeData::Stacked);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, DataViewerSignalEmission) {
    auto state = std::make_shared<DataViewerState>();

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

TEST(RealWidgetDeterministic, DataViewerFromInvalidJson) {
    auto state = std::make_shared<DataViewerState>();
    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    EXPECT_EQ(state->getInstanceId(), original_id);
}

// ============================================================================
// Deterministic Tests — Media Level (MediaWidget)
// ============================================================================

TEST(RealWidgetDeterministic, MediaWidgetRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("MediaWidget");
    ASSERT_TRUE(placed.isValid());

    EXPECT_EQ(harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 1u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, MediaWidgetFieldPreservation) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    auto placed = harness.createAndPlace("MediaWidget");
    ASSERT_TRUE(placed.isValid());

    auto * state = dynamic_cast<MediaWidgetState *>(
        harness.registry()->allStates()[0].get());
    ASSERT_NE(state, nullptr);

    // Set displayed data key
    state->setDisplayedDataKey("test_video.mp4");

    // Set viewport
    state->setZoom(2.5);
    state->setPan(100.0, -50.0);
    state->setCanvasSize(1920, 1080);

    // Set line display options
    LineDisplayOptions line_opts;
    line_opts.hex_color() = "#00ff00";
    line_opts.line_thickness = 5;
    state->displayOptions().set("whisker_1", line_opts);

    // Set mask display options
    MaskDisplayOptions mask_opts;
    mask_opts.hex_color() = "#0000ff";
    mask_opts.alpha() = 0.5f;
    state->displayOptions().set("tongue_mask", mask_opts);

    // Set interaction preferences
    LineInteractionPrefs line_prefs;
    line_prefs.polynomial_order = 5;
    line_prefs.edge_snapping_enabled = true;
    line_prefs.eraser_radius = 20;
    state->setLinePrefs(line_prefs);

    MaskInteractionPrefs mask_prefs;
    mask_prefs.brush_size = 30;
    mask_prefs.hover_circle_visible = false;
    state->setMaskPrefs(mask_prefs);

    // Set tool modes
    state->setActiveLineMode(LineToolMode::Add);
    state->setActiveMaskMode(MaskToolMode::Brush);
    state->setActivePointMode(PointToolMode::Select);

    // Add text overlay
    TextOverlayData overlay;
    overlay.text = "Frame: 100";
    overlay.x_position = 0.1f;
    overlay.y_position = 0.9f;
    overlay.color = "#ffffff";
    overlay.font_size = 16;
    state->addTextOverlay(overlay);

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    auto * state2 = dynamic_cast<MediaWidgetState *>(
        ch2.harness.registry()->allStates()[0].get());
    ASSERT_NE(state2, nullptr);

    // Verify primary display
    EXPECT_EQ(state2->displayedDataKey(), "test_video.mp4");

    // Verify viewport
    EXPECT_DOUBLE_EQ(state2->zoom(), 2.5);
    auto const [px, py] = state2->pan();
    EXPECT_DOUBLE_EQ(px, 100.0);
    EXPECT_DOUBLE_EQ(py, -50.0);
    auto const [cw, ch_val] = state2->canvasSize();
    EXPECT_EQ(cw, 1920);
    EXPECT_EQ(ch_val, 1080);

    // Verify line display options
    auto const * restored_line = state2->displayOptions().get<LineDisplayOptions>("whisker_1");
    ASSERT_NE(restored_line, nullptr);
    EXPECT_EQ(restored_line->hex_color(), "#00ff00");
    EXPECT_EQ(restored_line->line_thickness, 5);

    // Verify mask display options
    auto const * restored_mask = state2->displayOptions().get<MaskDisplayOptions>("tongue_mask");
    ASSERT_NE(restored_mask, nullptr);
    EXPECT_EQ(restored_mask->hex_color(), "#0000ff");
    EXPECT_FLOAT_EQ(restored_mask->alpha(), 0.5f);

    // Verify interaction preferences
    EXPECT_EQ(state2->linePrefs().polynomial_order, 5);
    EXPECT_TRUE(state2->linePrefs().edge_snapping_enabled);
    EXPECT_EQ(state2->linePrefs().eraser_radius, 20);
    EXPECT_EQ(state2->maskPrefs().brush_size, 30);
    EXPECT_FALSE(state2->maskPrefs().hover_circle_visible);

    // Verify tool modes
    EXPECT_EQ(state2->activeLineMode(), LineToolMode::Add);
    EXPECT_EQ(state2->activeMaskMode(), MaskToolMode::Brush);
    EXPECT_EQ(state2->activePointMode(), PointToolMode::Select);

    // Verify text overlays
    ASSERT_EQ(state2->textOverlays().size(), 1u);
    EXPECT_EQ(state2->textOverlays()[0].text, "Frame: 100");
    EXPECT_FLOAT_EQ(state2->textOverlays()[0].x_position, 0.1f);
    EXPECT_FLOAT_EQ(state2->textOverlays()[0].y_position, 0.9f);
    EXPECT_EQ(state2->textOverlays()[0].font_size, 16);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

TEST(RealWidgetDeterministic, MediaWidgetSignalEmission) {
    auto state = std::make_shared<MediaWidgetState>();

    QSignalSpy spy(state.get(), &EditorState::stateChanged);
    ASSERT_TRUE(spy.isValid());

    auto const json = state->toJson();
    bool const ok = state->fromJson(json);
    ASSERT_TRUE(ok);
    EXPECT_GE(spy.count(), 1);
}

TEST(RealWidgetDeterministic, MediaWidgetFromInvalidJson) {
    auto state = std::make_shared<MediaWidgetState>();
    auto const original_id = state->getInstanceId();

    EXPECT_FALSE(state->fromJson(""));
    EXPECT_FALSE(state->fromJson("not json"));
    EXPECT_FALSE(state->fromJson("null"));
    EXPECT_FALSE(state->fromJson("[]"));
    EXPECT_FALSE(state->fromJson("42"));

    EXPECT_EQ(state->getInstanceId(), original_id);
}

TEST(RealWidgetDeterministic, AllCoreWidgetsRoundTrip) {
    CorpusHarness ch;
    auto & harness = ch.harness;

    harness.createAndPlace("TestWidget");
    harness.createAndPlace("DataInspector");
    harness.createAndPlace("DataImportWidget");
    harness.createAndPlace("DataTransformWidget");
    harness.createAndPlace("DataViewerWidget");
    harness.createAndPlace("MediaWidget");

    EXPECT_EQ(harness.registry()->stateCount(), 6u);
    EXPECT_TRUE(harness.verifyAllDocked());

    auto const json1 = harness.captureState();

    CorpusHarness ch2;
    ASSERT_TRUE(ch2.harness.restoreState(json1));

    EXPECT_EQ(ch2.harness.registry()->stateCount(), 6u);
    EXPECT_TRUE(ch2.harness.verifyAllDocked());

    auto const infos1 = extractStateInfo(*harness.registry());
    auto const infos2 = extractStateInfo(*ch2.harness.registry());
    EXPECT_EQ(infos1, infos2);

    auto const json2 = ch2.harness.captureState();
    ExpectJsonEqual(json1, json2);
}

} // namespace
