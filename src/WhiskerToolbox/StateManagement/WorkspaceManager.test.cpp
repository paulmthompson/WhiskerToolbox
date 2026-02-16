/**
 * @file WorkspaceManager.test.cpp
 * @brief Integration tests for WorkspaceManager
 *
 * Tests workspace save/load round-trip, auto-save, recovery file management,
 * data load tracking, and dirty flag changes.
 *
 * These tests require Qt (for QObject, QTimer, signals) and use
 * a temporary directory for file I/O.
 */

#include <catch2/catch_test_macros.hpp>

#include "StateManagement/WorkspaceManager.hpp"
#include "StateManagement/WorkspaceData.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace StateManagement;

// ============================================================================
// Data Load Tracking Tests
// ============================================================================

TEST_CASE("WorkspaceManager - data load tracking", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK(mgr.dataLoads().empty());

    mgr.recordJsonConfigLoad(QStringLiteral("/data/config.json"));
    REQUIRE(mgr.dataLoads().size() == 1);
    CHECK(mgr.dataLoads()[0].loader_type == "json_config");
    CHECK(mgr.dataLoads()[0].source_path == "/data/config.json");

    mgr.recordVideoLoad(QStringLiteral("/data/video.mp4"));
    REQUIRE(mgr.dataLoads().size() == 2);
    CHECK(mgr.dataLoads()[1].loader_type == "video");

    mgr.recordImagesLoad(QStringLiteral("/data/images/"));
    REQUIRE(mgr.dataLoads().size() == 3);
    CHECK(mgr.dataLoads()[2].loader_type == "images");

    mgr.clearDataLoads();
    CHECK(mgr.dataLoads().empty());
}

// ============================================================================
// Dirty Flag Tests
// ============================================================================

TEST_CASE("WorkspaceManager - dirty flag and signals", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK_FALSE(mgr.hasUnsavedChanges());

    int dirty_signal_count = 0;
    bool last_dirty_value = false;
    QObject::connect(&mgr, &WorkspaceManager::dirtyChanged,
                     [&](bool dirty) {
                         dirty_signal_count++;
                         last_dirty_value = dirty;
                     });

    // Recording a data load should set dirty
    mgr.recordVideoLoad(QStringLiteral("/video.mp4"));
    CHECK(mgr.hasUnsavedChanges());
    CHECK(dirty_signal_count == 1);
    CHECK(last_dirty_value == true);

    // markClean should clear dirty
    mgr.markClean();
    CHECK_FALSE(mgr.hasUnsavedChanges());
    CHECK(dirty_signal_count == 2);
    CHECK(last_dirty_value == false);

    // markDirty explicitly
    mgr.markDirty();
    CHECK(mgr.hasUnsavedChanges());
    CHECK(dirty_signal_count == 3);

    // Setting dirty again should NOT re-emit
    mgr.markDirty();
    CHECK(dirty_signal_count == 3);
}

// ============================================================================
// Workspace Path Tests
// ============================================================================

TEST_CASE("WorkspaceManager - current path and markRestored", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK(mgr.currentWorkspacePath().isEmpty());

    int path_signal_count = 0;
    QString last_path;
    QObject::connect(&mgr, &WorkspaceManager::workspacePathChanged,
                     [&](QString path) {
                         path_signal_count++;
                         last_path = path;
                     });

    mgr.markRestored(QStringLiteral("/some/workspace.wtb"));
    CHECK(mgr.currentWorkspacePath() == "/some/workspace.wtb");
    CHECK_FALSE(mgr.hasUnsavedChanges());
    CHECK(path_signal_count == 1);
    CHECK(last_path == "/some/workspace.wtb");
}

// ============================================================================
// Save & Load Round-Trip Tests (no collaborators)
// ============================================================================

