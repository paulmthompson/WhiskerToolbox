/**
 * @file fuzz_registry_roundtrip.cpp
 * @brief Fuzz tests for EditorRegistry::toJson() / fromJson() round-trips
 *
 * Tests the registry serialization layer with random combinations of 1..N
 * editors of different types. Verifies:
 * - JSON idempotency: toJson() → fromJson() → toJson() produces identical JSON
 * - State count preservation across round-trips
 * - Instance ID preservation for all states
 * - Type name preservation for all states
 * - Selection context preservation (primary + multi-select)
 * - EditorRegistry handles mixed single/multi-instance types correctly
 *
 * Phase 3 of the Workspace Fuzz Testing Roadmap.
 *
 * Uses mock editor types (MockEditorStateA/B/C) to avoid heavy widget
 * dependencies while exercising the full registry serialization path.
 */

#include "MockEditorTypes.hpp"

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include "nlohmann/json.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"
#include "EditorState/SelectionContext.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

// ============================================================================
// Global Qt application setup
// ============================================================================

class QtTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "fuzz_registry_roundtrip";
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
// Helper: extract state info from a registry for comparison
// ============================================================================

struct StateInfo {
    std::string instance_id;
    std::string type_name;
    std::string display_name;
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
    // Sort by instance_id for deterministic comparison
    std::sort(infos.begin(), infos.end(),
              [](auto const & a, auto const & b) { return a.instance_id < b.instance_id; });
    return infos;
}

// ============================================================================
// Helper: map type indices to mock type IDs
// ============================================================================

static constexpr int NUM_MOCK_TYPES = 3;

EditorTypeId mockTypeId(int index) {
    switch (index % NUM_MOCK_TYPES) {
        case 0: return EditorTypeId("MockTypeA");
        case 1: return EditorTypeId("MockTypeB");
        case 2: return EditorTypeId("MockTypeC");
        default: return EditorTypeId("MockTypeA");
    }
}

// ============================================================================
// 1. Basic registry round-trip with random editor subset
// ============================================================================

void FuzzRegistryRoundTrip(
    std::vector<int> const & type_indices,
    std::vector<std::string> const & display_names)
{
    EditorRegistry registry;
    registerMockTypes(&registry);

    // Create random subset of editors (limit to 20 for performance)
    auto const n = std::min({type_indices.size(), display_names.size(), size_t(20)});

    for (size_t i = 0; i < n; ++i) {
        auto type_id = mockTypeId(type_indices[i]);

        // Skip if type doesn't allow multiple and one already exists
        auto const & info = registry.typeInfo(type_id);
        if (!info.allow_multiple && !registry.statesByType(type_id).empty()) {
            continue;
        }

        auto state = registry.createState(type_id);
        if (!state) continue;

        state->setDisplayName(QString::fromStdString(display_names[i]));
        registry.registerState(state);
    }

    if (registry.stateCount() == 0) {
        return; // Nothing to test
    }

    // Capture state info before serialization
    auto infos_before = extractStateInfo(registry);
    auto json1 = registry.toJson();

    // Create fresh registry with same types
    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1))
        << "fromJson failed on first round-trip";

    // Verify state count preserved
    EXPECT_EQ(registry.stateCount(), registry2.stateCount())
        << "State count changed after round-trip";

    // Verify all states preserved
    auto infos_after = extractStateInfo(registry2);
    ASSERT_EQ(infos_before.size(), infos_after.size());
    for (size_t i = 0; i < infos_before.size(); ++i) {
        EXPECT_EQ(infos_before[i].instance_id, infos_after[i].instance_id)
            << "Instance ID mismatch at index " << i;
        EXPECT_EQ(infos_before[i].type_name, infos_after[i].type_name)
            << "Type name mismatch at index " << i;
        EXPECT_EQ(infos_before[i].display_name, infos_after[i].display_name)
            << "Display name mismatch at index " << i;
    }

    // Verify JSON idempotency
    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}
