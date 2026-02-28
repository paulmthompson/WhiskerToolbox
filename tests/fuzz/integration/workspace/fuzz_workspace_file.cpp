/**
 * @file fuzz_workspace_file.cpp
 * @brief Fuzz tests for WorkspaceManager::readWorkspace() crash resistance
 *
 * Feeds arbitrary and structured-but-malformed .wtb JSON files to
 * WorkspaceManager::readWorkspace() and verifies it never crashes.
 * Also generates structured-but-corrupted workspace JSON with random
 * field combinations to exercise all parsing branches.
 *
 * Phase 5 of the Workspace Fuzz Testing Roadmap.
 *
 * Requires QCoreApplication (no display needed).
 */

#include "MockEditorTypes.hpp"

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "nlohmann/json.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "StateManagement/WorkspaceManager.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
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
            static char arg0[] = "fuzz_workspace_file";
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
// Helper: write content to a temp .wtb file and call readWorkspace()
// ============================================================================

/**
 * @brief Write file_content to a temporary .wtb file, call readWorkspace(),
 *        and return the result. Cleans up the file afterwards.
 * @return The optional<WorkspaceData> result (nullopt for invalid files)
 */
std::optional<StateManagement::WorkspaceData>
TryReadWorkspace(std::string const & file_content) {
    try {
        QTemporaryDir tmp_dir;
        if (!tmp_dir.isValid()) {
            return std::nullopt;
        }

        auto const file_path = tmp_dir.path() + "/fuzz_workspace.wtb";

        // Write content to file
        {
            QFile file(file_path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                return std::nullopt;
            }
            file.write(QByteArray::fromStdString(file_content));
            file.close();
        }

        // Attempt to read — should never crash
        StateManagement::WorkspaceManager manager(tmp_dir.path());
        return manager.readWorkspace(file_path);

    } catch (std::exception const &) {
        return std::nullopt; // Exceptions are acceptable, crashes are not
    }
}

/**
 * @brief Helper to escape a string for embedding inside a JSON string value
 */
std::string EscapeJsonString(std::string const & input) {
    std::string escaped;
    escaped.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    escaped += ' ';
                } else {
                    escaped += c;
                }
                break;
        }
    }
    return escaped;
}

// ============================================================================
// 1. Arbitrary bytes → readWorkspace() → no crash
// ============================================================================

/**
 * Tests that feeding completely arbitrary byte sequences to readWorkspace()
 * never causes a crash. The function should return nullopt for unparseable input.
 */
void FuzzWorkspaceFromGarbage(std::string const & file_content) {
    (void)TryReadWorkspace(file_content);
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceFromGarbage);

// ============================================================================
// 2. Random JSON objects → readWorkspace() → no crash
// ============================================================================

/**
 * Generates random JSON objects (with printable ASCII keys/values) and attempts
 * to parse them as workspace files. Should not crash.
 */
void FuzzWorkspaceFromRandomJsonObject(
    std::vector<std::string> const & keys,
    std::vector<std::string> const & values)
{
    std::string json_str = "{";
    auto const n = std::min(keys.size(), values.size());
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) json_str += ",";
        json_str += "\"" + EscapeJsonString(keys[i]) + "\"";
        json_str += ":\"" + EscapeJsonString(values[i]) + "\"";
    }
    json_str += "}";

    (void)TryReadWorkspace(json_str);
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceFromRandomJsonObject)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(100)).WithMaxSize(20));

// ============================================================================
// 3. Structured workspace JSON with corrupted fields
// ============================================================================

/**
 * Generates structurally plausible workspace JSON with fuzz-generated field values.
 * This exercises all the parsing branches in _readWorkspaceFile():
 * - version, created_at, modified_at (strings)
 * - data_loads (array of objects with loader_type, source_path, relative_path)
 * - editor_states (sub-object, may be invalid JSON when re-dumped)
 * - zone_layout (sub-object, may be invalid JSON when re-dumped)
 * - dock_state (string)
 * - applied_pipelines (array)
 * - table_definitions (array)
 */
