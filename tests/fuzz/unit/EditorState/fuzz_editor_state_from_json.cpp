/**
 * @file fuzz_editor_state_from_json.cpp
 * @brief Crash-resistance fuzz tests for EditorState::fromJson()
 *
 * Feeds arbitrary strings to each EditorState subclass's fromJson() method
 * and verifies that parsing never crashes — only returns true/false.
 * Also tests with structured-but-invalid JSON and partially valid JSON.
 *
 * Phase 2 of the Workspace Fuzz Testing Roadmap.
 */

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QCoreApplication>

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
// Helper: try fromJson on a state, swallowing exceptions
// ============================================================================

template<typename StateT>
void TryFromJson(std::string const & json_str) {
    try {
        auto state = std::make_shared<StateT>();
        (void)state->fromJson(json_str);
        // No crash = success. Return value (true/false) is irrelevant.
    } catch (std::exception const &) {
        // Exceptions are acceptable
    }
}

// ============================================================================
// 1. Arbitrary string → fromJson() → no crash (all types)
// ============================================================================

void FuzzEditorStateFromGarbage(std::string const & json_str) {
    TryFromJson<DataManagerWidgetState>(json_str);
    TryFromJson<TimeScrollBarState>(json_str);
    TryFromJson<DataTransformWidgetState>(json_str);
    TryFromJson<GroupManagementWidgetState>(json_str);
    TryFromJson<DataImportWidgetState>(json_str);
    TryFromJson<DataInspectorState>(json_str);
    TryFromJson<LinePlotState>(json_str);
    TryFromJson<EventPlotState>(json_str);
    TryFromJson<MediaWidgetState>(json_str);
}
FUZZ_TEST(EditorStateCrashResistance, FuzzEditorStateFromGarbage);

// ============================================================================
// 2. Random JSON objects → fromJson() → no crash
// ============================================================================

void FuzzEditorStateFromRandomJsonObject(
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

    TryFromJson<DataManagerWidgetState>(json_str);
    TryFromJson<TimeScrollBarState>(json_str);
    TryFromJson<DataTransformWidgetState>(json_str);
    TryFromJson<GroupManagementWidgetState>(json_str);
    TryFromJson<DataImportWidgetState>(json_str);
    TryFromJson<DataInspectorState>(json_str);
    TryFromJson<LinePlotState>(json_str);
    TryFromJson<EventPlotState>(json_str);
    TryFromJson<MediaWidgetState>(json_str);
}
FUZZ_TEST(EditorStateCrashResistance, FuzzEditorStateFromRandomJsonObject)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::Arbitrary<std::string>().WithMaxSize(100)).WithMaxSize(20));

// ============================================================================
// 3. Partially valid JSON (real field names, wrong types/values)
// ============================================================================

void FuzzEditorStatePartiallyValid(
    std::string const & display_name_val,
    std::string const & instance_id_val,
    std::string const & extra_field_val,
    int numeric_val,
    bool bool_val)
{
    // Use real field names from various state types but potentially wrong types
    std::string json_str = R"({
        "display_name": ")" + display_name_val + R"(",
        "instance_id": ")" + instance_id_val + R"(",
        "show_grid": )" + (bool_val ? "true" : "false") + R"(,
        "zoom_level": )" + std::to_string(numeric_val) + R"(,
        "play_speed": )" + std::to_string(numeric_val) + R"(,
        "selected_data_key": ")" + extra_field_val + R"(",
        "displayed_media_key": ")" + extra_field_val + R"(",
        "alignment_event_key": ")" + extra_field_val + R"(",
        "nonexistent_field": "should be ignored"
    })";

    TryFromJson<DataManagerWidgetState>(json_str);
    TryFromJson<TimeScrollBarState>(json_str);
    TryFromJson<DataTransformWidgetState>(json_str);
    TryFromJson<GroupManagementWidgetState>(json_str);
    TryFromJson<DataImportWidgetState>(json_str);
    TryFromJson<DataInspectorState>(json_str);
    TryFromJson<LinePlotState>(json_str);
    TryFromJson<EventPlotState>(json_str);
    TryFromJson<MediaWidgetState>(json_str);
}
FUZZ_TEST(EditorStateCrashResistance, FuzzEditorStatePartiallyValid)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::PrintableAsciiString().WithMaxSize(50),
        fuzztest::PrintableAsciiString().WithMaxSize(100),
        fuzztest::Arbitrary<int>(),
        fuzztest::Arbitrary<bool>());

// ============================================================================
// 4. Deterministic edge cases
// ============================================================================

TEST(EditorStateCrashResistance, EmptyObjectDoesNotCrash) {
    std::string const empty_obj = "{}";
    TryFromJson<DataManagerWidgetState>(empty_obj);
    TryFromJson<TimeScrollBarState>(empty_obj);
    TryFromJson<DataTransformWidgetState>(empty_obj);
    TryFromJson<GroupManagementWidgetState>(empty_obj);
    TryFromJson<DataImportWidgetState>(empty_obj);
    TryFromJson<DataInspectorState>(empty_obj);
    TryFromJson<LinePlotState>(empty_obj);
    TryFromJson<EventPlotState>(empty_obj);
    TryFromJson<MediaWidgetState>(empty_obj);
}

