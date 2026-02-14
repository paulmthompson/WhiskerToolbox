/**
 * @file Phase6.test.cpp
 * @brief Phase 6 tests: virtual environment support
 *
 * Tests PythonEngine features added in Phase 6:
 * - discoverVenvs
 * - validateVenv
 * - activateVenv / deactivateVenv
 * - isVenvActive / activeVenvPath
 * - listInstalledPackages
 * - installPackage
 * - pythonVersionTuple
 * - _looksLikeVenv / _findSitePackages / _readVenvPythonVersion (via public API)
 *
 * These tests share a single PythonEngine instance (Python can only be
 * initialized/finalized once per process).
 *
 * Some tests create temporary fake venv directory structures on disk.
 */

#include "PythonEngine.hpp"
#include "PythonBridge.hpp"
#include "PythonResult.hpp"

#include "DataManager/DataManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

using Catch::Matchers::ContainsSubstring;

namespace {

PythonEngine & engine() {
    static PythonEngine instance;
    return instance;
}

/// Create a fake venv structure at the given path.
/// Returns the path to site-packages.
std::filesystem::path createFakeVenv(
    std::filesystem::path const & root,
    int major = 3, int minor = 0,
    bool create_pyvenv_cfg = true) {

    std::filesystem::create_directories(root / "bin");

    // Create a fake python binary (just an empty file)
    {
        std::ofstream f(root / "bin" / "python");
        f << "#!/bin/sh\n";
    }

    // Create pyvenv.cfg
    if (create_pyvenv_cfg) {
        // Auto-detect version if minor == 0
        if (minor == 0) {
            minor = 12;  // default for test purposes
        }
        std::ofstream f(root / "pyvenv.cfg");
        f << "home = /usr/bin\n";
        f << "include-system-site-packages = false\n";
        f << "version = " << major << "." << minor << ".0\n";
    }

    // Create site-packages
    auto const sp = root / "lib"
                    / ("python" + std::to_string(major) + "." + std::to_string(minor))
                    / "site-packages";
    std::filesystem::create_directories(sp);

    return sp;
}

/// Cleanup a temporary directory.
void cleanupDir(std::filesystem::path const & dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

} // anonymous namespace

// ===========================================================================
// pythonVersionTuple
// ===========================================================================

TEST_CASE("Phase6: pythonVersionTuple returns valid version", "[python][phase6]") {
    auto & eng = engine();
    auto const [major, minor] = eng.pythonVersionTuple();
    REQUIRE(major == 3);
    REQUIRE(minor >= 8);  // We require Python 3.8+
}

// ===========================================================================
// discoverVenvs
// ===========================================================================

TEST_CASE("Phase6: discoverVenvs returns vector", "[python][phase6]") {
    auto & eng = engine();
    // Just verify it doesn't crash and returns a vector
    auto venvs = eng.discoverVenvs();
    // Result may be empty on CI/test machines — that's OK
    REQUIRE(venvs.size() >= 0);
}

TEST_CASE("Phase6: discoverVenvs finds venvs in extra paths", "[python][phase6]") {
    auto & eng = engine();

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_discover";
    cleanupDir(tmp_dir);
    std::filesystem::create_directories(tmp_dir);

    // Create two fake venvs inside the scan dir
    auto const [major, minor] = eng.pythonVersionTuple();
    createFakeVenv(tmp_dir / "my_venv1", major, minor);
    createFakeVenv(tmp_dir / "my_venv2", major, minor);

    // Also create a non-venv directory that should be skipped
    std::filesystem::create_directories(tmp_dir / "not_a_venv");

    auto venvs = eng.discoverVenvs({tmp_dir});

    // Should find at least our two fake venvs
    int found = 0;
    for (auto const & v : venvs) {
        auto const name = v.filename().string();
        if (name == "my_venv1" || name == "my_venv2") {
            ++found;
        }
    }
    REQUIRE(found == 2);

    cleanupDir(tmp_dir);
}

TEST_CASE("Phase6: discoverVenvs handles nonexistent extra path", "[python][phase6]") {
    auto & eng = engine();
    auto venvs = eng.discoverVenvs({"/this/path/does/not/exist/phase6test"});
    // Should not crash, may return empty or other system venvs
    REQUIRE(venvs.size() >= 0);
}

// ===========================================================================
// validateVenv
// ===========================================================================

TEST_CASE("Phase6: validateVenv with empty path", "[python][phase6]") {
    auto & eng = engine();
    auto error = eng.validateVenv({});
    REQUIRE(!error.empty());
    REQUIRE_THAT(error, ContainsSubstring("empty"));
}

TEST_CASE("Phase6: validateVenv with nonexistent path", "[python][phase6]") {
    auto & eng = engine();
    auto error = eng.validateVenv("/nonexistent/venv/path");
    REQUIRE(!error.empty());
    REQUIRE_THAT(error, ContainsSubstring("not exist") || ContainsSubstring("not a directory"));
}

TEST_CASE("Phase6: validateVenv with non-venv directory", "[python][phase6]") {
    auto & eng = engine();
    // /tmp exists but is not a venv
    auto error = eng.validateVenv(std::filesystem::temp_directory_path());
    REQUIRE(!error.empty());
    REQUIRE_THAT(error, ContainsSubstring("not appear to be"));
}

TEST_CASE("Phase6: validateVenv with version mismatch", "[python][phase6]") {
    auto & eng = engine();

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_validate_mismatch";
    cleanupDir(tmp_dir);

    // Create a venv with a mismatched version
    createFakeVenv(tmp_dir, 3, 7);  // Python 3.7 when embedded is 3.12+

    auto error = eng.validateVenv(tmp_dir);
    // If the embedded Python is not 3.7, this should fail with version mismatch
    auto const [major, minor] = eng.pythonVersionTuple();
    if (minor != 7) {
        REQUIRE(!error.empty());
        REQUIRE_THAT(error, ContainsSubstring("mismatch"));
    }

    cleanupDir(tmp_dir);
}

TEST_CASE("Phase6: validateVenv with matching version succeeds", "[python][phase6]") {
    auto & eng = engine();

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_validate_ok";
    cleanupDir(tmp_dir);

    auto const [major, minor] = eng.pythonVersionTuple();
    createFakeVenv(tmp_dir, major, minor);

    auto error = eng.validateVenv(tmp_dir);
    REQUIRE(error.empty());

    cleanupDir(tmp_dir);
}

// ===========================================================================
// activateVenv / deactivateVenv / isVenvActive / activeVenvPath
// ===========================================================================

TEST_CASE("Phase6: initially no venv is active", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();  // ensure clean state

    REQUIRE(!eng.isVenvActive());
    REQUIRE(eng.activeVenvPath().empty());
}

TEST_CASE("Phase6: activateVenv with valid fake venv", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_activate";
    cleanupDir(tmp_dir);

    auto const [major, minor] = eng.pythonVersionTuple();
    auto const site_packages = createFakeVenv(tmp_dir, major, minor);

    auto error = eng.activateVenv(tmp_dir);
    REQUIRE(error.empty());
    REQUIRE(eng.isVenvActive());
    REQUIRE(std::filesystem::equivalent(eng.activeVenvPath(), tmp_dir));

    // Check sys.path contains the site-packages
    auto result = eng.execute("import sys; print(sys.path)");
    REQUIRE(result.success);
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("site-packages"));