void FuzzWorkspaceStructured(
    std::string const & version,
    std::vector<std::string> const & source_paths,
    std::string const & editor_states_json,
    std::string const & zone_layout_json,
    std::string const & dock_state)
{
    try {
        nlohmann::json workspace;
        workspace["version"] = version;
        workspace["created_at"] = "2025-01-01T00:00:00Z";
        workspace["modified_at"] = "2025-01-01T00:00:00Z";

        auto loads_arr = nlohmann::json::array();
        for (auto const & p : source_paths) {
            nlohmann::json entry;
            entry["loader_type"] = "json_config";
            entry["source_path"] = p;
            entry["relative_path"] = p;
            loads_arr.push_back(entry);
        }
        workspace["data_loads"] = loads_arr;

        // editor_states — try to parse as JSON, fall back to empty object
        try {
            workspace["editor_states"] = nlohmann::json::parse(editor_states_json);
        } catch (...) {
            workspace["editor_states"] = nlohmann::json::object();
        }

        // zone_layout — try to parse as JSON, fall back to empty object
        try {
            workspace["zone_layout"] = nlohmann::json::parse(zone_layout_json);
        } catch (...) {
            workspace["zone_layout"] = nlohmann::json::object();
        }

        workspace["dock_state"] = dock_state;
        workspace["applied_pipelines"] = nlohmann::json::array();
        workspace["table_definitions"] = nlohmann::json::array();

        (void)TryReadWorkspace(workspace.dump(2));

    } catch (std::exception const &) {
        // nlohmann::json operations themselves might fail on extreme input
    }
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceStructured)
    .WithDomains(
        fuzztest::PrintableAsciiString().WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(10),
        fuzztest::PrintableAsciiString().WithMaxSize(500),
        fuzztest::PrintableAsciiString().WithMaxSize(500),
        fuzztest::PrintableAsciiString().WithMaxSize(200));

// ============================================================================
// 4. Structured workspace with random pipeline / table arrays
// ============================================================================

/**
 * Exercises the applied_pipelines and table_definitions parsing with
 * a mix of valid JSON strings, garbage strings, and nested objects.
 */
void FuzzWorkspaceWithPipelinesAndTables(
    std::vector<std::string> const & pipelines,
    std::vector<std::string> const & tables)
{
    try {
        nlohmann::json workspace;
        workspace["version"] = "1.0";
        workspace["created_at"] = "2025-01-01T00:00:00Z";
        workspace["modified_at"] = "2025-01-01T00:00:00Z";
        workspace["data_loads"] = nlohmann::json::array();
        workspace["editor_states"] = nlohmann::json::object();
        workspace["zone_layout"] = nlohmann::json::object();
        workspace["dock_state"] = "";

        // Insert pipelines as raw strings (some may be valid JSON, some not)
        auto pipelines_arr = nlohmann::json::array();
        for (auto const & pj : pipelines) {
            try {
                pipelines_arr.push_back(nlohmann::json::parse(pj));
            } catch (...) {
                pipelines_arr.push_back(pj); // Push as string if not valid JSON
            }
        }
        workspace["applied_pipelines"] = pipelines_arr;

        auto tables_arr = nlohmann::json::array();
        for (auto const & tj : tables) {
            try {
                tables_arr.push_back(nlohmann::json::parse(tj));
            } catch (...) {
                tables_arr.push_back(tj);
            }
        }
        workspace["table_definitions"] = tables_arr;

        (void)TryReadWorkspace(workspace.dump(2));

    } catch (std::exception const &) {
        // Construction might fail on extreme input
    }
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceWithPipelinesAndTables)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(300)).WithMaxSize(10),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(300)).WithMaxSize(10));

