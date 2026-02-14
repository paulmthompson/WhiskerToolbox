#include "PythonEngine.hpp"

#include "OutputRedirector.hpp"
#include "bind_module.hpp"

#include <pybind11/embed.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Embedded module — registers OutputRedirector so it is available before
// any Python code runs.  The module name is prefixed with an underscore to
// signal it is internal / not user-facing.
// ---------------------------------------------------------------------------
PYBIND11_EMBEDDED_MODULE(_wt_internal, m) {
    bind_output_redirector(m);

    // Provide a custom input() that always raises
    m.def("_disabled_input", [](py::args, py::kwargs) -> py::object {
        throw std::runtime_error(
            "input() is disabled in the WhiskerToolbox embedded Python console.");
    });
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

PythonEngine::PythonEngine()
    : _interpreter(std::make_unique<py::scoped_interpreter>()) {
    // Force the linker to include bindings.o (and all bind_*.o files
    // it references) even when linking from a static archive.
    ensure_whiskertoolbox_bindings_linked();

    try {
        _initNamespace();
        _installRedirectors();

        // Disable input() globally
        py::exec(R"(
import builtins as _b
import _wt_internal
_b.input = _wt_internal._disabled_input
del _b
)",
                 _globals);

        _initialized = true;
    } catch (py::error_already_set & e) {
        // If Python itself fails to start, surface the error but don't crash
        _initialized = false;
        // Can't do much — log to cerr as a fallback
        std::cerr << "PythonEngine: failed to initialize interpreter: "
                  << e.what() << "\n";
    }
}

