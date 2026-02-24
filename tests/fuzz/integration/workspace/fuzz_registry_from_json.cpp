/**
 * @file fuzz_registry_from_json.cpp
 * @brief Crash-resistance fuzz tests for EditorRegistry::fromJson()
 *
 * Feeds arbitrary and structured-but-malformed JSON to EditorRegistry::fromJson()
 * and verifies it never crashes — only returns true/false.
 * Also tests with corrupted workspace JSON structures.
 *
 * Phase 3 of the Workspace Fuzz Testing Roadmap.
 */

#include "MockEditorTypes.hpp"

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QCoreApplication>

#include "EditorState/EditorRegistry.hpp"
#include "EditorState/EditorState.hpp"

#include <memory>
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
            static char arg0[] = "fuzz_registry_from_json";
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
// Helper: safely call fromJson, swallowing exceptions
// ============================================================================

bool TryRegistryFromJson(std::string const & json_str) {
    try {
        EditorRegistry registry;
        registerMockTypes(&registry);
        return registry.fromJson(json_str);
    } catch (std::exception const &) {
        return false; // Exceptions are acceptable
    }
}

// ============================================================================
// 1. Arbitrary string → fromJson() → no crash
// ============================================================================

void FuzzRegistryFromGarbage(std::string const & json_str) {
    (void)TryRegistryFromJson(json_str);
}
FUZZ_TEST(RegistryCrashResistance, FuzzRegistryFromGarbage);

// ============================================================================
// 2. Random JSON objects → fromJson() → no crash
// ============================================================================

void FuzzRegistryFromRandomJsonObject(
    std::vector<std::string> const & keys,
    std::vector<std::string> const & values)
{
    // Build a JSON object string manually
    std::string json_str = "{";
    auto const n = std::min(keys.size(), values.size());
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) json_str += ",";
        json_str += "\"";
        for (char c : keys[i]) {
            if (c == '"') json_str += "\\\"";
            else if (c == '\\') json_str += "\\\\";
            else if (c == '\n') json_str += "\\n";
            else if (c == '\r') json_str += "\\r";
            else if (c == '\t') json_str += "\\t";
            else if (static_cast<unsigned char>(c) < 0x20) json_str += " ";
            else json_str += c;
        }
        json_str += "\":\"";
        for (char c : values[i]) {
            if (c == '"') json_str += "\\\"";
            else if (c == '\\') json_str += "\\\\";
            else if (c == '\n') json_str += "\\n";
            else if (c == '\r') json_str += "\\r";
            else if (c == '\t') json_str += "\\t";
            else if (static_cast<unsigned char>(c) < 0x20) json_str += " ";
            else json_str += c;
        }
        json_str += "\"";
    }
    json_str += "}";

    (void)TryRegistryFromJson(json_str);
}
FUZZ_TEST(RegistryCrashResistance, FuzzRegistryFromRandomJsonObject)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(100)).WithMaxSize(20));

// ============================================================================
// 3. Structured workspace JSON with corrupted state entries
// ============================================================================

void FuzzRegistryWithCorruptedStates(
    std::string const & version,
    std::vector<std::string> const & type_ids,
    std::vector<std::string> const & instance_ids,
    std::vector<std::string> const & state_jsons,
    std::string const & primary_selection)
{
    // Build a SerializedWorkspace-like JSON manually
    std::string json = R"({"version":")" + version + R"(","states":[)";

    auto const n = std::min({type_ids.size(), instance_ids.size(), state_jsons.size()});
    for (size_t i = 0; i < n && i < 10; ++i) {
        if (i > 0) json += ",";
        json += R"({"type_id":")" + type_ids[i]
              + R"(","instance_id":")" + instance_ids[i]
              + R"(","display_name":"item_)" + std::to_string(i)
              + R"(","state_json":")" + state_jsons[i]
              + R"("})";
    }

    json += R"(],"primary_selection":")" + primary_selection
          + R"(","all_selections":[]})";

    (void)TryRegistryFromJson(json);
}
FUZZ_TEST(RegistryCrashResistance, FuzzRegistryWithCorruptedStates)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(10),
        fuzztest::PrintableAsciiString().WithMaxSize(50));

// ============================================================================
// 4. Structured workspace JSON with valid type_ids but mangled state_json
// ============================================================================

void FuzzRegistryValidTypesGarbageState(
    int type_index,
    std::string const & garbage_state_json)
{
    std::string type_id;
    switch (type_index % 3) {
        case 0: type_id = "MockTypeA"; break;
        case 1: type_id = "MockTypeB"; break;
        case 2: type_id = "MockTypeC"; break;
    }

    std::string json = R"({"version":"1.0","states":[)"
        R"({"type_id":")" + type_id
        + R"(","instance_id":"00000000-0000-0000-0000-000000000001")"
        R"(,"display_name":"test")"
        R"(,"state_json":")" + garbage_state_json
        + R"("}],"primary_selection":"","all_selections":[]})";

    (void)TryRegistryFromJson(json);
}
FUZZ_TEST(RegistryCrashResistance, FuzzRegistryValidTypesGarbageState)
    .WithDomains(
        fuzztest::InRange(0, 10),
        fuzztest::PrintableAsciiString().WithMaxSize(500));

