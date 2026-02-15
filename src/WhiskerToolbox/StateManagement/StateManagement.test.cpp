/**
 * @file StateManagement.test.cpp
 * @brief Integration tests for AppPreferences, SessionStore, and StateManager
 *
 * These tests require Qt (for QObject, QTimer, signal/slot) and use
 * a temporary directory for file I/O.
 */

#include <catch2/catch_test_macros.hpp>

#include "StateManagement/AppPreferences.hpp"
#include "StateManagement/SessionStore.hpp"
#include "StateManagement/StateManager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

using namespace StateManagement;

// Helper to process pending events (needed for QTimer-based debounce)
static void processEvents() {
    QCoreApplication::processEvents();
}

// ============================================================================
// AppPreferences Tests
// ============================================================================

TEST_CASE("AppPreferences - save and load round trip", "[StateManagement][AppPreferences][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    // Save
    {
        AppPreferences prefs(tmp_dir.path());
        prefs.setPythonEnvSearchPaths({"/usr/bin", "/opt/python"});
        prefs.setPreferredPythonEnv("/usr/bin/python3");
        prefs.setDefaultImportDirectory("/data/import");
        prefs.setDefaultExportDirectory("/data/export");
        prefs.setDefaultTimeFrameKey("absolute_time");
        prefs.save();
    }

    // Load
    {
        AppPreferences prefs(tmp_dir.path());
        prefs.load();

        CHECK(prefs.pythonEnvSearchPaths().size() == 2);
        CHECK(prefs.pythonEnvSearchPaths()[0] == "/usr/bin");
        CHECK(prefs.pythonEnvSearchPaths()[1] == "/opt/python");
        CHECK(prefs.preferredPythonEnv() == "/usr/bin/python3");
        CHECK(prefs.defaultImportDirectory() == "/data/import");
        CHECK(prefs.defaultExportDirectory() == "/data/export");
        CHECK(prefs.defaultTimeFrameKey() == "absolute_time");
    }
}

TEST_CASE("AppPreferences - load nonexistent file uses defaults", "[StateManagement][AppPreferences][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    AppPreferences prefs(tmp_dir.path());
    prefs.load(); // No file exists — should not crash

    CHECK(prefs.pythonEnvSearchPaths().empty());
    CHECK(prefs.preferredPythonEnv().empty());
    CHECK(prefs.defaultImportDirectory().empty());
}

TEST_CASE("AppPreferences - signal emission on change", "[StateManagement][AppPreferences][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    AppPreferences prefs(tmp_dir.path());

    int signal_count = 0;
    QString last_key;
    QObject::connect(&prefs, &AppPreferences::preferenceChanged,
                     [&](QString key) {
                         signal_count++;
                         last_key = key;
                     });

    prefs.setDefaultImportDirectory("/new/path");
    CHECK(signal_count == 1);
    CHECK(last_key == "default_import_directory");

    // Same value → no signal
    prefs.setDefaultImportDirectory("/new/path");
    CHECK(signal_count == 1);

    // Different value → signal
    prefs.setDefaultImportDirectory("/other/path");
    CHECK(signal_count == 2);
}

TEST_CASE("AppPreferences - file path is correct", "[StateManagement][AppPreferences][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    AppPreferences prefs(tmp_dir.path());
    CHECK(prefs.filePath() == tmp_dir.path() + "/preferences.json");
}

// ============================================================================
// SessionStore Tests
// ============================================================================

TEST_CASE("SessionStore - path memory save and load", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    // Save
    {
        SessionStore session(tmp_dir.path());
        session.rememberPath("import_csv", tmp_dir.path());
        session.rememberPath("load_video", tmp_dir.path());
        session.save();
    }

    // Load
    {
        SessionStore session(tmp_dir.path());
        session.load();

        // tmp_dir path exists, so it should be returned
        CHECK(session.lastUsedPath("import_csv") == tmp_dir.path());
        CHECK(session.lastUsedPath("load_video") == tmp_dir.path());
    }
}

TEST_CASE("SessionStore - lastUsedPath returns fallback for unknown dialog", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    CHECK(session.lastUsedPath("unknown_dialog", "/fallback") == "/fallback");
    CHECK(session.lastUsedPath("unknown_dialog").isEmpty());
}

TEST_CASE("SessionStore - rememberPath stores directory for file paths", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    // Create a temporary file inside the temp dir
    auto const file_path = tmp_dir.path() + "/test_file.csv";
    QFile file(file_path);
    file.open(QIODevice::WriteOnly);
    file.close();

    session.rememberPath("test", file_path);

    // Should store the directory, not the file
    CHECK(session.lastUsedPath("test") == tmp_dir.path());
}