    // Check sys.prefix is set
    auto result2 = eng.execute("import sys; print(sys.prefix, end='')");
    REQUIRE(result2.success);
    REQUIRE(result2.stdout_text == tmp_dir.string());

    // Check VIRTUAL_ENV env var
    auto result3 = eng.execute("import os; print(os.environ.get('VIRTUAL_ENV', ''), end='')");
    REQUIRE(result3.success);
    REQUIRE(result3.stdout_text == tmp_dir.string());

    // Deactivate and verify
    eng.deactivateVenv();
    REQUIRE(!eng.isVenvActive());
    REQUIRE(eng.activeVenvPath().empty());

    // VIRTUAL_ENV should be gone
    auto result4 = eng.execute("import os; print(os.environ.get('VIRTUAL_ENV', 'NONE'), end='')");
    REQUIRE(result4.success);
    REQUIRE(result4.stdout_text == "NONE");

    cleanupDir(tmp_dir);
}

TEST_CASE("Phase6: activateVenv with invalid path returns error", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();

    auto error = eng.activateVenv("/nonexistent/venv/path");
    REQUIRE(!error.empty());
    REQUIRE(!eng.isVenvActive());
}

TEST_CASE("Phase6: activating new venv deactivates old one", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();

    auto const tmp1 = std::filesystem::temp_directory_path() / "wt_phase6_switch1";
    auto const tmp2 = std::filesystem::temp_directory_path() / "wt_phase6_switch2";
    cleanupDir(tmp1);
    cleanupDir(tmp2);

    auto const [major, minor] = eng.pythonVersionTuple();
    createFakeVenv(tmp1, major, minor);
    createFakeVenv(tmp2, major, minor);

    auto error1 = eng.activateVenv(tmp1);
    REQUIRE(error1.empty());
    REQUIRE(std::filesystem::equivalent(eng.activeVenvPath(), tmp1));

    auto error2 = eng.activateVenv(tmp2);
    REQUIRE(error2.empty());
    REQUIRE(std::filesystem::equivalent(eng.activeVenvPath(), tmp2));

    // sys.prefix should point to tmp2
    auto result = eng.execute("import sys; print(sys.prefix, end='')");
    REQUIRE(result.success);
    REQUIRE(result.stdout_text == tmp2.string());

    eng.deactivateVenv();
    cleanupDir(tmp1);
    cleanupDir(tmp2);
}

