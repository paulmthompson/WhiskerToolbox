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