FUZZ_TEST(RegistryRoundTrip, FuzzRegistryRoundTrip)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::InRange(0, 10)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(100)).WithMaxSize(20));

// ============================================================================
// 2. Registry round-trip with randomized state field values
// ============================================================================

void FuzzRegistryWithRandomizedFields(
    int int_val,
    double double_val,
    bool bool_val,
    std::string const & string_val,
    std::vector<std::string> const & list_val)
{
    EditorRegistry registry;
    registerMockTypes(&registry);

    // Create one of each type with randomized fields
    {
        auto state = std::make_shared<MockEditorStateA>();
        state->setIntField(int_val);
        state->setDoubleField(double_val);
        state->setBoolField(bool_val);
        state->setDisplayName("StateA");
        registry.registerState(state);
    }
    {
        auto state = std::make_shared<MockEditorStateB>();
        state->setStringField(string_val);
        state->setListField(list_val);
        state->setDisplayName("StateB");
        registry.registerState(state);
    }
    {
        auto state = std::make_shared<MockEditorStateC>();
        state->setNested({double_val, double_val, string_val});
        state->setDisplayName("StateC");
        registry.registerState(state);
    }

    auto json1 = registry.toJson();

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    EXPECT_EQ(registry2.stateCount(), 3u);

    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);

    // Verify specific field values survived the round-trip
    auto states_a = registry2.statesByType(EditorTypeId("MockTypeA"));
    ASSERT_EQ(states_a.size(), 1u);
    auto restored_a = std::dynamic_pointer_cast<MockEditorStateA>(states_a[0]);
    ASSERT_NE(restored_a, nullptr);
    EXPECT_EQ(restored_a->intField(), int_val);
    EXPECT_EQ(restored_a->boolField(), bool_val);

    auto states_b = registry2.statesByType(EditorTypeId("MockTypeB"));
    ASSERT_EQ(states_b.size(), 1u);
    auto restored_b = std::dynamic_pointer_cast<MockEditorStateB>(states_b[0]);
    ASSERT_NE(restored_b, nullptr);
    EXPECT_EQ(restored_b->stringField(), string_val);
    EXPECT_EQ(restored_b->listField(), list_val);
}
FUZZ_TEST(RegistryRoundTrip, FuzzRegistryWithRandomizedFields)
    .WithDomains(
        fuzztest::Arbitrary<int>(),
        fuzztest::Finite<double>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::PrintableAsciiString().WithMaxSize(200),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10));

// ============================================================================
// 3. Registry round-trip with selection context
//
// Verifies primary selection and multi-select are both preserved across
// round-trips. Previously, setSelectedData() was called last, clearing
// selections added by addToSelection(). Fixed by reversing the restore order.
// ============================================================================

void FuzzRegistryWithSelectionContext(
    std::string const & primary_key,
    std::vector<std::string> const & extra_keys)
{
    EditorRegistry registry;
    registerMockTypes(&registry);

    // Create a few editors
    auto state_a = std::make_shared<MockEditorStateA>();
    registry.registerState(state_a);
    auto state_b = std::make_shared<MockEditorStateB>();
    registry.registerState(state_b);

    // Set selection context
    SelectionSource source{EditorInstanceId("test"), "fuzz"};
    if (!primary_key.empty()) {
        registry.selectionContext()->setSelectedData(
            SelectedDataKey(QString::fromStdString(primary_key)), source);
    }
    for (auto const & key : extra_keys) {
        if (!key.empty()) {
            registry.selectionContext()->addToSelection(
                SelectedDataKey(QString::fromStdString(key)), source);
        }
    }

    // Capture selection state before round-trip
    auto all_before = registry.selectionContext()->allSelectedData();

    auto json1 = registry.toJson();

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    // Verify state count
    EXPECT_EQ(registry2.stateCount(), 2u);

    // Verify primary selection preserved
    auto primary_before = registry.selectionContext()->primarySelectedData().toString().toStdString();
    auto primary_after = registry2.selectionContext()->primarySelectedData().toString().toStdString();
    EXPECT_EQ(primary_before, primary_after)
        << "Primary selection not preserved";

    // Verify all selections preserved (multi-select fix)
    auto all_after = registry2.selectionContext()->allSelectedData();
    EXPECT_EQ(all_before.size(), all_after.size())
        << "Multi-select count not preserved across round-trip";
    for (auto const & key : all_before) {
        EXPECT_TRUE(all_after.count(key))
            << "Selection key lost: " << key.toString().toStdString();
    }

    // Verify idempotent round-trip
    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}
