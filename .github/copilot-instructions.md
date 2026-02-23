This is a Qt6/C++ project used for neural data analysis and visualization. It provides tools for loading, processing, and visualizing neural datasets, with a focus on usability and performance.

This is a C++23 project. Catch2 is used for testing. CMake is used as the build system. Qt6 is used for the GUI components.

Cross platform (Windows, Linux, MacOS) support is provided via CMake.

GCC, Clang, and MSVC compilers are supported.

## Building

**IMPORTANT: Always use CMake presets. Never invoke the compiler (clang++, g++, etc.) or build tool (ninja, make) directly.**

The project has already been configured. To rebuild after code changes:

```bash
cmake --build --preset linux-clang-release
```

If you need to reconfigure (e.g. after CMakeLists.txt changes):

```bash
cmake --preset linux-clang-release -DENABLE_ORTOOLS=OFF -DENABLE_TIME_TRACE=ON
cmake --build --preset linux-clang-release
```

The build output is in `out/build/Clang/Release`. **Never run ninja or make directly in the build directory.**

**Never modify or delete files in the build directory (out/build/).** Doing so can invalidate the CMake cache and trigger a full rebuild that takes a very long time.

## Running Tests

**IMPORTANT: Always use ctest with the preset. Never run test binaries directly.**

Run all tests:
```bash
ctest --preset linux-clang-release --output-on-failure
```

Run specific tests by name with `-R` (regex filter):
```bash
ctest --preset linux-clang-release --output-on-failure -R "test name pattern"
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

## Development Tools

You have access to several tools in your environment for performance analysis including heaptrack,
infer, cppcheck, iwyu, clang-tidy, and clang-format. The full description is located in 
docs/developer/copilot.qmd

The overall structure of the project is as follows:

The Data Manager library (src/DataManager) is a non-Qt library that provides the core data handling, 
and loading. The DataManager class holds all data types in a std::map<std::string, Variant> where the variant type is one of several supported data types. Current categories are
Media Data - (images, video)
Point Data - (time series of points)
Line Data - (time series of lines/curves)
Mask Data - (binary masks over time)
Digital Event Series - (single time events over time)
Digital Interval Series - (time intervals over time)
Analog Time Series - (continuous analog signals over time)
Tensor Data - (multi-dimensional arrays)

All data types store their time values as strongly typed TimeFrameIndex objects. A TimeFrameIndex is the index in a TimeFrame object. TimeFrame objects are vectors of time values that should be in "absolute" time.
This allows different data structures to have different time bases and sampling rates. Most interfaces 
enforce you to specify the time base you are requesting in.

Data Manager library also includes the loaders for these data types. It also holds a Transform architecture that takes one data type and converts it into another. For example Mask Area takes mask data and outputs area as an analog time series. DataManager also includes a TableView interface used for converting data objects into tabular formats. This is done by specifying row selectors (intervals, time events) as well as 
column computers that then work on data objects to aggregate and reduce data for each cellb based on the row and column objects. The DataManager stores all tables that are created.

The Data Manager itself as well as all of its objects are plugged into an Observer pattern so that changes to data objects propagate to all dependent objects. This is accomplished by holding std::function callbacks.

Almost all data objects are also assigned an EntityID which is unique to each object. These are held in a central EntityRegistry (src/Entity). The entity system can also store entities in groups. 

The UI for this project is located in src/WhiskerToolbox (legacy name). Most widgets hold a shared_ptr to the data manager. Widgets are arranged with the Qt6AdvancedDocking system. Several important widgets of note:
- MainWindow - the main application window
- DataManager_Widget - the main data manager view and object inspector. Each data type has its own sub widget. New data can be created from here
- GroupManagementWidget - widget for managing entity groups
- TableViewerWidget/TableDesignerWidget - widget for viewing and creating table views
- Media_Widget - widget for viewing media data (images, video, lines, masks, points). Also can be used for selecting and manipulating data
- DataViewer_Widget/DataViewer - generic data viewer for time series data (analog, digital)
- Analysis_Dashboard - Collection of widgets for plotting. These include spatial overlay widget (collapsing all point data, mask data, and line data onto a single view), scatter plot widget, event plot widget etc. The user can open as many plots as they want.
- DataTransform_Widget - UI for executing data transforms
- DataImport_Widget - unified data import widget supporting all data types (Lines, Masks, Points, AnalogTimeSeries, DigitalEventSeries, DigitalIntervalSeries, TensorData). Opens via Modules → Data Import menu. Uses EditorState pattern for state management and responds to SelectionContext for passive data type awareness.
- DataExport_Widget (formerly IO_Widgets) - data export/saving functionality for supported data types

The UI follows the EditorState architecture pattern where each widget type has:
- A State class (EditorState subclass) for serializable state management
- A Registration module that registers the widget type with EditorRegistry
- Integration with SelectionContext for inter-widget communication
- Zone-based placement using ZoneManager (Left, Center, Right, Bottom zones)