// ============================================================================
// 5. Workspace with data_loads containing diverse/malformed entries
// ============================================================================

/**
 * Exercises data_loads parsing with randomized loader_type, source_path,
 * and relative_path values, including edge cases like empty strings,
 * extremely long paths, and paths with special characters.
 */
void FuzzWorkspaceDataLoads(
    std::vector<std::string> const & loader_types,
    std::vector<std::string> const & source_paths,
    std::vector<std::string> const & relative_paths)
{
    try {
        nlohmann::json workspace;
        workspace["version"] = "1.0";
        workspace["created_at"] = "2025-01-01T00:00:00Z";
        workspace["modified_at"] = "2025-01-01T00:00:00Z";

        auto loads_arr = nlohmann::json::array();
        auto const n = std::min({loader_types.size(), source_paths.size(), relative_paths.size()});
        for (size_t i = 0; i < n && i < 20; ++i) {
            nlohmann::json entry;
            entry["loader_type"] = loader_types[i];
            entry["source_path"] = source_paths[i];
            entry["relative_path"] = relative_paths[i];
            loads_arr.push_back(entry);
        }
        workspace["data_loads"] = loads_arr;

        workspace["editor_states"] = nlohmann::json::object();
        workspace["zone_layout"] = nlohmann::json::object();
        workspace["dock_state"] = "";
        workspace["applied_pipelines"] = nlohmann::json::array();
        workspace["table_definitions"] = nlohmann::json::array();

        (void)TryReadWorkspace(workspace.dump(2));

    } catch (std::exception const &) {
        // Construction might fail
    }
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceDataLoads)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(50)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(20),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(20));

// ============================================================================
// 6. Workspace file with wrong JSON types for expected fields
// ============================================================================

/**
 * Tests readWorkspace() with a valid JSON document where fields have
 * incorrect types (e.g., data_loads is a string, editor_states is a number).
 */
void FuzzWorkspaceWrongTypes(
    int data_loads_type,
    int editor_states_type,
    int zone_layout_type,
    int dock_state_type)
{
    try {
        nlohmann::json workspace;
        workspace["version"] = "1.0";
        workspace["created_at"] = "2025-01-01T00:00:00Z";
        workspace["modified_at"] = "2025-01-01T00:00:00Z";

        // Assign wrong types to each field based on fuzz input
        auto wrong_type = [](int selector) -> nlohmann::json {
            switch (selector % 7) {
                case 0: return nullptr;
                case 1: return 42;
                case 2: return 3.14;
                case 3: return true;
                case 4: return "a string";
                case 5: return nlohmann::json::array({1, 2, 3});
                case 6: return nlohmann::json::object({{"k", "v"}});
                default: return nullptr;
            }
        };

        workspace["data_loads"] = wrong_type(data_loads_type);
        workspace["editor_states"] = wrong_type(editor_states_type);
        workspace["zone_layout"] = wrong_type(zone_layout_type);
        workspace["dock_state"] = wrong_type(dock_state_type);
        workspace["applied_pipelines"] = nlohmann::json::array();
        workspace["table_definitions"] = nlohmann::json::array();

        (void)TryReadWorkspace(workspace.dump(2));

    } catch (std::exception const &) {
        // Construction might fail
    }
}
FUZZ_TEST(WorkspaceFileCrashResistance, FuzzWorkspaceWrongTypes)
    .WithDomains(
        fuzztest::InRange(0, 20),
        fuzztest::InRange(0, 20),
        fuzztest::InRange(0, 20),
        fuzztest::InRange(0, 20));

// ============================================================================
// 7. Round-trip: save → read → verify fields preserved
// ============================================================================

/**
 * Creates a WorkspaceManager, records fuzz-generated data loads and pipelines,
 * wires up mock editor states, saves to a file, reads it back, and verifies
 * that all fields survive the round-trip.
 */