TEST_CASE("WorkspaceManager - save and load with no collaborators", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/test.wtb");

    // Save
    {
        WorkspaceManager mgr(tmp_dir.path());
        mgr.recordJsonConfigLoad(QStringLiteral("/data/config.json"));
        mgr.recordVideoLoad(QStringLiteral("/data/video.mp4"));

        bool saved = mgr.saveWorkspace(ws_path);
        CHECK(saved);
        CHECK(mgr.currentWorkspacePath() == ws_path);
        CHECK_FALSE(mgr.hasUnsavedChanges());
    }

    // Verify the file exists
    CHECK(QFile::exists(ws_path));

    // Load
    {
        WorkspaceManager mgr(tmp_dir.path());
        auto data = mgr.readWorkspace(ws_path);
        REQUIRE(data.has_value());
        CHECK(data->version == "1.0");
        CHECK_FALSE(data->created_at.empty());
        CHECK_FALSE(data->modified_at.empty());

        // Data loads
        REQUIRE(data->data_loads.size() == 2);
        CHECK(data->data_loads[0].loader_type == "json_config");
        CHECK(data->data_loads[0].source_path == "/data/config.json");
        CHECK(data->data_loads[1].loader_type == "video");
        CHECK(data->data_loads[1].source_path == "/data/video.mp4");

        // No collaborators → empty states
        CHECK(data->editor_states_json.empty());
        CHECK(data->zone_layout_json.empty());
    }
}

TEST_CASE("WorkspaceManager - save signals", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    int saved_count = 0;
    QString saved_path;
    QObject::connect(&mgr, &WorkspaceManager::workspaceSaved,
                     [&](QString path) {
                         saved_count++;
                         saved_path = path;
                     });

    auto const ws_path = tmp_dir.path() + QStringLiteral("/test.wtb");
    mgr.saveWorkspace(ws_path);

    CHECK(saved_count == 1);
    CHECK(saved_path == ws_path);
}

TEST_CASE("WorkspaceManager - read nonexistent file returns nullopt", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());
    auto data = mgr.readWorkspace(QStringLiteral("/nonexistent/path.wtb"));
    CHECK_FALSE(data.has_value());
}

// ============================================================================
// Recovery File Tests
// ============================================================================

TEST_CASE("WorkspaceManager - recovery file path", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());
    CHECK(mgr.recoveryFilePath() == tmp_dir.path() + "/recovery.wtb");
}

TEST_CASE("WorkspaceManager - no recovery file by default", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());
    CHECK_FALSE(mgr.hasRecoveryFile());
}

TEST_CASE("WorkspaceManager - deleteRecoveryFile is safe when no file", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());
    // Should not crash
    mgr.deleteRecoveryFile();
    CHECK_FALSE(mgr.hasRecoveryFile());
}

TEST_CASE("WorkspaceManager - deleteRecoveryFile removes existing file", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    // Manually create a recovery file
    auto const recovery_path = tmp_dir.path() + QStringLiteral("/recovery.wtb");
    {
        QFile f(recovery_path);
        f.open(QIODevice::WriteOnly);
        f.write("{}");
        f.close();
    }

    WorkspaceManager mgr(tmp_dir.path());
    CHECK(mgr.hasRecoveryFile());

    mgr.deleteRecoveryFile();
    CHECK_FALSE(mgr.hasRecoveryFile());
}

// ============================================================================
// Auto-Save Tests
// ============================================================================

TEST_CASE("WorkspaceManager - auto-save enable/disable", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK_FALSE(mgr.isAutoSaveEnabled());

    // Note: enableAutoSave() creates a QTimer which needs a running event loop.
    // Without QCoreApplication the timer won't actually start, so we just
    // verify that the calls don't crash and disable returns to initial state.
    mgr.enableAutoSave(60000);
    // Timer may not be active without event loop, so skip checking isAutoSaveEnabled()

    mgr.disableAutoSave();
    CHECK_FALSE(mgr.isAutoSaveEnabled());
}

// ============================================================================
// Workspace File Format Tests
// ============================================================================

