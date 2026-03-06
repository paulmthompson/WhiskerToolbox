This is a Qt6/C++ project used for neural data analysis and visualization. It provides tools for loading, processing, and visualizing neural datasets, with a focus on usability and performance.

This is a C++23 project. Catch2 is used for testing. CMake is used as the build system. Qt6 is used for the GUI components.

Cross platform (Windows, Linux, MacOS) support is provided via CMake.

GCC, Clang, and MSVC compilers are supported.

## Building

**IMPORTANT: Always use CMake presets. Never invoke the compiler (clang++, g++, etc.) or build tool (ninja, make) directly.**

**Determine the correct CMake preset before building.**
Check the `out/build/` directory to see which environment is currently configured:
- If `out/build/Clang/Release/` exists, use `linux-clang-release`.
- If `out/build/Clang/Debug/` exists, use `linux-clang-debug`.
- If `out/build/GCC/Release/` exists, use `linux-gcc-release`.
- If `out/build/GCC/Debug/` exists, use `linux-gcc-debug`.

The project has already been configured. To rebuild after code changes, use the preset you determined above. For example, if Clang Release is configured:

```bash
cmake --build --preset linux-clang-release > build_log.txt 2>&1
```

If you need to reconfigure (e.g. after CMakeLists.txt changes):

```bash
cmake --preset linux-clang-release -DENABLE_ORTOOLS=OFF -DENABLE_TIME_TRACE=ON > config_log.txt 2>&1
cmake --build --preset linux-clang-release > build_log.txt 2>&1
```

**IMPORTANT: Handling Build Output:**
Compiling and configuring generates many warnings and mountains of text. **Do NOT pollute your context window with standard build output.**
Under normal circumstances, always redirect stdout and stderr to a file (as shown above). If the command returns a success code (0), assume it worked. If the command fails, use `tail` or `grep` to inspect the bottom of the log file to find the specific error, rather than reading the entire file.

**Never run ninja or make directly in the build directory.**

**Never modify or delete files in the build directory (out/build/).** Doing so can invalidate the CMake cache and trigger a full rebuild that takes a very long time.

## Running Tests

**IMPORTANT: Always use ctest with the preset. Never run test binaries directly.**

**IMPORTANT: Handling Test Output:**
Like building, testing can generate significant output. When running tests normally, redirect the output to check only for success or failure.

Run all tests:
```bash
ctest --preset linux-clang-release --output-on-failure > test_log.txt 2>&1
```

Run specific tests by name with `-R` (regex filter):
```bash
ctest --preset linux-clang-release --output-on-failure -R "test name pattern" > test_log.txt 2>&1
```

### Finding Test Names

**To find the correct test name, use `ctest -N` to list all tests and grep for keywords:**

```bash
ctest --preset linux-clang-release -N | grep -i "keyword"
```

The project uses two test discovery modes:

1. **Catch2 tests** (most tests): Each TEST_CASE gets its own ctest entry. The test name is 
   the TEST_CASE string, e.g. `"loadStepFromDescriptor loads valid step with parameters"`.

2. **Fuzz tests** (tests/fuzz/): Use GoogleTest discovery. Test names follow the pattern
   `TestSuite.TestName`, e.g. `"PipelineLoaderFuzz.FuzzBinaryTransformPipeline"`.

3. **Bundled tests**: When `ENABLE_DETAILED_TEST_DISCOVERY` is OFF, all Catch2 tests in a 
   binary are bundled as a single ctest entry like `"test_TransformsV2_all"`.

**Do NOT guess test names or iterate blindly. Always use `ctest -N | grep` first.**

### Test CMakeLists Structure

Tests are organized under `tests/` with subdirectories mirroring `src/`. Each test subdirectory 
has a `CMakeLists.txt` that defines the test executable and its sources. If you need to find 
which test binary contains a specific test file, look at the `CMakeLists.txt` in that directory.