void FuzzWorkspaceRoundTrip(
    std::vector<std::string> const & source_paths,
    std::vector<std::string> const & pipeline_jsons,
    std::vector<std::string> const & table_jsons)
{
    QTemporaryDir tmp_dir;
    if (!tmp_dir.isValid()) return;

    auto const file_path = tmp_dir.path() + "/roundtrip.wtb";

    // -- Set up a manager with mock registry --
    StateManagement::WorkspaceManager manager(tmp_dir.path());

    EditorRegistry registry;
    registerMockTypes(&registry);
    manager.setEditorRegistry(&registry);

    // Create a state so the registry has something to serialize
    auto state_a = std::make_shared<MockEditorStateA>();
    registry.registerState(state_a);

    // Record data loads (limited to valid categories)
    for (auto const & path : source_paths) {
        if (path.empty()) continue;
        manager.recordDataLoad(StateManagement::DataLoadEntry{
            .loader_type = "json_config",
            .source_path = path});
    }

    // Record pipelines
    for (auto const & pj : pipeline_jsons) {
        manager.recordAppliedPipeline(pj);
    }

    // Record tables
    for (auto const & tj : table_jsons) {
        manager.recordTableDefinition(tj);
    }

    // Save
    bool saved = manager.saveWorkspace(file_path);
    if (!saved) return; // File I/O failure — skip this iteration

    // Read back
    auto loaded = manager.readWorkspace(file_path);
    ASSERT_TRUE(loaded.has_value()) << "readWorkspace failed on file we just saved";

    // Verify data loads preserved
    EXPECT_EQ(loaded->data_loads.size(), manager.dataLoads().size());

    // Verify pipeline count
    EXPECT_EQ(loaded->applied_pipelines.size(), pipeline_jsons.size());

    // Verify table definition count
    EXPECT_EQ(loaded->table_definitions.size(), table_jsons.size());

    // Verify editor states are non-empty (we created one state)
    EXPECT_FALSE(loaded->editor_states_json.empty());

    // Verify version
    EXPECT_EQ(loaded->version, "1.0");
}
FUZZ_TEST(WorkspaceFileRoundTrip, FuzzWorkspaceRoundTrip)
    .WithDomains(
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(100)).WithMaxSize(5),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(5),
        fuzztest::VectorOf(fuzztest::PrintableAsciiString().WithMaxSize(200)).WithMaxSize(5));

// ============================================================================
// 8. Full workspace idempotency: save → read → save again → compare
// ============================================================================