PythonEngine::~PythonEngine() {
    if (_initialized) {
        // Restore original sys.stdout/stderr so Python's finalization doesn't
        // try to flush our already-destroyed redirector objects.
        try {
            py::module_ sys = py::module_::import("sys");
            sys.attr("stdout") = sys.attr("__stdout__");
            sys.attr("stderr") = sys.attr("__stderr__");
        } catch (...) {
            // Non-fatal — interpreter may already be in a bad state
        }
    }

    _stdout_redirector = nullptr;
    _stderr_redirector = nullptr;

    // IMPORTANT: Do NOT call _interpreter.reset() here. Member destruction
    // order is reverse-of-declaration: _globals (py::dict) is destroyed
    // first, then _interpreter (scoped_interpreter calls Py_Finalize).
    // This ensures Python objects are decref'd while the interpreter is
    // still alive.
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

PythonResult PythonEngine::execute(std::string const & code) {
    PythonResult result;
    if (!_initialized) {
        result.stderr_text = "Python interpreter is not initialized.";
        return result;
    }

    try {
        // Drain any leftover output from a previous run
        (void)_drainOutput();

        // Try to compile as an expression first (REPL behavior).
        // If the code is a single expression like "1+1", eval() it and
        // auto-print the repr() of the result (unless None), just like
        // the interactive Python interpreter does.
        //
        // We use the Python C API (Py_CompileString) instead of
        // exception-based control flow because catching
        // py::error_already_set across DLL boundaries is unreliable
        // on Windows/MSVC.
        PyObject * compiled = Py_CompileString(code.c_str(), "<input>", Py_eval_input);
        if (compiled != nullptr) {
            // Valid expression — evaluate it
            py::object code_obj = py::reinterpret_steal<py::object>(compiled);
            py::object value = py::reinterpret_steal<py::object>(
                PyEval_EvalCode(code_obj.ptr(), _globals.ptr(), _globals.ptr()));
            if (!value) {
                throw py::error_already_set();
            }
            if (!value.is_none()) {
                // Print repr like the interactive interpreter
                py::object builtins = py::module_::import("builtins");
                py::object repr_str = builtins.attr("repr")(value);
                py::print(repr_str);
            }
            // Store the result as '_' in the namespace (standard REPL behavior)
            _globals["_"] = value;
        } else {
            // Not a valid expression — clear the SyntaxError and use exec()
            PyErr_Clear();
            py::exec(code, _globals);
        }
        result.success = true;
    } catch (py::error_already_set & e) {
        result.stderr_text = _formatException(e);
        result.success = false;
    } catch (std::exception const & e) {
        result.stderr_text = std::string("C++ exception: ") + e.what();
        result.success = false;
    }

    auto [out, err] = _drainOutput();
    result.stdout_text = std::move(out);
    if (!err.empty()) {
        if (!result.stderr_text.empty()) {
            result.stderr_text += "\n";
        }
        result.stderr_text += err;
    }

    return result;
}

PythonResult PythonEngine::executeFile(std::filesystem::path const & path) {
    PythonResult result;
    if (!_initialized) {
        result.stderr_text = "Python interpreter is not initialized.";
        return result;
    }

    // Read the file
    std::ifstream file(path);
    if (!file.is_open()) {
        result.stderr_text = "Could not open file: " + path.string();
        return result;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();

    // Temporarily change working directory to script's parent
    auto const parent_dir = path.parent_path();
    std::string restore_cwd_code;
    if (!parent_dir.empty()) {
        try {
            py::module_ os = py::module_::import("os");
            py::object old_cwd = os.attr("getcwd")();
            restore_cwd_code = old_cwd.cast<std::string>();
            os.attr("chdir")(parent_dir.string());
        } catch (...) {
            // Non-fatal — continue without changing directory
        }
    }

    // Set __file__ for the script
    _globals["__file__"] = path.string();

    result = execute(code);

    // Restore working directory
    if (!restore_cwd_code.empty()) {
        try {
            py::module_ os = py::module_::import("os");
            os.attr("chdir")(restore_cwd_code);
        } catch (...) {
            // Non-fatal
        }
    }

    // Clean up __file__
    if (_globals.contains("__file__")) {
        _globals.attr("pop")("__file__", py::none());
    }

    return result;
}

// ---------------------------------------------------------------------------
// Namespace Management
// ---------------------------------------------------------------------------

void PythonEngine::resetNamespace() {
    if (!_initialized) {
        return;
    }
    _globals = py::dict{};
    _initNamespace();
    _installRedirectors();

    // Re-disable input()
    py::exec(R"(
import builtins as _b
import _wt_internal
_b.input = _wt_internal._disabled_input
del _b
)",
             _globals);
}

void PythonEngine::inject(std::string const & name, py::object obj) {
    if (!_initialized) {
        return;
    }
    _globals[name.c_str()] = std::move(obj);
}

// ---------------------------------------------------------------------------
// Environment Configuration (Phase 5)
// ---------------------------------------------------------------------------

void PythonEngine::setWorkingDirectory(std::filesystem::path const & dir) {
    if (!_initialized || dir.empty()) {
        return;
    }
    try {
        py::module_ os = py::module_::import("os");
        os.attr("chdir")(dir.string());
    } catch (py::error_already_set & e) {
        e.restore();
        PyErr_Clear();
    } catch (...) {
        // Non-fatal
    }
}

std::filesystem::path PythonEngine::getWorkingDirectory() const {
    if (!_initialized) {
        return {};
    }
    try {
        py::module_ os = py::module_::import("os");
        return os.attr("getcwd")().cast<std::string>();
    } catch (...) {
        return {};
    }
}

void PythonEngine::setSysArgv(std::string const & args) {
    if (!_initialized) {
        return;
    }
    try {
        // Use Python's shlex.split to properly parse the argument string
        // (handles quoted strings, escape characters, etc.)
        py::module_ shlex = py::module_::import("shlex");
        py::module_ sys = py::module_::import("sys");

        if (args.empty()) {
            sys.attr("argv") = py::list();
        } else {
            sys.attr("argv") = shlex.attr("split")(args);
        }
    } catch (py::error_already_set & e) {
        // Fallback: simple split on whitespace
        e.restore();
        PyErr_Clear();
        try {
            py::module_ sys = py::module_::import("sys");
            py::list argv;

            std::istringstream iss(args);
            std::string token;
            while (iss >> token) {
                argv.append(token);
            }
            sys.attr("argv") = argv;
        } catch (...) {
            // Non-fatal
        }
    }
}

PythonResult PythonEngine::executePrelude(std::string const & prelude) {
    if (!_initialized || prelude.empty()) {
        return PythonResult{.stdout_text = "", .stderr_text = "", .success = true};
    }
    return execute(prelude);
}

// ---------------------------------------------------------------------------
// Virtual Environment Support (Phase 6)
// ---------------------------------------------------------------------------

std::vector<std::filesystem::path>
PythonEngine::discoverVenvs(std::vector<std::filesystem::path> const & extra_paths) const {
    std::vector<std::filesystem::path> result;

    // Collect candidate directories to scan
    std::vector<std::filesystem::path> scan_dirs = extra_paths;

    // Common venv locations
    char const * home = std::getenv("HOME");
    if (home) {
        std::filesystem::path const home_path(home);
        scan_dirs.push_back(home_path / ".virtualenvs");
        scan_dirs.push_back(home_path / ".conda" / "envs");
        scan_dirs.push_back(home_path / ".pyenv" / "versions");
    }

    for (auto const & scan_dir : scan_dirs) {
        if (!std::filesystem::is_directory(scan_dir)) {
            continue;
        }
        try {
            for (auto const & entry : std::filesystem::directory_iterator(scan_dir)) {
                if (entry.is_directory() && _looksLikeVenv(entry.path())) {
                    result.push_back(entry.path());
                }
            }
        } catch (std::filesystem::filesystem_error const &) {
            // Permission denied or similar — skip this scan dir
        }
    }

    // Sort by name for consistent ordering
    std::sort(result.begin(), result.end());
    return result;
}

std::string PythonEngine::validateVenv(std::filesystem::path const & venv_path) const {
    if (venv_path.empty()) {
        return "Virtual environment path is empty.";
    }

    if (!std::filesystem::is_directory(venv_path)) {
        return "Path does not exist or is not a directory: " + venv_path.string();
    }

    if (!_looksLikeVenv(venv_path)) {
        return "Path does not appear to be a Python virtual environment: " + venv_path.string();
    }

    // Check site-packages exists
    auto const site_packages = _findSitePackages(venv_path);
    if (site_packages.empty()) {
        return "Could not locate site-packages directory in: " + venv_path.string();
    }

    // Check version compatibility
    auto const [venv_major, venv_minor] = _readVenvPythonVersion(venv_path);
    if (venv_major > 0) {
        auto const [embed_major, embed_minor] = pythonVersionTuple();
        if (venv_major != embed_major || venv_minor != embed_minor) {
            return "Python version mismatch: embedded interpreter is "
                   + std::to_string(embed_major) + "." + std::to_string(embed_minor)
                   + " but venv uses "
                   + std::to_string(venv_major) + "." + std::to_string(venv_minor)
                   + ". The venv must match the embedded Python version.";
        }
    }

    return {};  // valid
}

std::string PythonEngine::activateVenv(std::filesystem::path const & venv_path) {
    if (!_initialized) {
        return "Python interpreter is not initialized.";
    }

    // Validate first
    auto const error = validateVenv(venv_path);
    if (!error.empty()) {
        return error;
    }

    // Deactivate any existing venv first
    if (isVenvActive()) {
        deactivateVenv();
    }

    auto const site_packages = _findSitePackages(venv_path);
    if (site_packages.empty()) {
        return "Could not locate site-packages in: " + venv_path.string();
    }

    try {
        py::module_ sys = py::module_::import("sys");
        py::module_ os = py::module_::import("os");

        // Save original state for deactivation (deep copy of sys.path)
        py::list current_path = sys.attr("path");
        _original_sys_path = py::list{};
        for (auto const & item : current_path) {
            _original_sys_path.append(item);
        }
        _original_sys_prefix = sys.attr("prefix").cast<std::string>();
        _original_sys_exec_prefix = sys.attr("exec_prefix").cast<std::string>();

        // Prepend site-packages to sys.path
        py::list path = sys.attr("path");
        path.attr("insert")(0, site_packages.string());

        // Also add the venv's lib directory (some packages install there)
        auto const venv_lib = site_packages.parent_path();
        if (std::filesystem::is_directory(venv_lib)) {
            path.attr("insert")(1, venv_lib.string());
        }

        // Set sys.prefix and sys.exec_prefix
        sys.attr("prefix") = venv_path.string();
        sys.attr("exec_prefix") = venv_path.string();

        // Set VIRTUAL_ENV environment variable
        os.attr("environ")["VIRTUAL_ENV"] = venv_path.string();

        // Add venv bin to PATH
        auto const bin_dir = venv_path / "bin";
        if (std::filesystem::is_directory(bin_dir)) {
            try {
                std::string current_path = os.attr("environ")["PATH"].cast<std::string>();
                os.attr("environ")["PATH"] = bin_dir.string() + ":" + current_path;
            } catch (...) {
                os.attr("environ")["PATH"] = bin_dir.string();
            }
        }

        _active_venv_path = venv_path;
        return {};  // success

    } catch (py::error_already_set & e) {
        auto msg = _formatException(e);
        return "Failed to activate venv: " + msg;
    } catch (std::exception const & e) {
        return std::string("Failed to activate venv: ") + e.what();
    }
}

void PythonEngine::deactivateVenv() {
    if (!_initialized || !isVenvActive()) {
        return;
    }

    try {
        py::module_ sys = py::module_::import("sys");
        py::module_ os = py::module_::import("os");

        // Restore sys.path — use a deep copy to avoid aliasing
        if (_original_sys_path.ptr() != nullptr && py::len(_original_sys_path) > 0) {
            py::list restored_path;
            for (auto const & item : _original_sys_path) {
                restored_path.append(item);
            }
            sys.attr("path") = restored_path;
        }

        // Restore sys.prefix and sys.exec_prefix
        if (!_original_sys_prefix.empty()) {
            sys.attr("prefix") = _original_sys_prefix;
        }
        if (!_original_sys_exec_prefix.empty()) {
            sys.attr("exec_prefix") = _original_sys_exec_prefix;
        }

        // Remove VIRTUAL_ENV from environment
        try {
            py::exec(R"(
import os as _os
if 'VIRTUAL_ENV' in _os.environ:
    del _os.environ['VIRTUAL_ENV']
del _os
)", _globals);
        } catch (...) {
            // Non-fatal
        }

    } catch (...) {
        // Non-fatal — best-effort restore
    }

    _active_venv_path.clear();
    _original_sys_path = py::list{};
    _original_sys_prefix.clear();
    _original_sys_exec_prefix.clear();
}

std::filesystem::path PythonEngine::activeVenvPath() const {
    return _active_venv_path;
}

bool PythonEngine::isVenvActive() const {
    return !_active_venv_path.empty();
}

std::vector<std::pair<std::string, std::string>> PythonEngine::listInstalledPackages() const {
    std::vector<std::pair<std::string, std::string>> packages;
    if (!_initialized) {
        return packages;
    }

    try {
        // Use importlib.metadata (Python 3.8+)
        auto result = py::eval(R"(
[(d.metadata["Name"], d.metadata["Version"])
 for d in __import__("importlib.metadata", fromlist=["distributions"]).distributions()]
)",
                               _globals);

        for (auto const & item : result) {
            auto tuple = item.cast<py::tuple>();
            auto name = tuple[0].cast<std::string>();
            auto version = tuple[1].cast<std::string>();
            packages.emplace_back(std::move(name), std::move(version));
        }

        // Sort by package name
        std::sort(packages.begin(), packages.end(),
                  [](auto const & a, auto const & b) { return a.first < b.first; });

    } catch (py::error_already_set & e) {
        e.restore();
        PyErr_Clear();
        // Fallback: try pkg_resources
        try {
            auto result = py::eval(R"(
[(d.project_name, d.version)
 for d in __import__("pkg_resources").working_set]
)",
                                   _globals);
            for (auto const & item : result) {
                auto tuple = item.cast<py::tuple>();
                packages.emplace_back(
                    tuple[0].cast<std::string>(),
                    tuple[1].cast<std::string>());
            }
            std::sort(packages.begin(), packages.end(),
                      [](auto const & a, auto const & b) { return a.first < b.first; });
        } catch (...) {
            // Both methods failed — return empty
        }
    } catch (...) {
        // Non-fatal
    }

    return packages;
}