FUZZ_TEST(RegistryRoundTrip, FuzzRegistryWithSelectionContext)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10));

// ============================================================================
// 4. Registry round-trip with multiple instances of same type
// ============================================================================

void FuzzRegistryMultipleInstances(
    int instance_count,
    std::vector<std::string> const & display_names)
{
    EditorRegistry registry;
    registerMockTypes(&registry);

    // MockTypeB allows multiple instances
    auto const count = std::clamp(instance_count, 0, 15);
    for (int i = 0; i < count; ++i) {
        auto state = std::make_shared<MockEditorStateB>();
        if (static_cast<size_t>(i) < display_names.size()) {
            state->setDisplayName(QString::fromStdString(display_names[i]));
        }
        registry.registerState(state);
    }

    if (registry.stateCount() == 0) {
        return;
    }

    auto infos_before = extractStateInfo(registry);
    auto json1 = registry.toJson();

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    EXPECT_EQ(registry.stateCount(), registry2.stateCount())
        << "State count mismatch for multi-instance type";

    auto infos_after = extractStateInfo(registry2);
    ASSERT_EQ(infos_before.size(), infos_after.size());
    for (size_t i = 0; i < infos_before.size(); ++i) {
        EXPECT_EQ(infos_before[i].instance_id, infos_after[i].instance_id);
        EXPECT_EQ(infos_before[i].type_name, infos_after[i].type_name);
        EXPECT_EQ(infos_before[i].display_name, infos_after[i].display_name);
    }

    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}
FUZZ_TEST(RegistryRoundTrip, FuzzRegistryMultipleInstances)
    .WithDomains(
        fuzztest::InRange(0, 15),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(100)).WithMaxSize(15));

// ============================================================================
// 5. Deterministic: empty registry round-trip
// ============================================================================

TEST(RegistryRoundTrip, EmptyRegistryRoundTrip) {
    EditorRegistry registry;
    registerMockTypes(&registry);

    auto json1 = registry.toJson();
    ASSERT_FALSE(json1.empty());

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    EXPECT_EQ(registry2.stateCount(), 0u);

    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}

// ============================================================================
// 6. Deterministic: one of each type round-trip
// ============================================================================

