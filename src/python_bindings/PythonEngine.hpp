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

    // === Environment Configuration (Phase 5) ===

    /**
     * @brief Set the Python working directory.
     *
     * Calls `os.chdir()` to change to the specified directory.
     * If the directory does not exist, this is a no-op.
     *
     * @param dir  Absolute path to the directory.
     */
    void setWorkingDirectory(std::filesystem::path const & dir);

    /**
     * @brief Get the current Python working directory.
     */
    [[nodiscard]] std::filesystem::path getWorkingDirectory() const;

    /**
     * @brief Set sys.argv for script execution.
     *
     * Parses an argument string into a list of strings (splitting on whitespace,
     * respecting quoted strings) and sets `sys.argv`.
     *
     * @param args  Space-separated argument string.
     */
    void setSysArgv(std::string const & args);

    /**
     * @brief Execute a prelude code string.
     *
     * Silently executes the given code (e.g. auto-imports). Errors are
     * suppressed — the prelude is best-effort. Returns the PythonResult
     * for diagnostic purposes.
     *
     * @param prelude  Python code to execute as startup prelude.
     */
    [[nodiscard]] PythonResult executePrelude(std::string const & prelude);

    // === Virtual Environment Support (Phase 6) ===

    /**
     * @brief Discover virtual environments in common locations.
     *
     * Scans:
     *  - @p extra_paths (user-configured directories)
     *  - ~/.virtualenvs/
     *  - ~/.conda/envs/
     *  - ~/.pyenv/versions/
     *
     * Only returns paths that look like valid Python venvs (contain a
     * pyvenv.cfg or bin/python).
     *
     * @param extra_paths  Additional directories to scan.
     * @return List of discovered venv root paths.
     */
    [[nodiscard]] std::vector<std::filesystem::path>
    discoverVenvs(std::vector<std::filesystem::path> const & extra_paths = {}) const;

    /**
     * @brief Validate that a virtual environment is compatible.
     *
     * Checks:
     *  1. The path exists and looks like a venv (pyvenv.cfg or bin/python)
     *  2. The venv's Python version matches the embedded interpreter's major.minor
     *
     * @param venv_path  Root directory of the venv.
     * @return Empty string on success, or a human-readable error message.
     */
    [[nodiscard]] std::string validateVenv(std::filesystem::path const & venv_path) const;

    /**
     * @brief Activate a virtual environment.
     *
     * Prepends the venv's site-packages to `sys.path`, sets `sys.prefix`,
     * `sys.exec_prefix`, and `os.environ["VIRTUAL_ENV"]`.
     *
     * Call validateVenv() first to check compatibility.
     *
     * @param venv_path  Root directory of the venv.
     * @return Empty string on success, or a human-readable error message.
     */
    [[nodiscard]] std::string activateVenv(std::filesystem::path const & venv_path);

    /**
     * @brief Deactivate the currently active virtual environment.
     *
     * Restores `sys.path`, `sys.prefix`, `sys.exec_prefix`, and removes
     * `VIRTUAL_ENV` from `os.environ`.
     */
    void deactivateVenv();

    /**
     * @brief Get the currently active venv path, or empty if none.
     */
    [[nodiscard]] std::filesystem::path activeVenvPath() const;

    /**
     * @brief Check whether a virtual environment is currently active.
     */
    [[nodiscard]] bool isVenvActive() const;

    /**
     * @brief List packages installed in the active venv.
     *
     * Uses `importlib.metadata.distributions()` to enumerate packages.
     * Returns (name, version) pairs.
     *
     * @return Package list, or empty if no venv is active or listing fails.
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> listInstalledPackages() const;

    /**
     * @brief Install a package in the active venv using pip.
     *
     * Runs `pip install <package>` as a subprocess in the venv.
     * Blocks until completion.
     *
     * @param package  Package spec (e.g. "numpy", "scipy>=1.10").
     * @return PythonResult with pip's stdout/stderr.
     */
    [[nodiscard]] PythonResult installPackage(std::string const & package);

    /**
     * @brief Get the Python version of the embedded interpreter (major, minor).
     *
     * Returns e.g. {3, 12}.
     */
    [[nodiscard]] std::pair<int, int> pythonVersionTuple() const;

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

    /// Find the site-packages path inside a venv root.
    [[nodiscard]] std::filesystem::path _findSitePackages(std::filesystem::path const & venv_root) const;

    /// Read the Python version from a venv's pyvenv.cfg.
    [[nodiscard]] std::pair<int, int> _readVenvPythonVersion(std::filesystem::path const & venv_root) const;

    /// Check if a path looks like a valid venv (has pyvenv.cfg or bin/python).
    [[nodiscard]] static bool _looksLikeVenv(std::filesystem::path const & path);

    std::unique_ptr<pybind11::scoped_interpreter> _interpreter;
    pybind11::dict _globals;

    // Pointers into the Python-side OutputRedirector instances held in sys.stdout/stderr.
    OutputRedirector * _stdout_redirector{nullptr};
    OutputRedirector * _stderr_redirector{nullptr};

    bool _initialized{false};

    // Virtual environment state (Phase 6)
    std::filesystem::path _active_venv_path;
    pybind11::list _original_sys_path;
    std::string _original_sys_prefix;
    std::string _original_sys_exec_prefix;
};

#endif // PYTHON_ENGINE_HPP
