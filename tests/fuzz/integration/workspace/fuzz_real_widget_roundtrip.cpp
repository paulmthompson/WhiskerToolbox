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
 * Currently covers CorpusLevel::Minimal (TestWidget only).
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

static auto const kCorpusLevel = CorpusLevel::Minimal;

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

} // namespace