TEST(RegistryRoundTrip, OneOfEachTypeRoundTrip) {
    EditorRegistry registry;
    registerMockTypes(&registry);

    auto state_a = std::make_shared<MockEditorStateA>();
    state_a->setDisplayName("Alpha");
    state_a->setIntField(99);
    state_a->setDoubleField(2.718);
    state_a->setBoolField(true);
    registry.registerState(state_a);

    auto state_b = std::make_shared<MockEditorStateB>();
    state_b->setDisplayName("Beta");
    state_b->setStringField("hello");
    state_b->setListField({"a", "b", "c"});
    state_b->setOptionalField("present");
    registry.registerState(state_b);

    auto state_c = std::make_shared<MockEditorStateC>();
    state_c->setDisplayName("Gamma");
    state_c->setNested({1.0, 2.0, "center"});
    state_c->setNestedList({{3.0, 4.0, "top"}, {5.0, 6.0, "bottom"}});
    registry.registerState(state_c);

    EXPECT_EQ(registry.stateCount(), 3u);

    auto infos_before = extractStateInfo(registry);
    auto json1 = registry.toJson();

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    EXPECT_EQ(registry2.stateCount(), 3u);

    auto infos_after = extractStateInfo(registry2);
    for (size_t i = 0; i < infos_before.size(); ++i) {
        EXPECT_EQ(infos_before[i].instance_id, infos_after[i].instance_id);
        EXPECT_EQ(infos_before[i].type_name, infos_after[i].type_name);
        EXPECT_EQ(infos_before[i].display_name, infos_after[i].display_name);
    }

    // Verify specific field values
    auto restored_a = std::dynamic_pointer_cast<MockEditorStateA>(
        registry2.statesByType(EditorTypeId("MockTypeA"))[0]);
    ASSERT_NE(restored_a, nullptr);
    EXPECT_EQ(restored_a->intField(), 99);
    EXPECT_DOUBLE_EQ(restored_a->doubleField(), 2.718);
    EXPECT_EQ(restored_a->boolField(), true);

    auto restored_b = std::dynamic_pointer_cast<MockEditorStateB>(
        registry2.statesByType(EditorTypeId("MockTypeB"))[0]);
    ASSERT_NE(restored_b, nullptr);
    EXPECT_EQ(restored_b->stringField(), "hello");
    ASSERT_EQ(restored_b->listField().size(), 3u);
    EXPECT_EQ(restored_b->listField()[0], "a");
    EXPECT_TRUE(restored_b->optionalField().has_value());
    EXPECT_EQ(*restored_b->optionalField(), "present");

    auto restored_c = std::dynamic_pointer_cast<MockEditorStateC>(
        registry2.statesByType(EditorTypeId("MockTypeC"))[0]);
    ASSERT_NE(restored_c, nullptr);
    EXPECT_DOUBLE_EQ(restored_c->nested().x, 1.0);
    EXPECT_DOUBLE_EQ(restored_c->nested().y, 2.0);
    EXPECT_EQ(restored_c->nested().label, "center");
    ASSERT_EQ(restored_c->nestedList().size(), 2u);

    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}

// ============================================================================
// 7. Deterministic: fromJson clears existing states
// ============================================================================

TEST(RegistryRoundTrip, FromJsonClearsExistingStates) {
    EditorRegistry registry;
    registerMockTypes(&registry);

    // Pre-populate with 3 editors
    registry.registerState(std::make_shared<MockEditorStateA>());
    registry.registerState(std::make_shared<MockEditorStateB>());
    registry.registerState(std::make_shared<MockEditorStateB>());
    EXPECT_EQ(registry.stateCount(), 3u);

    // Create a different registry with just 1 editor
    EditorRegistry source;
    registerMockTypes(&source);
    source.registerState(std::make_shared<MockEditorStateC>());
    auto json = source.toJson();

    // fromJson should clear the 3 existing states and restore just 1
    ASSERT_TRUE(registry.fromJson(json));
    EXPECT_EQ(registry.stateCount(), 1u);

    auto states = registry.statesByType(EditorTypeId("MockTypeC"));
    EXPECT_EQ(states.size(), 1u);
}

// ============================================================================
// 8. Deterministic: unknown types are skipped (no crash)
// ============================================================================

TEST(RegistryRoundTrip, UnknownTypeSkipped) {
    // Registry1 has all 3 types
    EditorRegistry registry1;
    registerMockTypes(&registry1);

    registry1.registerState(std::make_shared<MockEditorStateA>());
    registry1.registerState(std::make_shared<MockEditorStateB>());
    registry1.registerState(std::make_shared<MockEditorStateC>());

    auto json = registry1.toJson();

    // Registry2 only knows about MockTypeA — B and C should be skipped
    EditorRegistry registry2;
    registerMockTypes(&registry2, 1); // Only MockTypeA

    ASSERT_TRUE(registry2.fromJson(json));
    EXPECT_EQ(registry2.stateCount(), 1u);

    auto states = registry2.statesByType(EditorTypeId("MockTypeA"));
    EXPECT_EQ(states.size(), 1u);
}