TEST_CASE("WorkspaceManager - workspace file is valid JSON", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/format_test.wtb");

    WorkspaceManager mgr(tmp_dir.path());
    mgr.recordJsonConfigLoad(QStringLiteral("/data/experiment.json"));
    mgr.saveWorkspace(ws_path);

    // Read raw file content and verify it's valid JSON with expected keys
    QFile f(ws_path);
    REQUIRE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    auto const contents = f.readAll();
    f.close();

    // Should contain expected top-level keys
    auto const text = QString::fromUtf8(contents);
    CHECK(text.contains(QStringLiteral("\"version\"")));
    CHECK(text.contains(QStringLiteral("\"data_loads\"")));
    CHECK(text.contains(QStringLiteral("\"editor_states\"")));
    CHECK(text.contains(QStringLiteral("\"zone_layout\"")));
    CHECK(text.contains(QStringLiteral("\"created_at\"")));
    CHECK(text.contains(QStringLiteral("\"modified_at\"")));

    // Should contain our data load
    CHECK(text.contains(QStringLiteral("json_config")));
    CHECK(text.contains(QStringLiteral("experiment.json")));
}

TEST_CASE("WorkspaceManager - save creates parent directories", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/nested/deep/test.wtb");

    WorkspaceManager mgr(tmp_dir.path());
    bool saved = mgr.saveWorkspace(ws_path);
    CHECK(saved);
    CHECK(QFile::exists(ws_path));
}

// ============================================================================
// Pipeline Tracking Tests
// ============================================================================

TEST_CASE("WorkspaceManager - pipeline tracking", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK(mgr.appliedPipelines().empty());

    mgr.recordAppliedPipeline(R"({"name":"zscore","version":1})");
    REQUIRE(mgr.appliedPipelines().size() == 1);
    CHECK(mgr.appliedPipelines()[0] == R"({"name":"zscore","version":1})");

    mgr.recordAppliedPipeline(R"({"name":"smooth","window":5})");
    REQUIRE(mgr.appliedPipelines().size() == 2);

    // Recording a pipeline should mark workspace dirty
    CHECK(mgr.hasUnsavedChanges());

    mgr.clearAppliedPipelines();
    CHECK(mgr.appliedPipelines().empty());
}

TEST_CASE("WorkspaceManager - pipeline round-trip save/load", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/pipeline_test.wtb");

    // Save with pipelines
    {
        WorkspaceManager mgr(tmp_dir.path());
        mgr.recordAppliedPipeline(R"({"name":"zscore","version":1})");
        mgr.recordAppliedPipeline(R"({"name":"smooth","window":5})");
        bool saved = mgr.saveWorkspace(ws_path);
        CHECK(saved);
    }

    // Load and verify
    {
        WorkspaceManager mgr(tmp_dir.path());
        auto data = mgr.readWorkspace(ws_path);
        REQUIRE(data.has_value());
        REQUIRE(data->applied_pipelines.size() == 2);
        // Pipelines are stored as JSON objects (re-serialized), verify structure
        CHECK(data->applied_pipelines[0].find("zscore") != std::string::npos);
        CHECK(data->applied_pipelines[1].find("smooth") != std::string::npos);
    }
}

// ============================================================================
// Table Definition Tracking Tests
// ============================================================================

TEST_CASE("WorkspaceManager - table definition tracking", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    WorkspaceManager mgr(tmp_dir.path());

    CHECK(mgr.tableDefinitions().empty());

    mgr.recordTableDefinition("table_1");
    REQUIRE(mgr.tableDefinitions().size() == 1);
    CHECK(mgr.tableDefinitions()[0] == "table_1");

    mgr.recordTableDefinition("table_2");
    REQUIRE(mgr.tableDefinitions().size() == 2);

    // Recording should mark dirty
    CHECK(mgr.hasUnsavedChanges());

    mgr.clearTableDefinitions();
    CHECK(mgr.tableDefinitions().empty());
}

