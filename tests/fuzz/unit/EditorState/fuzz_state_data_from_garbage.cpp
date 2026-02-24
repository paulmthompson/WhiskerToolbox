/**
 * @file fuzz_state_data_from_garbage.cpp
 * @brief Crash-resistance fuzz tests for EditorState data structs
 *
 * Feeds arbitrary strings to rfl::json::read<T>() for every StateData struct
 * and verifies that parsing never crashes — only returns a valid result or
 * an error. This is the classic "throw garbage at the parser" approach.
 *
 * Phase 1 of the Workspace Fuzz Testing Roadmap.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <rfl.hpp>
#include <rfl/json.hpp>

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
// Crash-resistance: arbitrary string → rfl::json::read<T>() → no crash
// ============================================================================

/**
 * @brief Feed arbitrary strings to all StateData deserializers.
 *
 * The only requirement is that no call crashes. Parsing failures
 * (returning an error result) are expected and acceptable.
 */
void FuzzStateDataFromGarbage(std::string const & json_str) {
    // Each read may succeed or fail — we only care that it doesn't crash
    try {
        (void)rfl::json::read<TestWidgetStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<DataManagerWidgetStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<TimeScrollBarStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<DataTransformWidgetStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<GroupManagementWidgetStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<DataImportWidgetStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<DataInspectorStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<LinePlotStateData>(json_str);
    } catch (std::exception const &) {}

    try {
        (void)rfl::json::read<EventPlotStateData>(json_str);
    } catch (std::exception const &) {}
}
FUZZ_TEST(StateDataCrashResistance, FuzzStateDataFromGarbage);

// ============================================================================
// Structured garbage: valid JSON but incorrect types/missing fields
// ============================================================================

/**
 * @brief Generate valid JSON objects with random keys and string values,
 *        then try to deserialize as each StateData type.
 *
 * This specifically targets the case where the JSON is syntactically valid
 * but semantically wrong — e.g., wrong field names, wrong types for fields,
 * extra unexpected fields, etc.
 */
void FuzzStateDataFromRandomJsonObject(
    std::vector<std::string> const & keys,
    std::vector<std::string> const & values)
{
    // Build a JSON object string manually
    std::string json_str = "{";
    auto const n = std::min(keys.size(), values.size());
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) json_str += ",";
        // Escape the key and value minimally for valid JSON
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

    // No crashes allowed
    try { (void)rfl::json::read<TestWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(json_str); } catch (...) {}
}
FUZZ_TEST(StateDataCrashResistance, FuzzStateDataFromRandomJsonObject)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(100)).WithMaxSize(20)
    );

// ============================================================================
// Partial valid JSON: mix real field names with garbage values
// ============================================================================

/**
 * @brief Construct JSON with real field names but fuzzed values
 *        (type mismatches, out-of-range values, etc.)
 */
void FuzzStateDataPartiallyValid(
    std::string const & display_name_val,
    std::string const & instance_id_val,
    std::string const & extra_field_val,
    int numeric_val,
    bool bool_val)
{
    // Use real field names but potentially wrong types
    std::string json_str = R"({
        "display_name": ")" + display_name_val + R"(",
        "instance_id": ")" + instance_id_val + R"(",
        "show_grid": )" + (bool_val ? "true" : "false") + R"(,
        "zoom_level": )" + std::to_string(numeric_val) + R"(,
        "play_speed": )" + std::to_string(numeric_val) + R"(,
        "selected_data_key": ")" + extra_field_val + R"(",
        "nonexistent_field": "should be ignored"
    })";

    // No crashes allowed
    try { (void)rfl::json::read<TestWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(json_str); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(json_str); } catch (...) {}
}
FUZZ_TEST(StateDataCrashResistance, FuzzStateDataPartiallyValid)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<bool>()
    );

// ============================================================================
// Minimal JSON: empty object should parse without crash for all types
// ============================================================================

TEST(StateDataCrashResistance, EmptyObjectDoesNotCrash) {
    std::string const empty_obj = "{}";
    try { (void)rfl::json::read<TestWidgetStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(empty_obj); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(empty_obj); } catch (...) {}
}

TEST(StateDataCrashResistance, NullDoesNotCrash) {
    std::string const null_str = "null";
    try { (void)rfl::json::read<TestWidgetStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(null_str); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(null_str); } catch (...) {}
}

TEST(StateDataCrashResistance, EmptyStringDoesNotCrash) {
    std::string const empty_str;
    try { (void)rfl::json::read<TestWidgetStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(empty_str); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(empty_str); } catch (...) {}
}

TEST(StateDataCrashResistance, ArrayDoesNotCrash) {
    std::string const array_str = "[1, 2, 3]";
    try { (void)rfl::json::read<TestWidgetStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<DataManagerWidgetStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<TimeScrollBarStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<DataTransformWidgetStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<GroupManagementWidgetStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<DataImportWidgetStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<DataInspectorStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<LinePlotStateData>(array_str); } catch (...) {}
    try { (void)rfl::json::read<EventPlotStateData>(array_str); } catch (...) {}
}

} // namespace