void FuzzWorkspaceIdempotency(
    int int_val, double double_val, bool bool_val,
    std::string const & pipeline_str)
{
    QTemporaryDir tmp_dir;
    if (!tmp_dir.isValid()) return;

    auto const file1 = tmp_dir.path() + "/idem1.wtb";
    auto const file2 = tmp_dir.path() + "/idem2.wtb";

    // -- Build first workspace --
    StateManagement::WorkspaceManager manager1(tmp_dir.path());
    EditorRegistry registry1;
    registerMockTypes(&registry1);
    manager1.setEditorRegistry(&registry1);

    auto state_a = std::make_shared<MockEditorStateA>();
    state_a->setIntField(int_val);
    // Guard NaN/Inf for double — they won't round-trip through JSON
    if (std::isfinite(double_val)) {
        state_a->setDoubleField(double_val);
    }
    state_a->setBoolField(bool_val);
    registry1.registerState(state_a);

    if (!pipeline_str.empty()) {
        manager1.recordAppliedPipeline(pipeline_str);
    }

    if (!manager1.saveWorkspace(file1)) return;

    // -- Read and restore into second manager --
    auto data = manager1.readWorkspace(file1);
    ASSERT_TRUE(data.has_value());

    StateManagement::WorkspaceManager manager2(tmp_dir.path());
    EditorRegistry registry2;
    registerMockTypes(&registry2);
    manager2.setEditorRegistry(&registry2);

    // Restore registry state
    ASSERT_TRUE(registry2.fromJson(data->editor_states_json));

    // Restore data loads
    for (auto const & entry : data->data_loads) {
        manager2.recordDataLoad(entry);
    }
    for (auto const & pj : data->applied_pipelines) {
        manager2.recordAppliedPipeline(pj);
    }
    for (auto const & tj : data->table_definitions) {
        manager2.recordTableDefinition(tj);
    }

    // Save second workspace
    if (!manager2.saveWorkspace(file2)) return;

    // Read both back and compare editor states
    auto data1 = manager1.readWorkspace(file1);
    auto data2 = manager2.readWorkspace(file2);
    ASSERT_TRUE(data1.has_value());
    ASSERT_TRUE(data2.has_value());

    // Editor states should be structurally identical
    if (!data1->editor_states_json.empty() && !data2->editor_states_json.empty()) {
        auto parsed1 = nlohmann::json::parse(data1->editor_states_json);
        auto parsed2 = nlohmann::json::parse(data2->editor_states_json);
        EXPECT_EQ(parsed1, parsed2)
            << "Editor states diverged on round-trip!\n"
            << "  First:  " << data1->editor_states_json.substr(0, 300) << "\n"
            << "  Second: " << data2->editor_states_json.substr(0, 300);
    }

    // Data loads count should match
    EXPECT_EQ(data1->data_loads.size(), data2->data_loads.size());

    // Pipeline count should match
    EXPECT_EQ(data1->applied_pipelines.size(), data2->applied_pipelines.size());
}
FUZZ_TEST(WorkspaceFileRoundTrip, FuzzWorkspaceIdempotency)
    .WithDomains(
        fuzztest::Arbitrary<int>(),
        fuzztest::Finite<double>(),
        fuzztest::Arbitrary<bool>(),
        fuzztest::PrintableAsciiString().WithMaxSize(200));

// ============================================================================
// Deterministic edge cases
// ============================================================================

TEST(WorkspaceFileDeterministic, EmptyFile) {
    auto result = TryReadWorkspace("");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, NullJson) {
    auto result = TryReadWorkspace("null");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, EmptyObject) {
    auto result = TryReadWorkspace("{}");
    // Should succeed — all fields have defaults or are optional
    if (result.has_value()) {
        EXPECT_EQ(result->version, "1.0"); // default
        EXPECT_TRUE(result->data_loads.empty());
        EXPECT_TRUE(result->editor_states_json.empty());
        EXPECT_TRUE(result->zone_layout_json.empty());
    }
}

TEST(WorkspaceFileDeterministic, EmptyArray) {
    auto result = TryReadWorkspace("[]");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, BareNumber) {
    auto result = TryReadWorkspace("42");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, BareString) {
    auto result = TryReadWorkspace("\"hello\"");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, BareBoolean) {
    auto result = TryReadWorkspace("true");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, ValidMinimalWorkspace) {
    std::string json = R"({
        "version": "1.0",
        "created_at": "2025-01-01T00:00:00Z",
        "modified_at": "2025-01-01T00:00:00Z",
        "data_loads": [],
        "editor_states": {},
        "zone_layout": {},
        "dock_state": "",
        "applied_pipelines": [],
        "table_definitions": []
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->version, "1.0");
    EXPECT_EQ(result->created_at, "2025-01-01T00:00:00Z");
    EXPECT_TRUE(result->data_loads.empty());
    EXPECT_TRUE(result->editor_states_json.empty());
    EXPECT_TRUE(result->zone_layout_json.empty());
    EXPECT_TRUE(result->dock_state_base64.empty());
    EXPECT_TRUE(result->applied_pipelines.empty());
    EXPECT_TRUE(result->table_definitions.empty());
}