// ============================================================================
// 5. Deterministic edge cases
// ============================================================================

TEST(RegistryCrashResistance, EmptyStringDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson(""));
}

TEST(RegistryCrashResistance, NullJsonDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson("null"));
}

TEST(RegistryCrashResistance, EmptyObjectDoesNotCrash) {
    // Empty object is missing required fields
    EXPECT_FALSE(TryRegistryFromJson("{}"));
}

TEST(RegistryCrashResistance, EmptyArrayDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson("[]"));
}

TEST(RegistryCrashResistance, BareNumberDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson("42"));
}

TEST(RegistryCrashResistance, BareStringDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson("\"hello\""));
}

TEST(RegistryCrashResistance, BareBooleanDoesNotCrash) {
    EXPECT_FALSE(TryRegistryFromJson("true"));
}

TEST(RegistryCrashResistance, ValidStructureEmptyStates) {
    std::string json = R"({
        "version": "1.0",
        "states": [],
        "primary_selection": "",
        "all_selections": []
    })";

    EditorRegistry registry;
    registerMockTypes(&registry);
    EXPECT_TRUE(registry.fromJson(json));
    EXPECT_EQ(registry.stateCount(), 0u);
}

TEST(RegistryCrashResistance, StatesArrayContainsNonObjects) {
    std::string json = R"({
        "version": "1.0",
        "states": [42, "string", true, null],
        "primary_selection": "",
        "all_selections": []
    })";

    // Should not crash
    (void)TryRegistryFromJson(json);
}

TEST(RegistryCrashResistance, MissingStatesField) {
    std::string json = R"({
        "version": "1.0",
        "primary_selection": "",
        "all_selections": []
    })";

    (void)TryRegistryFromJson(json);
}

TEST(RegistryCrashResistance, StatesWithUnknownTypes) {
    std::string json = R"({
        "version": "1.0",
        "states": [
            {"type_id": "NonExistentType", "instance_id": "abc", "display_name": "x", "state_json": "{}"},
            {"type_id": "AlsoFake", "instance_id": "def", "display_name": "y", "state_json": "{}"}
        ],
        "primary_selection": "",
        "all_selections": []
    })";

    EditorRegistry registry;
    registerMockTypes(&registry);
    // Should succeed but produce 0 states (unknown types are skipped)
    EXPECT_TRUE(registry.fromJson(json));
    EXPECT_EQ(registry.stateCount(), 0u);
}

TEST(RegistryCrashResistance, MixedKnownAndUnknownTypes) {
    // First serialize a known type to get valid state_json
    MockEditorStateA state_a;
    auto valid_json = state_a.toJson();

    // Escape for embedding (state_json is a string inside JSON)
    std::string escaped;
    for (char c : valid_json) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
    }

    std::string json = R"({
        "version": "1.0",
        "states": [
            {"type_id": "MockTypeA", "instance_id": ")" + state_a.getInstanceId().toStdString() + R"(", "display_name": "Real", "state_json": ")" + escaped + R"("},
            {"type_id": "FakeType", "instance_id": "fake-id", "display_name": "Fake", "state_json": "{}"}
        ],
        "primary_selection": "",
        "all_selections": []
    })";

    EditorRegistry registry;
    registerMockTypes(&registry);
    EXPECT_TRUE(registry.fromJson(json));
    // Only the known type should survive
    EXPECT_EQ(registry.stateCount(), 1u);
}

TEST(RegistryCrashResistance, ExistingStatesPreservedOnParseFailure) {
    EditorRegistry registry;
    registerMockTypes(&registry);

    // Add a state
    auto original = std::make_shared<MockEditorStateA>();
    original->setIntField(999);
    registry.registerState(original);
    auto original_id = original->getInstanceId();

    // Feed unparseable JSON — should return false
    EXPECT_FALSE(registry.fromJson("not json"));

    // Original state should still be there
    EXPECT_EQ(registry.stateCount(), 1u);
    auto found = registry.state(EditorInstanceId(original_id));
    ASSERT_NE(found, nullptr);
}

TEST(RegistryCrashResistance, DeeplyNestedJson) {
    // Build deeply nested JSON that resembles workspace structure
    std::string json = R"({"version":"1.0","states":[)";
    json += R"({"type_id":"MockTypeA","instance_id":"x","display_name":"d","state_json":)";

    // Deeply nested state_json
    std::string nested = "\"";
    for (int i = 0; i < 50; ++i) {
        nested += "{\\\"nested\\\":";
    }
    nested += "\\\"deep\\\"";
    for (int i = 0; i < 50; ++i) {
        nested += "}";
    }
    nested += "\"";

    json += nested;
    json += R"(}],"primary_selection":"","all_selections":[]})";

    (void)TryRegistryFromJson(json);
}

} // namespace