PythonResult PythonEngine::installPackage(std::string const & package) {
    PythonResult result;
    if (!_initialized) {
        result.stderr_text = "Python interpreter is not initialized.";
        return result;
    }

    if (package.empty()) {
        result.stderr_text = "Package name is empty.";
        return result;
    }

    try {
        // Use subprocess to run pip install
        std::string const pip_code = R"(
import subprocess, sys
_pip_result = subprocess.run(
    [sys.executable, "-m", "pip", "install", ")" + package + R"("],
    capture_output=True, text=True, timeout=300
)
print(_pip_result.stdout, end='')
if _pip_result.stderr:
    import sys as _sys
    print(_pip_result.stderr, end='', file=_sys.stderr)
_pip_rc = _pip_result.returncode
del _pip_result
)";
        result = execute(pip_code);

        // Check return code
        try {
            auto rc = _globals["_pip_rc"].cast<int>();
            if (rc != 0) {
                result.success = false;
                if (result.stderr_text.empty()) {
                    result.stderr_text = "pip install failed with return code " + std::to_string(rc);
                }
            }
            // Clean up temp variable
            _globals.attr("pop")("_pip_rc", py::none());
        } catch (...) {
            // Non-fatal — assume success if we got this far
        }

    } catch (py::error_already_set & e) {
        result.stderr_text = _formatException(e);
        result.success = false;
    } catch (std::exception const & e) {
        result.stderr_text = std::string("pip install error: ") + e.what();
        result.success = false;
    }

    return result;
}

