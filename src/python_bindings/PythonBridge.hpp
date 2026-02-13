#ifndef PYTHON_BRIDGE_HPP
#define PYTHON_BRIDGE_HPP

/**
 * @file PythonBridge.hpp
 * @brief Non-Qt bridge between the DataManager and the embedded Python interpreter.
 *
 * PythonBridge injects a live DataManager (and optionally individual data
 * objects / TimeFrames) into the Python namespace so that user scripts can
 * read and write data through the same observer-enabled API that C++ uses.
 *
 * All public methods must be called from the **main thread** — they
 * manipulate the interpreter namespace which is not thread-safe.
 *
 * @see PythonEngine for the interpreter itself.
 * @see bind_datamanager.cpp for the pybind11 DataManager bindings.
 */

#include "PythonResult.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class PythonEngine;

/**
 * @brief Connects a live DataManager instance to the embedded Python interpreter.
 *
 * Typical lifecycle:
 * @code
 *   auto dm = std::make_shared<DataManager>();
 *   PythonEngine engine;
 *   PythonBridge bridge(dm, engine);
 *
 *   bridge.exposeDataManager();           // dm available as `dm` in Python
 *   engine.execute("print(dm.getAllKeys())");
 *
 *   // After user script adds data via dm.setData():
 *   auto new_keys = bridge.importNewData();
 * @endcode
 */
class PythonBridge {
public:
    /**
     * @brief Construct a bridge between a DataManager and a PythonEngine.
     * @param dm  Shared pointer to the live DataManager.
     * @param engine  Reference to the (already-initialized) PythonEngine.
     */
    explicit PythonBridge(std::shared_ptr<DataManager> dm, PythonEngine & engine);

    ~PythonBridge() = default;

    // Non-copyable (holds reference to engine)
    PythonBridge(PythonBridge const &) = delete;
    PythonBridge & operator=(PythonBridge const &) = delete;
    PythonBridge(PythonBridge &&) = delete;
    PythonBridge & operator=(PythonBridge &&) = delete;

    // === Exposure Methods ===

    /**
     * @brief Inject the DataManager into Python namespace as `dm`.
     *
     * Also auto-imports the `whiskertoolbox_python` module as `wt` for
     * convenient access to type constructors (e.g. `wt.AnalogTimeSeries`).
     *
     * Safe to call multiple times — re-injects the same shared_ptr.
     */
    void exposeDataManager();

    /**
     * @brief Inject a single data object by key into the Python namespace.
     *
     * Retrieves the data from the DataManager via `getDataVariant()` and
     * injects it under @p python_name.  Returns false if the key does not
     * exist or the type is not bound.
     *
     * @param key          The DataManager key for the data.
     * @param python_name  The name to use in the Python namespace.
     * @return true if injection succeeded.
     */
    bool exposeData(std::string const & key, std::string const & python_name);

    /**
     * @brief Expose a TimeFrame by its TimeKey under @p python_name.
     *
     * @param time_key     The TimeKey string for the TimeFrame.
     * @param python_name  The name to use in the Python namespace.
     * @return true if the TimeFrame exists and was injected.
     */
    bool exposeTimeFrame(std::string const & time_key, std::string const & python_name);

    // === Data Import ===

    /**
     * @brief Scan Python namespace for new data objects and register them.
     *
     * After script execution, this method inspects user-defined variables
     * in the Python namespace.  Any variable whose value is a bound
     * WhiskerToolbox data type (AnalogTimeSeries, DigitalEventSeries, etc.)
     * that is **not** already registered in the DataManager is added under
     * the variable's Python name, using the default time key.
     *
     * Objects that are already in the DataManager (because the user called
     * `dm.setData()` directly) are skipped — this method only catches
     * "orphan" data objects.
     *
     * @param default_time_key  TimeKey to use for newly imported data
     *                          (defaults to "time").
     * @return Names of newly registered data keys.
     */
    std::vector<std::string> importNewData(std::string const & default_time_key = "time");

    // === Convenience Execution ===

    /**
     * @brief Execute Python code with the DataManager already exposed.
     *
     * Equivalent to calling `exposeDataManager()` then `engine.execute(code)`.
     * Guarantees that `dm` and `wt` are available in the namespace.
     *
     * @param code  Python code string to execute.
     * @return PythonResult with stdout/stderr capture.
     */
    [[nodiscard]] PythonResult execute(std::string const & code);

    /**
     * @brief Execute a .py file with the DataManager already exposed.
     *
     * Equivalent to calling `exposeDataManager()` then `engine.executeFile(path)`.
     *
     * @param path  Path to the .py file.
     * @return PythonResult with stdout/stderr capture.
     */
    [[nodiscard]] PythonResult executeFile(std::filesystem::path const & path);

    // === Queries ===

    /// Check whether the DataManager is currently exposed in the Python namespace.
    [[nodiscard]] bool isDataManagerExposed() const;

    /// Get the underlying DataManager.
    [[nodiscard]] std::shared_ptr<DataManager> const & dataManager() const;

    /// Get the underlying PythonEngine.
    [[nodiscard]] PythonEngine & engine();
    [[nodiscard]] PythonEngine const & engine() const;

private:
    std::shared_ptr<DataManager> _dm;
    PythonEngine & _engine;
    bool _dm_exposed{false};
};

#endif // PYTHON_BRIDGE_HPP