TEST(WorkspaceFileDeterministic, DataLoadsPreserved) {
    std::string json = R"({
        "version": "1.0",
        "data_loads": [
            {"loader_type": "json_config", "source_path": "/home/user/data.json"},
            {"loader_type": "video", "source_path": "/home/user/video.mp4"},
            {"loader_type": "images", "source_path": "/home/user/images/"}
        ],
        "editor_states": {},
        "zone_layout": {}
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->data_loads.size(), 3u);
    EXPECT_EQ(result->data_loads[0].loader_type, "json_config");
    EXPECT_EQ(result->data_loads[1].loader_type, "video");
    EXPECT_EQ(result->data_loads[2].loader_type, "images");
}

TEST(WorkspaceFileDeterministic, DataLoadsWithMissingFields) {
    std::string json = R"({
        "version": "1.0",
        "data_loads": [
            {"loader_type": "json_config"},
            {"source_path": "/path"},
            {},
            {"loader_type": "video", "source_path": "/v.mp4", "relative_path": "../v.mp4"}
        ],
        "editor_states": {},
        "zone_layout": {}
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->data_loads.size(), 4u);
    // Missing fields should have empty string defaults
    EXPECT_EQ(result->data_loads[0].source_path, "");
    EXPECT_EQ(result->data_loads[1].loader_type, "");
    EXPECT_EQ(result->data_loads[2].loader_type, "");
    EXPECT_EQ(result->data_loads[2].source_path, "");
    // Complete entry should be fully preserved
    EXPECT_EQ(result->data_loads[3].loader_type, "video");
    EXPECT_EQ(result->data_loads[3].relative_path, "../v.mp4");
}

TEST(WorkspaceFileDeterministic, EditorStatesPreservedAsJson) {
    // editor_states is embedded as a parsed JSON object, not an escaped string
    std::string json = R"({
        "version": "1.0",
        "data_loads": [],
        "editor_states": {
            "version": "1.0",
            "states": [
                {
                    "type_id": "MockTypeA",
                    "instance_id": "test-id-1",
                    "display_name": "Test Widget",
                    "state_json": "{}"
                }
            ],
            "primary_selection": "",
            "all_selections": []
        },
        "zone_layout": {}
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->editor_states_json.empty());

    // Verify the editor states JSON is valid and parseable
    auto parsed = nlohmann::json::parse(result->editor_states_json);
    EXPECT_TRUE(parsed.contains("states"));
    EXPECT_EQ(parsed["states"].size(), 1u);
}

TEST(WorkspaceFileDeterministic, DockStatePreserved) {
    std::string json = R"({
        "version": "1.0",
        "data_loads": [],
        "editor_states": {},
        "zone_layout": {},
        "dock_state": "AAAA+base64encodedstate=="
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->dock_state_base64, "AAAA+base64encodedstate==");
}

TEST(WorkspaceFileDeterministic, AppliedPipelinesPreserved) {
    std::string json = R"({
        "version": "1.0",
        "data_loads": [],
        "editor_states": {},
        "zone_layout": {},
        "applied_pipelines": [
            {"transform": "mask_area", "input": "mask1"},
            "plain string pipeline"
        ],
        "table_definitions": []
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->applied_pipelines.size(), 2u);
    // First one was a JSON object, should be dumped back to string
    auto parsed0 = nlohmann::json::parse(result->applied_pipelines[0]);
    EXPECT_EQ(parsed0["transform"], "mask_area");
    // Second one is a plain string
    EXPECT_EQ(result->applied_pipelines[1], "plain string pipeline");
}

TEST(WorkspaceFileDeterministic, TableDefinitionsPreserved) {
    std::string json = R"({
        "version": "1.0",
        "data_loads": [],
        "editor_states": {},
        "zone_layout": {},
        "applied_pipelines": [],
        "table_definitions": [
            {"table_name": "my_table", "columns": ["col1", "col2"]},
            "raw table string"
        ]
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->table_definitions.size(), 2u);
    auto parsed0 = nlohmann::json::parse(result->table_definitions[0]);
    EXPECT_EQ(parsed0["table_name"], "my_table");
    EXPECT_EQ(result->table_definitions[1], "raw table string");
}