TEST(EditorStateCrashResistance, NullJsonDoesNotCrash) {
    std::string const null_json = "null";
    TryFromJson<DataManagerWidgetState>(null_json);
    TryFromJson<TimeScrollBarState>(null_json);
    TryFromJson<DataTransformWidgetState>(null_json);
    TryFromJson<GroupManagementWidgetState>(null_json);
    TryFromJson<DataImportWidgetState>(null_json);
    TryFromJson<DataInspectorState>(null_json);
    TryFromJson<LinePlotState>(null_json);
    TryFromJson<EventPlotState>(null_json);
    TryFromJson<MediaWidgetState>(null_json);
}

TEST(EditorStateCrashResistance, EmptyStringDoesNotCrash) {
    std::string const empty_str;
    TryFromJson<DataManagerWidgetState>(empty_str);
    TryFromJson<TimeScrollBarState>(empty_str);
    TryFromJson<DataTransformWidgetState>(empty_str);
    TryFromJson<GroupManagementWidgetState>(empty_str);
    TryFromJson<DataImportWidgetState>(empty_str);
    TryFromJson<DataInspectorState>(empty_str);
    TryFromJson<LinePlotState>(empty_str);
    TryFromJson<EventPlotState>(empty_str);
    TryFromJson<MediaWidgetState>(empty_str);
}

TEST(EditorStateCrashResistance, ArrayInputDoesNotCrash) {
    std::string const array_json = R"([1, "two", true, null, {"nested": "object"}])";
    TryFromJson<DataManagerWidgetState>(array_json);
    TryFromJson<TimeScrollBarState>(array_json);
    TryFromJson<DataTransformWidgetState>(array_json);
    TryFromJson<GroupManagementWidgetState>(array_json);
    TryFromJson<DataImportWidgetState>(array_json);
    TryFromJson<DataInspectorState>(array_json);
    TryFromJson<LinePlotState>(array_json);
    TryFromJson<EventPlotState>(array_json);
    TryFromJson<MediaWidgetState>(array_json);
}

TEST(EditorStateCrashResistance, FromJsonReturnsFalseForInvalidInput) {
    // Verify that fromJson returns false (not just doesn't crash) for invalid input
    auto verify_false_return = [](auto state, std::string const & input,
                                  std::string const & description) {
        EXPECT_FALSE(state->fromJson(input))
            << state->getTypeName().toStdString()
            << " returned true for: " << description;
    };

    std::vector<std::string> const invalid_inputs = {
        "",
        "null",
        "42",
        "\"string\"",
        "[1,2,3]",
        "true",
    };

    std::vector<std::string> const descriptions = {
        "empty string",
        "null",
        "bare number",
        "bare string",
        "array",
        "bare boolean",
    };

    for (size_t i = 0; i < invalid_inputs.size(); ++i) {
        verify_false_return(std::make_shared<DataManagerWidgetState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<TimeScrollBarState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<DataTransformWidgetState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<GroupManagementWidgetState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<DataImportWidgetState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<DataInspectorState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<LinePlotState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<EventPlotState>(), invalid_inputs[i], descriptions[i]);
        verify_false_return(std::make_shared<MediaWidgetState>(), invalid_inputs[i], descriptions[i]);
    }
}

// ============================================================================
// 5. Instance ID not corrupted by failed fromJson
// ============================================================================

TEST(EditorStateCrashResistance, FailedFromJsonPreservesInstanceId) {
    auto verify_preserved = [](auto state) {
        QString const original_id = state->getInstanceId();
        QString const original_name = state->getDisplayName();

        // Feed garbage — should fail
        EXPECT_FALSE(state->fromJson("not valid json at all!!!"));

        // State should be unchanged
        EXPECT_EQ(state->getInstanceId(), original_id)
            << "Instance ID corrupted for: " << state->getTypeName().toStdString();
        EXPECT_EQ(state->getDisplayName(), original_name)
            << "Display name corrupted for: " << state->getTypeName().toStdString();
    };

    verify_preserved(std::make_shared<DataManagerWidgetState>());
    verify_preserved(std::make_shared<TimeScrollBarState>());
    verify_preserved(std::make_shared<DataTransformWidgetState>());
    verify_preserved(std::make_shared<GroupManagementWidgetState>());
    verify_preserved(std::make_shared<DataImportWidgetState>());
    verify_preserved(std::make_shared<DataInspectorState>());
    verify_preserved(std::make_shared<LinePlotState>());
    verify_preserved(std::make_shared<EventPlotState>());
    verify_preserved(std::make_shared<MediaWidgetState>());
}

} // namespace