TEST_CASE("SessionStore - recent workspaces", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    session.addRecentWorkspace("/exp01/workspace.whisker");
    session.addRecentWorkspace("/exp02/workspace.whisker");
    session.addRecentWorkspace("/exp03/workspace.whisker");

    auto recent = session.recentWorkspaces();
    REQUIRE(recent.size() == 3);
    // Most recent first
    CHECK(recent[0] == "/exp03/workspace.whisker");
    CHECK(recent[1] == "/exp02/workspace.whisker");
    CHECK(recent[2] == "/exp01/workspace.whisker");
}

TEST_CASE("SessionStore - recent workspaces moves duplicates to front", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    session.addRecentWorkspace("/exp01/ws.whisker");
    session.addRecentWorkspace("/exp02/ws.whisker");
    session.addRecentWorkspace("/exp01/ws.whisker"); // Re-add

    auto recent = session.recentWorkspaces();
    REQUIRE(recent.size() == 2);
    CHECK(recent[0] == "/exp01/ws.whisker"); // Moved to front
    CHECK(recent[1] == "/exp02/ws.whisker");
}

TEST_CASE("SessionStore - recent workspaces capped at max", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    for (int i = 0; i < 15; ++i) {
        session.addRecentWorkspace(QString("/exp%1/ws.whisker").arg(i));
    }

    auto recent = session.recentWorkspaces();
    CHECK(recent.size() == SessionData::max_recent_workspaces);
    // Most recent should be last added
    CHECK(recent[0] == "/exp14/ws.whisker");
}

TEST_CASE("SessionStore - remove recent workspace", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    session.addRecentWorkspace("/exp01/ws.whisker");
    session.addRecentWorkspace("/exp02/ws.whisker");
    session.removeRecentWorkspace("/exp01/ws.whisker");

    auto recent = session.recentWorkspaces();
    REQUIRE(recent.size() == 1);
    CHECK(recent[0] == "/exp02/ws.whisker");
}

TEST_CASE("SessionStore - window geometry save and load", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    // Save
    {
        SessionStore session(tmp_dir.path());
        // Directly set data for testing (no QMainWindow needed)
        auto & data = const_cast<SessionData &>(session.data());
        data.window_x = 50;
        data.window_y = 75;
        data.window_width = 1600;
        data.window_height = 1200;
        data.window_maximized = false;
        session.save();
    }

    // Load
    {
        SessionStore session(tmp_dir.path());
        session.load();

        auto const & data = session.data();
        CHECK(data.window_x == 50);
        CHECK(data.window_y == 75);
        CHECK(data.window_width == 1600);
        CHECK(data.window_height == 1200);
        CHECK_FALSE(data.window_maximized);
    }
}

TEST_CASE("SessionStore - signal emission", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    int signal_count = 0;
    QObject::connect(&session, &SessionStore::sessionChanged,
                     [&]() { signal_count++; });

    session.rememberPath("test", tmp_dir.path());
    CHECK(signal_count == 1);

    session.addRecentWorkspace("/exp01/ws.whisker");
    CHECK(signal_count == 2);
}

// ============================================================================
// StateManager Tests
// ============================================================================

TEST_CASE("StateManager - construction with explicit dir", "[StateManagement][StateManager][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    StateManager mgr(tmp_dir.path());

    REQUIRE(mgr.preferences() != nullptr);
    REQUIRE(mgr.session() != nullptr);
    CHECK(mgr.configDir() == tmp_dir.path());
}

TEST_CASE("StateManager - loadAll and saveAll", "[StateManagement][StateManager][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    // Save
    {
        StateManager mgr(tmp_dir.path());
        mgr.loadAll();
        mgr.preferences()->setDefaultImportDirectory("/test/path");
        mgr.session()->addRecentWorkspace("/test/workspace.whisker");
        mgr.saveAll();
    }

    // Load
    {
        StateManager mgr(tmp_dir.path());
        mgr.loadAll();

        CHECK(mgr.preferences()->defaultImportDirectory() == "/test/path");
        auto recent = mgr.session()->recentWorkspaces();
        REQUIRE(recent.size() == 1);
        CHECK(recent[0] == "/test/workspace.whisker");
    }
}

TEST_CASE("SessionStore - rememberPath ignores empty path", "[StateManagement][Session][Qt]") {
    QTemporaryDir tmp_dir;
    REQUIRE(tmp_dir.isValid());

    SessionStore session(tmp_dir.path());

    int signal_count = 0;
    QObject::connect(&session, &SessionStore::sessionChanged,
                     [&]() { signal_count++; });

    session.rememberPath("test", ""); // Empty path should be ignored
    CHECK(signal_count == 0);
    CHECK(session.lastUsedPath("test").isEmpty());
}
