#ifndef PYTHON_ENGINE_HPP
#define PYTHON_ENGINE_HPP

/**
 * @file PythonEngine.hpp
 * @brief Embedded Python interpreter for WhiskerToolbox.
 *
 * PythonEngine owns a `pybind11::scoped_interpreter`, provides persistent
 * namespace execution (REPL behaviour), stdout/stderr capture, and script
 * file execution.
 *
 * All public methods must be called from the **main thread** — the embedded
 * interpreter is single-threaded and the GIL is not released between calls.
 *
 * @see PythonResult for the return type of execute()/executeFile().
 * @see OutputRedirector for the stdout/stderr capture mechanism.
 */

#include "PythonResult.hpp"

#include <pybind11/embed.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class OutputRedirector;

class PythonEngine {
public:
    PythonEngine();
    ~PythonEngine();

    // Non-copyable, non-movable (interpreter is process-global)
    PythonEngine(PythonEngine const &) = delete;
    PythonEngine & operator=(PythonEngine const &) = delete;
    PythonEngine(PythonEngine &&) = delete;
    PythonEngine & operator=(PythonEngine &&) = delete;

    // === Execution ===

    /**
     * @brief Execute a string of Python code in the persistent namespace.
     *
     * Variables defined in one call remain available in subsequent calls
     * (REPL-style).  stdout and stderr are captured in the returned
     * PythonResult.
     */
    [[nodiscard]] PythonResult execute(std::string const & code);

    /**
     * @brief Execute a .py file.
     *
     * The file is read and executed via `exec()` in the persistent namespace.
     * The working directory is temporarily changed to the script's parent
     * directory for the duration of execution.
     */
    [[nodiscard]] PythonResult executeFile(std::filesystem::path const & path);

    // === Namespace Management ===

    /**
     * @brief Clear all user-defined variables from the namespace.
     *
     * Re-imports the default prelude (builtins, etc.) so the interpreter
     * remains usable.
     */
    void resetNamespace();

    /**
     * @brief Inject a pybind11 object into the Python namespace.
     *
     * After injection the object is accessible by @p name in subsequent
     * execute() calls.
     */
    void inject(std::string const & name, pybind11::object obj);

    // === Queries ===

    /// True once the interpreter has been successfully initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// Read-only access to the persistent globals dict.
    [[nodiscard]] pybind11::dict const & globals() const;

    /// Read-write access to the persistent globals dict (for PythonBridge).
    [[nodiscard]] pybind11::dict & globals();

    /**
     * @brief Get a snapshot of user-defined variable names in the namespace.
     *
     * Excludes builtins / dunder names / modules — useful for UI namespace
     * inspectors.
     */
    [[nodiscard]] std::vector<std::string> getUserVariableNames() const;

    /// The Python version string (e.g. "3.12.1").
    [[nodiscard]] std::string pythonVersion() const;

private:
    /// Build the initial globals dict with builtins + __name__ = "__main__".
    void _initNamespace();

    /// Install OutputRedirector instances as sys.stdout / sys.stderr.
    void _installRedirectors();

    /// Drain the redirectors and return their content.
    [[nodiscard]] std::pair<std::string, std::string> _drainOutput();

    /// Format a Python exception (with traceback) into a string.
    [[nodiscard]] static std::string _formatException(pybind11::error_already_set & e);

    std::unique_ptr<pybind11::scoped_interpreter> _interpreter;
    pybind11::dict _globals;

    // Pointers into the Python-side OutputRedirector instances held in sys.stdout/stderr.
    OutputRedirector * _stdout_redirector{nullptr};
    OutputRedirector * _stderr_redirector{nullptr};

    bool _initialized{false};
};

#endif // PYTHON_ENGINE_HPP