std::pair<int, int> PythonEngine::pythonVersionTuple() const {
    if (!_initialized) {
        return {0, 0};
    }
    try {
        py::module_ sys = py::module_::import("sys");
        py::object vi = sys.attr("version_info");
        int major = vi.attr("major").cast<int>();
        int minor = vi.attr("minor").cast<int>();
        return {major, minor};
    } catch (...) {
        return {0, 0};
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool PythonEngine::isInitialized() const noexcept {
    return _initialized;
}

py::dict const & PythonEngine::globals() const {
    return _globals;
}

py::dict & PythonEngine::globals() {
    return _globals;
}

std::vector<std::string> PythonEngine::getUserVariableNames() const {
    std::vector<std::string> names;
    if (!_initialized) {
        return names;
    }

    for (auto const & item : _globals) {
        auto key = item.first.cast<std::string>();

        // Skip dunder names
        if (key.size() >= 2 && key[0] == '_' && key[1] == '_') {
            continue;
        }
        // Skip internal redirector names
        if (key == "_wt_stdout" || key == "_wt_stderr") {
            continue;
        }
        // Skip module objects (the user rarely cares about `import os`)
        try {
            if (py::isinstance(item.second, py::module_::import("types").attr("ModuleType"))) {
                continue;
            }
        } catch (...) {
            // If we can't check, include it
        }
        names.push_back(key);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::string PythonEngine::pythonVersion() const {
    if (!_initialized) {
        return "N/A";
    }
    try {
        py::module_ sys = py::module_::import("sys");
        return sys.attr("version").cast<std::string>();
    } catch (...) {
        return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void PythonEngine::_initNamespace() {
    // Start from a clean dict with builtins available
    _globals = py::dict{};
    _globals["__builtins__"] = py::module_::import("builtins");
    _globals["__name__"] = "__main__";
}

void PythonEngine::_installRedirectors() {
    // Import the embedded helper module
    py::module_ internal = py::module_::import("_wt_internal");
    py::object RedirectorClass = internal.attr("OutputRedirector");

    py::object stdout_obj = RedirectorClass();
    py::object stderr_obj = RedirectorClass();

    // Stash in globals so they stay alive
    _globals["_wt_stdout"] = stdout_obj;
    _globals["_wt_stderr"] = stderr_obj;

    // Keep raw pointers for drain()
    _stdout_redirector = stdout_obj.cast<OutputRedirector *>();
    _stderr_redirector = stderr_obj.cast<OutputRedirector *>();

    // Replace sys.stdout / sys.stderr
    py::module_ sys = py::module_::import("sys");
    sys.attr("stdout") = stdout_obj;
    sys.attr("stderr") = stderr_obj;
}

std::pair<std::string, std::string> PythonEngine::_drainOutput() {
    std::string out;
    std::string err;
    if (_stdout_redirector) {
        out = _stdout_redirector->drain();
    }
    if (_stderr_redirector) {
        err = _stderr_redirector->drain();
    }
    return {out, err};
}

std::string PythonEngine::_formatException(py::error_already_set & e) {
    std::string msg;
    try {
        // Use traceback.format_exception for a full Python-style traceback
        py::module_ tb = py::module_::import("traceback");
        py::object formatted = tb.attr("format_exception")(
            e.type(), e.value(), e.trace());
        for (auto const & line : formatted) {
            msg += line.cast<std::string>();
        }
    } catch (...) {
        // Fallback: just use pybind11's what()
        msg = e.what();
    }
    // Restore the Python error state so pybind11 is happy
    e.restore();
    PyErr_Clear();
    return msg;
}

// ---------------------------------------------------------------------------
// Private helpers — Virtual Environment (Phase 6)
// ---------------------------------------------------------------------------

std::filesystem::path PythonEngine::_findSitePackages(
    std::filesystem::path const & venv_root) const {
    if (!_initialized) {
        return {};
    }

    // Get the expected python version for the path
    auto const [major, minor] = pythonVersionTuple();

    // Common layout: lib/pythonX.Y/site-packages (Unix)
    {
        auto const candidate = venv_root / "lib"
                               / ("python" + std::to_string(major) + "." + std::to_string(minor))
                               / "site-packages";
        if (std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    // Conda layout: lib/site-packages (sometimes)
    {
        auto const candidate = venv_root / "lib" / "site-packages";
        if (std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    // Windows layout: Lib/site-packages
    {
        auto const candidate = venv_root / "Lib" / "site-packages";
        if (std::filesystem::is_directory(candidate)) {
            return candidate;
        }
    }

    // Fallback: search for any site-packages under lib/
    auto const lib_dir = venv_root / "lib";
    if (std::filesystem::is_directory(lib_dir)) {
        try {
            for (auto const & entry : std::filesystem::directory_iterator(lib_dir)) {
                if (entry.is_directory()) {
                    auto const sp = entry.path() / "site-packages";
                    if (std::filesystem::is_directory(sp)) {
                        return sp;
                    }
                }
            }
        } catch (...) {
            // Permission denied
        }
    }

    return {};
}

std::pair<int, int> PythonEngine::_readVenvPythonVersion(
    std::filesystem::path const & venv_root) const {

    // Try reading pyvenv.cfg
    auto const cfg_path = venv_root / "pyvenv.cfg";
    if (std::filesystem::exists(cfg_path)) {
        std::ifstream cfg(cfg_path);
        std::string line;
        while (std::getline(cfg, line)) {
            // Look for "version = X.Y.Z" or "version_info = X.Y.Z..."
            if (line.find("version") != std::string::npos && line.find('=') != std::string::npos) {
                auto const eq_pos = line.find('=');
                auto val = line.substr(eq_pos + 1);
                // Trim whitespace
                val.erase(0, val.find_first_not_of(" \t"));
                val.erase(val.find_last_not_of(" \t\r\n") + 1);

                // Parse major.minor from "X.Y" or "X.Y.Z"
                int major = 0;
                int minor = 0;
                auto const dot1 = val.find('.');
                if (dot1 != std::string::npos) {
                    try {
                        major = std::stoi(val.substr(0, dot1));
                        auto const rest = val.substr(dot1 + 1);
                        auto const dot2 = rest.find('.');
                        minor = std::stoi(rest.substr(0, dot2));
                        if (major >= 2 && major <= 4) {  // sanity check
                            return {major, minor};
                        }
                    } catch (...) {
                        // Parse error — continue scanning
                    }
                }
            }
        }
    }

    // Fallback: try to find the python binary's version from directory name
    // e.g., lib/python3.12/site-packages → 3.12
    auto const lib_dir = venv_root / "lib";
    if (std::filesystem::is_directory(lib_dir)) {
        try {
            for (auto const & entry : std::filesystem::directory_iterator(lib_dir)) {
                auto const name = entry.path().filename().string();
                if (name.substr(0, 6) == "python" && name.size() > 6) {
                    auto const ver = name.substr(6);
                    auto const dot = ver.find('.');
                    if (dot != std::string::npos) {
                        try {
                            int major = std::stoi(ver.substr(0, dot));
                            int minor = std::stoi(ver.substr(dot + 1));
                            return {major, minor};
                        } catch (...) {}
                    }
                }
            }
        } catch (...) {}
    }

    return {0, 0};  // unknown
}

bool PythonEngine::_looksLikeVenv(std::filesystem::path const & path) {
    // Check for pyvenv.cfg (standard venv)
    if (std::filesystem::exists(path / "pyvenv.cfg")) {
        return true;
    }

    // Check for bin/python or Scripts/python.exe (venv or conda)
    if (std::filesystem::exists(path / "bin" / "python")) {
        return true;
    }
    if (std::filesystem::exists(path / "Scripts" / "python.exe")) {
        return true;
    }

    // Check for conda-meta directory (conda env)
    if (std::filesystem::is_directory(path / "conda-meta")) {
        return true;
    }

    return false;
}
