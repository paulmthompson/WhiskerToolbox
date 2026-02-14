/**
 * @file Phase5.test.cpp
 * @brief Phase 5 tests: working directory, sys.argv, auto-import prelude
 *
 * Tests PythonEngine features added in Phase 5:
 * - setWorkingDirectory / getWorkingDirectory
 * - setSysArgv
 * - executePrelude
 *
 * These tests share a single PythonEngine instance (Python can only be
 * initialized/finalized once per process).
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

} // anonymous namespace

// ===========================================================================
// Working Directory Tests
// ===========================================================================

TEST_CASE("Phase5: getWorkingDirectory returns non-empty path", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto cwd = eng.getWorkingDirectory();
    REQUIRE(!cwd.empty());
}

TEST_CASE("Phase5: setWorkingDirectory changes cwd", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto const original = eng.getWorkingDirectory();

    // Change to /tmp (should always exist on Linux)
    std::filesystem::path const tmp = std::filesystem::temp_directory_path();
    eng.setWorkingDirectory(tmp);

    auto const new_cwd = eng.getWorkingDirectory();
    REQUIRE(std::filesystem::equivalent(new_cwd, tmp));

    // Python's os.getcwd() also agrees
    auto result = eng.execute("import os; print(os.getcwd(), end='')");
    REQUIRE(result.success);
    REQUIRE(std::filesystem::equivalent(result.stdout_text, tmp.string()));

    // Restore
    eng.setWorkingDirectory(original);
}

TEST_CASE("Phase5: setWorkingDirectory with empty path is no-op", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto const before = eng.getWorkingDirectory();
    eng.setWorkingDirectory({});
    auto const after = eng.getWorkingDirectory();
    REQUIRE(before == after);
}

TEST_CASE("Phase5: setWorkingDirectory with nonexistent path is no-op", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto const before = eng.getWorkingDirectory();
    eng.setWorkingDirectory("/this/path/definitely/does/not/exist/phase5test");
    auto const after = eng.getWorkingDirectory();
    REQUIRE(before == after);
}

// ===========================================================================
// sys.argv Tests
// ===========================================================================

TEST_CASE("Phase5: setSysArgv sets sys.argv", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    eng.setSysArgv("script.py --input data.csv --verbose");

    auto result = eng.execute("import sys; print(sys.argv)");
    REQUIRE(result.success);
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("script.py"));
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("--input"));
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("data.csv"));
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("--verbose"));
}

TEST_CASE("Phase5: setSysArgv with empty string sets empty argv", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    eng.setSysArgv("");

    auto result = eng.execute("import sys; print(len(sys.argv), end='')");
    REQUIRE(result.success);
    REQUIRE(result.stdout_text == "0");
}

TEST_CASE("Phase5: setSysArgv handles quoted strings", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    eng.setSysArgv(R"(script.py "hello world" --name "John Doe")");

    auto result = eng.execute("import sys; print(len(sys.argv), end='')");
    REQUIRE(result.success);
    // Should be 4: script.py, "hello world", --name, "John Doe"
    REQUIRE(result.stdout_text == "4");

    auto result2 = eng.execute("import sys; print(sys.argv[1], end='')");
    REQUIRE(result2.success);
    REQUIRE(result2.stdout_text == "hello world");
}

// ===========================================================================
// Auto-Import Prelude Tests
// ===========================================================================

TEST_CASE("Phase5: executePrelude runs code", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude("PRELUDE_VAR = 42");
    REQUIRE(result.success);

    // Variable should be accessible
    auto check = eng.execute("print(PRELUDE_VAR, end='')");
    REQUIRE(check.success);
    REQUIRE(check.stdout_text == "42");
}

TEST_CASE("Phase5: executePrelude with empty string is no-op success", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude("");
    REQUIRE(result.success);
}

TEST_CASE("Phase5: executePrelude with import statement", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude("import os\nimport sys");
    REQUIRE(result.success);

    // os and sys should be available
    auto check = eng.execute("print(os.name, end='')");
    REQUIRE(check.success);
    REQUIRE(!check.stdout_text.empty());
}

TEST_CASE("Phase5: executePrelude with whiskertoolbox import", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude("from whiskertoolbox_python import *");
    REQUIRE(result.success);

    // AnalogTimeSeries should be available
    auto check = eng.execute("print(type(AnalogTimeSeries).__name__, end='')");
    REQUIRE(check.success);
    REQUIRE_THAT(check.stdout_text, ContainsSubstring("pybind11_type"));
}

TEST_CASE("Phase5: executePrelude error does not break engine", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude("import nonexistent_module_xyz_123");
    REQUIRE(!result.success);
    REQUIRE_THAT(result.stderr_text, ContainsSubstring("ModuleNotFoundError"));

    // Engine should still work after prelude error
    auto check = eng.execute("print(1 + 1, end='')");
    REQUIRE(check.success);
    REQUIRE(check.stdout_text == "2");
}

TEST_CASE("Phase5: executePrelude with multiline code", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto result = eng.executePrelude(
        "import os\n"
        "import sys\n"
        "PRELUDE_X = 100\n"
        "PRELUDE_Y = 200\n"
        "def prelude_add(a, b):\n"
        "    return a + b\n"
    );
    REQUIRE(result.success);

    auto check = eng.execute("print(prelude_add(PRELUDE_X, PRELUDE_Y), end='')");
    REQUIRE(check.success);
    REQUIRE(check.stdout_text == "300");
}

// ===========================================================================
// Integration: Bridge + Phase 5 Features
// ===========================================================================

TEST_CASE("Phase5: Bridge exposeDataManager + prelude + sys.argv together", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    auto dm = std::make_shared<DataManager>();
    PythonBridge bridge(dm, eng);

    // Expose dm
    bridge.exposeDataManager();

    // Run prelude
    auto prelude_result = eng.executePrelude("from whiskertoolbox_python import *");
    REQUIRE(prelude_result.success);

    // Set argv
    eng.setSysArgv("analysis.py --key test_data");

    // Verify all are accessible
    auto check = eng.execute(
        "import sys\n"
        "assert dm is not None\n"
        "assert 'AnalogTimeSeries' in dir()\n"
        "assert sys.argv[0] == 'analysis.py'\n"
        "print('all_ok', end='')"
    );
    REQUIRE(check.success);
    REQUIRE(check.stdout_text == "all_ok");
}

TEST_CASE("Phase5: Working directory affects executeFile", "[python][phase5]") {
    auto & eng = engine();
    eng.resetNamespace();

    // Create a temp script
    auto const tmp_dir = std::filesystem::temp_directory_path() / "wt_phase5_test";
    std::filesystem::create_directories(tmp_dir);
    auto const script_path = tmp_dir / "test_cwd.py";

    {
        std::ofstream f(script_path);
        f << "import os\n"
          << "print(os.getcwd(), end='')\n";
    }

    auto result = eng.executeFile(script_path);
    REQUIRE(result.success);
    // executeFile temporarily changes to script's parent dir
    REQUIRE(std::filesystem::equivalent(result.stdout_text, tmp_dir.string()));

    // Cleanup
    std::filesystem::remove(script_path);
    std::filesystem::remove(tmp_dir);
}
