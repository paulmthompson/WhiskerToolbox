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

        py::exec(code, _globals);
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
