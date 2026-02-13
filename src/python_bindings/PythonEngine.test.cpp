#include "PythonEngine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// Python can only be initialized/finalized once per process.  All tests
// share a single PythonEngine and call resetNamespace() for isolation.
// ---------------------------------------------------------------------------
static PythonEngine & getEngine() {
    static PythonEngine engine;
    return engine;
}

/// RAII helper – resets the namespace before each logical test.
struct EngineFixture {
    PythonEngine & engine;
    EngineFixture() : engine(getEngine()) { engine.resetNamespace(); }
};

// ===== Initialization =====================================================

TEST_CASE("PythonEngine initializes successfully", "[PythonEngine][init]") {
    auto & engine = getEngine();
    REQUIRE(engine.isInitialized());
}

TEST_CASE("PythonEngine reports Python version", "[PythonEngine][init]") {
    auto & engine = getEngine();
    auto version = engine.pythonVersion();
    REQUIRE_FALSE(version.empty());
    REQUIRE_FALSE(version == "N/A");
    REQUIRE_THAT(version, ContainsSubstring("."));
}

// ===== Execution ==========================================================

TEST_CASE("PythonEngine execute captures stdout", "[PythonEngine][execute]") {
    EngineFixture fix;

    SECTION("Simple print") {
        auto result = fix.engine.execute("print(2 + 2)");
        REQUIRE(result.success);
        REQUIRE_THAT(result.stdout_text, ContainsSubstring("4"));
        REQUIRE(result.stderr_text.empty());
    }

    SECTION("Multiple prints") {
        auto result = fix.engine.execute("print('hello')\nprint('world')");
        REQUIRE(result.success);
        REQUIRE_THAT(result.stdout_text, ContainsSubstring("hello"));
        REQUIRE_THAT(result.stdout_text, ContainsSubstring("world"));
    }

    SECTION("Print with end parameter") {
        auto result = fix.engine.execute("print('a', end='')\nprint('b', end='')");
        REQUIRE(result.success);
        REQUIRE(result.stdout_text == "ab");
    }
}

TEST_CASE("PythonEngine execute captures stderr on error", "[PythonEngine][execute]") {
    EngineFixture fix;

    SECTION("SyntaxError") {
        auto result = fix.engine.execute("def f(");
        REQUIRE_FALSE(result.success);
        REQUIRE_THAT(result.stderr_text, ContainsSubstring("SyntaxError"));
    }

    SECTION("ValueError") {
        auto result = fix.engine.execute("raise ValueError('test error')");
        REQUIRE_FALSE(result.success);
        REQUIRE_THAT(result.stderr_text, ContainsSubstring("ValueError"));
        REQUIRE_THAT(result.stderr_text, ContainsSubstring("test error"));
    }

    SECTION("NameError") {
        auto result = fix.engine.execute("print(undefined_variable)");
        REQUIRE_FALSE(result.success);
        REQUIRE_THAT(result.stderr_text, ContainsSubstring("NameError"));
    }
}

// ===== Namespace Persistence ==============================================

TEST_CASE("PythonEngine namespace persists across calls", "[PythonEngine][namespace]") {
    EngineFixture fix;

    SECTION("Variable defined in one call is accessible in the next") {
        auto r1 = fix.engine.execute("x = 42");
        REQUIRE(r1.success);

        auto r2 = fix.engine.execute("print(x)");
        REQUIRE(r2.success);
        REQUIRE_THAT(r2.stdout_text, ContainsSubstring("42"));
    }

    SECTION("Function defined in one call can be called in the next") {
        auto r1 = fix.engine.execute("def greet(name): return f'Hello, {name}!'");
        REQUIRE(r1.success);

        auto r2 = fix.engine.execute("print(greet('World'))");
        REQUIRE(r2.success);
        REQUIRE_THAT(r2.stdout_text, ContainsSubstring("Hello, World!"));
    }

    SECTION("Import persists") {
        auto r1 = fix.engine.execute("import math");
        REQUIRE(r1.success);

        auto r2 = fix.engine.execute("print(math.pi)");
        REQUIRE(r2.success);
        REQUIRE_THAT(r2.stdout_text, ContainsSubstring("3.14159"));
    }
}

// ===== Namespace Reset ====================================================

TEST_CASE("PythonEngine resetNamespace clears user variables", "[PythonEngine][namespace]") {
    EngineFixture fix;

    auto r1 = fix.engine.execute("my_var = 'hello'");
    REQUIRE(r1.success);

    fix.engine.resetNamespace();

    auto r2 = fix.engine.execute("print(my_var)");
    REQUIRE_FALSE(r2.success);
    REQUIRE_THAT(r2.stderr_text, ContainsSubstring("NameError"));
}