// ============================================================================
// 9. Deterministic: triple round-trip stability
// ============================================================================

TEST(RegistryRoundTrip, TripleRoundTripStability) {
    EditorRegistry registry;
    registerMockTypes(&registry);

    auto state_a = std::make_shared<MockEditorStateA>();
    state_a->setIntField(123);
    state_a->setDisplayName("Stable");
    registry.registerState(state_a);

    auto state_b1 = std::make_shared<MockEditorStateB>();
    state_b1->setStringField("first");
    state_b1->setListField({"x", "y"});
    registry.registerState(state_b1);

    auto state_b2 = std::make_shared<MockEditorStateB>();
    state_b2->setStringField("second");
    registry.registerState(state_b2);

    // Primary + multi-select
    SelectionSource source{EditorInstanceId("test"), "stability"};
    registry.selectionContext()->setSelectedData(SelectedDataKey("data_key_1"), source);
    registry.selectionContext()->addToSelection(SelectedDataKey("data_key_2"), source);
    registry.selectionContext()->addToSelection(SelectedDataKey("data_key_3"), source);

    auto json_original = registry.toJson();

    // Round-trip 3 times
    std::string json_current = json_original;
    for (int i = 0; i < 3; ++i) {
        EditorRegistry reg_i;
        registerMockTypes(&reg_i);
        ASSERT_TRUE(reg_i.fromJson(json_current))
            << "fromJson failed at round-trip " << i;

        EXPECT_EQ(reg_i.stateCount(), 3u)
            << "State count wrong at round-trip " << i;

        json_current = reg_i.toJson();
        ExpectJsonEqual(json_original, json_current);
    }
}

// ============================================================================
// 10. Deterministic: selection context round-trip with empty selection
// ============================================================================

TEST(RegistryRoundTrip, EmptySelectionRoundTrip) {
    EditorRegistry registry;
    registerMockTypes(&registry);
    registry.registerState(std::make_shared<MockEditorStateA>());

    // No selection set — primary should be empty
    auto json1 = registry.toJson();

    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(json1));

    EXPECT_TRUE(registry2.selectionContext()->primarySelectedData().isEmpty());
    EXPECT_TRUE(registry2.selectionContext()->allSelectedData().empty());

    auto json2 = registry2.toJson();
    ExpectJsonEqual(json1, json2);
}

// ============================================================================
// 11. Deterministic: workspaceChanged signal on fromJson
// ============================================================================

TEST(RegistryRoundTrip, FromJsonEmitsWorkspaceChanged) {
    EditorRegistry source;
    registerMockTypes(&source);
    source.registerState(std::make_shared<MockEditorStateA>());
    source.registerState(std::make_shared<MockEditorStateB>());
    auto json = source.toJson();

    EditorRegistry target;
    registerMockTypes(&target);

    QSignalSpy spy(&target, &EditorRegistry::workspaceChanged);
    ASSERT_TRUE(target.fromJson(json));

    // workspaceChanged should fire for each registerState call
    EXPECT_GE(spy.count(), 1)
        << "workspaceChanged not emitted on fromJson";
}

// ============================================================================
// 12. Deterministic: hasUnsavedChanges and markAllClean
// ============================================================================

TEST(RegistryRoundTrip, HasUnsavedChangesAfterFromJson) {
    EditorRegistry source;
    registerMockTypes(&source);
    source.registerState(std::make_shared<MockEditorStateA>());
    auto json = source.toJson();

    EditorRegistry target;
    registerMockTypes(&target);
    ASSERT_TRUE(target.fromJson(json));

    // After fromJson, states will be dirty (fromJson calls markDirty)
    EXPECT_TRUE(target.hasUnsavedChanges());

    target.markAllClean();
    EXPECT_FALSE(target.hasUnsavedChanges());
}

} // namespace