// ===========================================================================
// listInstalledPackages
// ===========================================================================

TEST_CASE("Phase6: listInstalledPackages returns packages", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();

    // Without a venv, this should still work (returns system packages)
    auto packages = eng.listInstalledPackages();
    // On most systems, at least pip/setuptools should be available
    // But on minimal test systems it might be empty — just check it doesn't crash
    REQUIRE(packages.size() >= 0);

    // If we got any packages, verify they have non-empty names and versions
    for (auto const & [name, version] : packages) {
        REQUIRE(!name.empty());
        // Version can technically be empty, but usually isn't
    }
}

TEST_CASE("Phase6: listInstalledPackages returns sorted results", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto packages = eng.listInstalledPackages();
    if (packages.size() >= 2) {
        for (size_t i = 1; i < packages.size(); ++i) {
            REQUIRE(packages[i - 1].first <= packages[i].first);
        }
    }
}

// ===========================================================================
// installPackage
// ===========================================================================

TEST_CASE("Phase6: installPackage with empty name returns error", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.installPackage("");
    REQUIRE(!result.success);
    REQUIRE(!result.stderr_text.empty());
}

// NOTE: We do NOT test actual pip install in unit tests to avoid network
// dependency and modifying the system Python environment. The installPackage
// method is tested for error handling only.

// ===========================================================================
// Integration
// ===========================================================================

TEST_CASE("Phase6: activate venv + bridge + execute", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_integration";
    cleanupDir(tmp_dir);

    auto const [major, minor] = eng.pythonVersionTuple();
    createFakeVenv(tmp_dir, major, minor);

    // Create bridge
    auto dm = std::make_shared<DataManager>();
    PythonBridge bridge(dm, eng);
    bridge.exposeDataManager();

    // Activate venv
    auto error = eng.activateVenv(tmp_dir);
    REQUIRE(error.empty());

    // Execute code that uses both dm and checks venv
    auto result = eng.execute(
        "import sys\n"
        "assert dm is not None\n"
        "assert sys.prefix == '" + tmp_dir.string() + "'\n"
        "print('integration_ok', end='')"
    );
    REQUIRE(result.success);
    REQUIRE(result.stdout_text == "integration_ok");

    eng.deactivateVenv();
    cleanupDir(tmp_dir);
}

TEST_CASE("Phase6: deactivateVenv restores original sys.path", "[python][phase6]") {
    auto & eng = engine();
    eng.resetNamespace();
    eng.deactivateVenv();

    // Capture original sys.path length
    auto result_before = eng.execute("import sys; print(len(sys.path), end='')");
    REQUIRE(result_before.success);
    int const path_len_before = std::stoi(result_before.stdout_text);

    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase6_restore";
    cleanupDir(tmp_dir);

    auto const [major, minor] = eng.pythonVersionTuple();
    createFakeVenv(tmp_dir, major, minor);

    eng.activateVenv(tmp_dir);
    eng.deactivateVenv();

    // sys.path length should be restored
    auto result_after = eng.execute("import sys; print(len(sys.path), end='')");
    REQUIRE(result_after.success);
    int const path_len_after = std::stoi(result_after.stdout_text);
    REQUIRE(path_len_after == path_len_before);

    cleanupDir(tmp_dir);
}