// ===== Inject =============================================================

TEST_CASE("PythonEngine inject places object in namespace", "[PythonEngine][inject]") {
    EngineFixture fix;
    namespace py = pybind11;

    fix.engine.inject("injected_value", py::int_(99));

    auto result = fix.engine.execute("print(injected_value)");
    REQUIRE(result.success);
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("99"));
}

// ===== getUserVariableNames ===============================================

TEST_CASE("PythonEngine getUserVariableNames", "[PythonEngine][namespace]") {
    EngineFixture fix;

    (void)fix.engine.execute("alpha = 1\nbeta = 2\ngamma = 3");
    auto names = fix.engine.getUserVariableNames();

    REQUIRE(std::find(names.begin(), names.end(), "alpha") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "beta") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "gamma") != names.end());

    // Internal names should NOT appear
    REQUIRE(std::find(names.begin(), names.end(), "__builtins__") == names.end());
    REQUIRE(std::find(names.begin(), names.end(), "__name__") == names.end());
    REQUIRE(std::find(names.begin(), names.end(), "_wt_stdout") == names.end());
    REQUIRE(std::find(names.begin(), names.end(), "_wt_stderr") == names.end());
}

// ===== input() disabled ===================================================

TEST_CASE("PythonEngine input() is disabled", "[PythonEngine][execute]") {
    EngineFixture fix;

    auto result = fix.engine.execute("x = input('prompt: ')");
    REQUIRE_FALSE(result.success);
    REQUIRE_THAT(result.stderr_text, ContainsSubstring("disabled"));
}

// ===== Edge cases =========================================================

TEST_CASE("PythonEngine handles empty code gracefully", "[PythonEngine][execute]") {
    EngineFixture fix;

    SECTION("Empty string") {
        auto result = fix.engine.execute("");
        REQUIRE(result.success);
        REQUIRE(result.stdout_text.empty());
        REQUIRE(result.stderr_text.empty());
    }

    SECTION("Whitespace only") {
        auto result = fix.engine.execute("   \n\n   ");
        REQUIRE(result.success);
    }

    SECTION("Comment only") {
        auto result = fix.engine.execute("# this is a comment");
        REQUIRE(result.success);
    }
}

// ===== executeFile ========================================================

TEST_CASE("PythonEngine executeFile runs a script", "[PythonEngine][executeFile]") {
    EngineFixture fix;
    namespace fs = std::filesystem;

    auto tmp_dir = fs::temp_directory_path();
    auto script_path = tmp_dir / "wt_test_script.py";

    {
        std::ofstream f(script_path);
        f << "result = 0\n"
          << "for i in range(10):\n"
          << "    result += i\n"
          << "print(result)\n";
    }

    auto result = fix.engine.executeFile(script_path);
    REQUIRE(result.success);
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("45"));

    fs::remove(script_path);
}

TEST_CASE("PythonEngine executeFile reports missing file", "[PythonEngine][executeFile]") {
    EngineFixture fix;

    auto result = fix.engine.executeFile("/nonexistent/path/to/script.py");
    REQUIRE_FALSE(result.success);
    REQUIRE_THAT(result.stderr_text, ContainsSubstring("Could not open file"));
}

// ===== Error recovery =====================================================

TEST_CASE("PythonEngine error does not corrupt state", "[PythonEngine][execute]") {
    EngineFixture fix;

    (void)fix.engine.execute("x = 100");

    auto err_result = fix.engine.execute("raise RuntimeError('boom')");
    REQUIRE_FALSE(err_result.success);

    auto result = fix.engine.execute("print(x)");
    REQUIRE(result.success);
    REQUIRE_THAT(result.stdout_text, ContainsSubstring("100"));
}

TEST_CASE("PythonEngine stdout is isolated between calls", "[PythonEngine][execute]") {
    EngineFixture fix;

    auto r1 = fix.engine.execute("print('first')");
    REQUIRE(r1.success);
    REQUIRE_THAT(r1.stdout_text, ContainsSubstring("first"));

    auto r2 = fix.engine.execute("print('second')");
    REQUIRE(r2.success);
    REQUIRE_THAT(r2.stdout_text, ContainsSubstring("second"));
    REQUIRE(r2.stdout_text.find("first") == std::string::npos);
}