The `tests/TransformsV2/CMakeLists.txt` uses `--whole-archive` linker flags because TransformsV2 
uses static registration (RAII objects in anonymous namespaces). Any new test binary linking 
TransformsV2 must also use `--whole-archive` or the transforms won't be registered at runtime.

## Development Tools (Auto-Fixing & Analysis)

Do not attempt to manually text-search and replace code to fix standard C++ linting or formatting issues. Instead, utilize the sophisticated tooling available in this environment:

- **clang-tidy**: If you detect a bug, anti-pattern, or modernization opportunity, run the auto-fixer rather than trying to fix it manually:
  `run-clang-tidy -fix` (or your project's specific wrapper)
- **clang-format**: After generating or heavily modifying C++ code, always format the file:
  `clang-format -i path/to/file.cpp`
- **IWYU (Include What You Use)**: Ensures files directly include all headers they use.
  `include-what-you-use -Xiwyu --verbose=3 $(jq -r '.[] | select(.file | contains("main.cpp")) | .arguments[]' out/build/Clang/Release/compile_commands.json | grep -v "^clang" | tr '\n' ' ')`
  (Or simply: `include-what-you-use src/file.cpp [compiler flags]`)
- **cppcheck**: Static analysis for bugs, undefined behavior, and stylistic issues.
  File: `cppcheck --enable=warning,performance --suppress=missingIncludeSystem src/file.cpp`
  Project: `cppcheck --enable=all --suppress=missingIncludeSystem --project=compile_commands.json`
- **Infer**: Deep static analysis for null pointer dereferences, memory/resource leaks, and race conditions.
  `infer run --compilation-database out/build/Clang/Release/compile_commands.json`
- **Hotspot**: GUI for Linux perf profiler to visualize CPU performance data.
  Record: `perf record -g ./out/build/Clang/Release/WhiskerToolbox`
  View: `hotspot perf.data`
- **Heaptrack**: Heap memory profiler to identify memory leaks and excessive allocations.
  Profile: `/home/runner/work/WhiskerToolbox/WhiskerToolbox/tools/heaptrack/build/bin/heaptrack ./out/build/Clang/Release/WhiskerToolbox`
  View: `heaptrack_gui heaptrack.APP.PID.gz` OR `heaptrack_print heaptrack.APP.PID.gz`
- **ClangBuildAnalyzer**: Analyze and visualize Clang build times.
  `tools/ClangBuildAnalyzer/build/ClangBuildAnalyzer --start out/build/Clang/Release`
  `cmake --build out/build/Clang/Release -- -ftime-trace`
  `tools/ClangBuildAnalyzer/build/ClangBuildAnalyzer --all out/build/Clang/Release trace.bin`
  `tools/ClangBuildAnalyzer/build/ClangBuildAnalyzer --analyze trace.bin`

### Tool Summary Target Matrix

| Tool | Purpose | Type | Output |
|------------------|------------------|------------------|------------------|
| **IWYU** | Include dependency analysis | Static analysis | Terminal |
| **cppcheck** | Bug & quality static analysis | Static analysis | Terminal |
| **Infer** | Advanced bug detection | Static analysis | Terminal + HTML |
| **Hotspot** | CPU profiling visualization | Runtime profiling | GUI |
| **Heaptrack** | Memory profiling | Runtime profiling | Terminal + GUI |
| **ClangBuildAnalyzer** | Build time analysis | Build tool | Terminal |

## Project Architecture (Libraries)

*Note: The developer documentation at `docs/developer` generally mirrors the `src` directory structure. Consult the corresponding `.qmd` files for exhaustive architecture details of any given module.*

The overall structure of the project is divided into several static and dynamic libraries:

- **`NEURALYZER_GEOMETRY` (`src/CoreGeometry`)**: A lightweight geometry library providing definitions and operations for points, lines, masks, bounding boxes, and polygons.
- **`CoreMath` (`src/CoreMath`)**: A lightweight library primarily for fitting polynomials. Uses the Armadillo library as a backend and includes functions for fitting to lines and points.
- **`CorePlotting` (`src/CorePlotting`)**: An API-agnostic layout library that maps core data objects (e.g., MaskData, DigitalEventSeries) to abstract canvases and sets up axes. It also provides infrastructure for hit detection (e.g., Quadtree via SpatialIndex) and CPU-based line intersections.
- **`PlottingOpenGL` (`src/PlottingOpenGL`)**: OpenGL-specific renderer implementing `CorePlotting` structures using Qt-provided OpenGL. Houses shaders and a shader management system.
- **`DeepLearning` (`src/DeepLearning`)**: Interface for running models (TorchScript, AOTInductor) using a libtorch backend, returning `TensorData` objects.
- **`MLCore` (`src/MLCore`)**: Provides supervised and unsupervised machine learning (MLPack backend) for core data types, primarily operating on `TensorData`.
- **`ObserverData` (`src/Observer`)**: A lightweight, non-thread-safe observer pattern implementation using `std::function` callbacks. Objects (including `DataManager`) inherit from `ObserverData` to notify of changes. Consumers must manage `CallbackID` lifetimes and manually unregister to prevent memory leaks or crashes.
- **`SpatialIndex` (`src/SpatialIndex`)**: Provides efficient spatial querying via Quadtree, RTree, and KdTree implementations. Primarily used by `CorePlotting` for fast hit detection and interaction with large sample sets.
- **`ImageProcessing` (`src/ImageProcessing`)**: A collection of OpenCV wrappers tailored for core data types.
- **`TimeFrame` (`src/TimeFrame`)**: The core temporal organization system. Provides `TimeFrameIndex` (a strong type for element access) and allows different data objects to exist in independent temporal coordinate systems.
- **`DataManager` (`src/DataManager`)**: Core non-Qt library for data handling and loading. Stores all data types (Media, Point, Line, Mask, Digital Event/Interval, Analog, Tensor) in a `std::map<std::string, Variant>`. Includes a powerful Observer pattern for callbacks and a generic TableView interface for tabular conversions. Data timebases are strongly typed with `TimeFrameIndex`.
- **`GatherResult` (`src/GatherResult`)**: Interface library used for collecting and consolidating multiple sub-views of data objects.
- **`PythonBridge` (`src/python_bindings`)**: Provides pybind11 wrapping for core data types, `DataManager`, and `TimeFrame` objects, as well as an embedded Python interpreter.
- **`TransformsV2`**: Data conversion architecture (e.g., converts Mask Data to Analog Time Series). Uses static registration.
- **`ParameterSchema` (`src/ParameterSchema`)**: Shared library for compile-time parameter schema extraction from reflect-cpp structs. Provides `ParameterFieldDescriptor`, `ParameterSchema`, `extractParameterSchema<T>()`, and `ParameterUIHints<T>`. Used by both TransformsV2 and the Command Architecture. No Qt dependency.
- **`Entity` (`src/Entity`)**: Centralized entity registry system assigning unique `EntityID`s to data objects and managing grouping.
- **`TriageSession` (`src/TriageSession`)**: Mark/Commit/Recall state machine for triage workflows. Consumes the Command Architecture to execute user-configured pipelines over frame ranges. No Qt dependency.
- **`AutoParamWidget` (`src/WhiskerToolbox/AutoParamWidget`)**: Generic Qt form widget that takes a `ParameterSchema` and dynamically generates appropriate input widgets for each field. Depends on `Qt6::Widgets` and `ParameterSchema` only — no TransformsV2 dependency.
- **`ColormapControls` (`src/WhiskerToolbox/Plots/Common/ColormapControls`)**: Composable Qt widget for selecting a colormap preset (with gradient icon previews) and configuring color range (Auto/Manual/Symmetric with vmin/vmax). Depends on `Qt6::Widgets` and `CorePlotting`. Used by HeatmapWidget and planned for SpectrogramWidget.
- **`LayoutTesting` (`src/WhiskerToolbox/LayoutTesting`)**: Debug-only library for detecting layout constraint violations (zero-dimension widgets, below-minimumSizeHint, extreme aspect ratios). Provides a global event-filter monitor and a static `getViolations()` API for Catch2 tests. Depends on `Qt6::Widgets` only.
- **`WhiskerToolbox` (`src/WhiskerToolbox`)**: The main Qt6 GUI application. Driven by a shared `DataManager` instance and built on the `Qt6AdvancedDocking` system. Follows the EditorState architecture pattern with state management, registration, SelectionContext integration, and Zone-based placement.
- **`EditorState` (`src/WhiskerToolbox/EditorState`)**: Core infrastructure for widget state management, inter-widget communication, and workspace serialization. Every widget houses its serializable state in a subclass of `EditorState`. Supports JSON serialization (reflect-cpp), dirty tracking, and Qt signal-based change notification.

### Key UI Components (`WhiskerToolbox`)
- **MainWindow**: The main application window.
- **DataManager_Widget**: Main data view and object inspector. Sub-widgets for each data type.
- **GroupManagementWidget**: Manages entity groups.
- **TableViewerWidget/TableDesignerWidget**: Viewing and creating table views.
- **Media_Widget**: Viewing media data and manipulating selections.
- **DataViewer_Widget/DataViewer**: Generic viewer for time series (analog/digital).
- **DataTransform_Widget**: UI for executing data transforms.
- **DataImport_Widget**: Unified data import supporting all standard data types. Opens via Modules → Data Import.
- **DataExport_Widget**: Data export/saving functionality.

## Specific Task Workflows

When asked to implement a specific task, follow these standardized workflows to maintain project hygiene.

### Adding a New Feature
When adding a new feature or creating new files, you must complete the following steps:

1. **Doxygen Headers:** Every new C++ file (`.hpp`/`.cpp`) must have an `@file` Doxygen tag at the top with a brief description of the file's contents.
2. **Auto-Formatting:** After modifying or adding files, ALWAYS run `clang-format -i` exclusively on the files you touched (do not format the entire repository).
3. **Linting (clang-tidy):** **Do not skip this step.** Run clang-tidy on every C++ file (`.hpp`/`.cpp`) file you touched and apply auto-fixes. Use the exact command:
   ```bash
   run-clang-tidy -fix -p out/build/Clang/Release path/to/file1.cpp path/to/file2.cpp > tidy_log.txt 2>&1
   grep -c "error:\|warning:" tidy_log.txt || echo "Clean"
   ```
   Report the warning/error count from the grep output. If auto-fixes were applied, re-run the build to confirm nothing broke. Redirect output to `tidy_log.txt` to avoid context bloat; use `grep -E "error:|warning:" tidy_log.txt` to inspect failures.
4. **Developer Documentation:** Always create or modify the corresponding developer documentation entry.
   - For a source file `src/Path/To/File.cpp`, create/edit `docs/developer/Path/To/File.qmd`.
   - You MUST add any newly created `.qmd` files to the navigation structure in `docs/_quarto.yml`.
5. **User Documentation:** If the feature is user-facing, you must add at least a stub article to the User Guide.
   - Create `docs/user_guide/Path/To/Feature.qmd` and link it in `docs/_quarto.yml`.

### Adding a New Static Library
If a new static library is created via CMake, you must:
1. Update `CMakeLists.txt` appropriately.
2. Add a one or two-sentence description of the new library to the **Project Architecture (Libraries)** section in this document (`.github/copilot-instructions.md`).

## Git Hygiene

**IMPORTANT: Never use catch-all commit commands like `git add .` or `git add *`.**

When operating as an AI assistant (especially in cloud environments), you generate temporary files, massive build logs (`build_log.txt`, `test_log.txt`), or debugging artifacts. 

- **Always stage files explicitly by path:** Use `git add path/to/specific/file.cpp`
- **Review before committing:** If asked to commit changes, double-check exactly which files you are staging using `git status` or by looking at your own file modifications.
- **Do not commit build artifacts:** Never stage `build_log.txt`, `test_log.txt`, `config_log.txt`, or anything inside the `out/build/` directory.