TEST(WorkspaceFileDeterministic, NonExistentFileReturnsNullopt) {
    StateManagement::WorkspaceManager manager("/tmp");
    auto result = manager.readWorkspace("/tmp/nonexistent_file_that_does_not_exist.wtb");
    EXPECT_FALSE(result.has_value());
}

TEST(WorkspaceFileDeterministic, MissingOptionalFieldsHandledGracefully) {
    // Only version present — all other fields should get defaults
    std::string json = R"({"version": "2.0"})";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->version, "2.0");
    EXPECT_TRUE(result->data_loads.empty());
    EXPECT_TRUE(result->editor_states_json.empty());
    EXPECT_TRUE(result->zone_layout_json.empty());
    EXPECT_TRUE(result->dock_state_base64.empty());
    EXPECT_TRUE(result->applied_pipelines.empty());
    EXPECT_TRUE(result->table_definitions.empty());
}

TEST(WorkspaceFileDeterministic, DeeplyNestedEditorStates) {
    // Build deeply nested JSON that tests parsing limits
    std::string inner = R"({"deep": true})";
    for (int i = 0; i < 30; ++i) {
        inner = R"({"nested": )" + inner + "}";
    }

    std::string json = R"({"version": "1.0", "data_loads": [], "editor_states": )"
                      + inner + R"(, "zone_layout": {}})";

    // Should not crash — may or may not parse successfully
    (void)TryReadWorkspace(json);
}

TEST(WorkspaceFileDeterministic, ExtraneousFieldsIgnored) {
    std::string json = R"({
        "version": "1.0",
        "unknown_field_1": "hello",
        "unknown_field_2": 42,
        "really_unknown": [1, 2, 3],
        "data_loads": [],
        "editor_states": {},
        "zone_layout": {}
    })";

    auto result = TryReadWorkspace(json);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->version, "1.0");
}

TEST(WorkspaceFileDeterministic, SaveAndReadBackMatchesSavedState) {
    QTemporaryDir tmp_dir;
    ASSERT_TRUE(tmp_dir.isValid());

    auto const file_path = tmp_dir.path() + "/test_save.wtb";

    StateManagement::WorkspaceManager manager(tmp_dir.path());
    EditorRegistry registry;
    registerMockTypes(&registry);
    manager.setEditorRegistry(&registry);

    // Create and register a state
    auto state = std::make_shared<MockEditorStateA>();
    state->setIntField(123);
    state->setDoubleField(4.56);
    state->setBoolField(true);
    registry.registerState(state);

    // Record data loads
    manager.recordDataLoad(StateManagement::DataLoadEntry{
        .loader_type = "json_config",
        .source_path = "/home/user/test.json"});
    manager.recordAppliedPipeline(R"({"transform": "test"})");
    manager.recordTableDefinition(R"({"name": "table1"})");

    // Save
    ASSERT_TRUE(manager.saveWorkspace(file_path));

    // Read back
    auto loaded = manager.readWorkspace(file_path);
    ASSERT_TRUE(loaded.has_value());

    EXPECT_EQ(loaded->version, "1.0");
    ASSERT_EQ(loaded->data_loads.size(), 1u);
    EXPECT_EQ(loaded->data_loads[0].loader_type, "json_config");
    EXPECT_FALSE(loaded->editor_states_json.empty());
    ASSERT_EQ(loaded->applied_pipelines.size(), 1u);
    ASSERT_EQ(loaded->table_definitions.size(), 1u);

    // Verify editor state round-trip through registry
    EditorRegistry registry2;
    registerMockTypes(&registry2);
    ASSERT_TRUE(registry2.fromJson(loaded->editor_states_json));
    EXPECT_EQ(registry2.stateCount(), 1u);
}

} // namespace