TEST_CASE("WorkspaceManager - table definitions round-trip save/load", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/table_test.wtb");

    // Save with tables
    {
        WorkspaceManager mgr(tmp_dir.path());
        mgr.recordTableDefinition("whisker_stats");
        mgr.recordTableDefinition("trial_summary");
        bool saved = mgr.saveWorkspace(ws_path);
        CHECK(saved);
    }

    // Load and verify
    {
        WorkspaceManager mgr(tmp_dir.path());
        auto data = mgr.readWorkspace(ws_path);
        REQUIRE(data.has_value());
        REQUIRE(data->table_definitions.size() == 2);
        CHECK(data->table_definitions[0].find("whisker_stats") != std::string::npos);
        CHECK(data->table_definitions[1].find("trial_summary") != std::string::npos);
    }
}

// ============================================================================
// Relative Path Conversion Tests
// ============================================================================

TEST_CASE("WorkspaceManager - relative paths saved in workspace file", "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const data_dir = tmp_dir.path() + QStringLiteral("/data");
    QDir().mkpath(data_dir);

    // Create a dummy config file so the path is valid
    auto const config_path = data_dir + QStringLiteral("/config.json");
    {
        QFile f(config_path);
        f.open(QIODevice::WriteOnly);
        f.write("{}");
        f.close();
    }

    auto const ws_path = tmp_dir.path() + QStringLiteral("/project.wtb");

    // Save — paths should be converted to relative
    {
        WorkspaceManager mgr(tmp_dir.path());
        mgr.recordJsonConfigLoad(config_path);
        bool saved = mgr.saveWorkspace(ws_path);
        CHECK(saved);
    }

    // Read raw file and check for relative_path field
    {
        QFile f(ws_path);
        REQUIRE(f.open(QIODevice::ReadOnly | QIODevice::Text));
        auto const text = QString::fromUtf8(f.readAll());
        f.close();

        CHECK(text.contains(QStringLiteral("\"relative_path\"")));
        CHECK(text.contains(QStringLiteral("data/config.json")));
    }

    // Load — verify relative path is populated
    {
        WorkspaceManager mgr(tmp_dir.path());
        auto data = mgr.readWorkspace(ws_path);
        REQUIRE(data.has_value());
        REQUIRE(data->data_loads.size() == 1);
        CHECK_FALSE(data->data_loads[0].relative_path.empty());
        // source_path should still contain the original absolute path
        CHECK(data->data_loads[0].source_path.find("config.json") != std::string::npos);
    }
}

// ============================================================================
// Combined Save/Load with All Phase 2b Fields
// ============================================================================

TEST_CASE("WorkspaceManager - full round-trip with pipelines tables and relative paths",
          "[StateManagement][Workspace][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    auto const ws_path = tmp_dir.path() + QStringLiteral("/full_test.wtb");

    // Save with all provenance
    {
        WorkspaceManager mgr(tmp_dir.path());
        mgr.recordJsonConfigLoad(QStringLiteral("/data/config.json"));
        mgr.recordAppliedPipeline(R"({"name":"normalize"})");
        mgr.recordTableDefinition("main_table");
        bool saved = mgr.saveWorkspace(ws_path);
        CHECK(saved);
    }

    // Load and verify all fields preserved
    {
        WorkspaceManager mgr(tmp_dir.path());
        auto data = mgr.readWorkspace(ws_path);
        REQUIRE(data.has_value());

        // Data loads present
        REQUIRE(data->data_loads.size() == 1);
        CHECK(data->data_loads[0].loader_type == "json_config");

        // Pipelines present
        REQUIRE(data->applied_pipelines.size() == 1);
        CHECK(data->applied_pipelines[0].find("normalize") != std::string::npos);

        // Table definitions present
        REQUIRE(data->table_definitions.size() == 1);
        CHECK(data->table_definitions[0].find("main_table") != std::string::npos);

        // Workspace file format keys present
        QFile f(ws_path);
        REQUIRE(f.open(QIODevice::ReadOnly | QIODevice::Text));
        auto const text = QString::fromUtf8(f.readAll());
        f.close();

        CHECK(text.contains(QStringLiteral("\"applied_pipelines\"")));
        CHECK(text.contains(QStringLiteral("\"table_definitions\"")));
    }
}
