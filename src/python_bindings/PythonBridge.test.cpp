/**
 * @file PythonBridge.test.cpp
 * @brief Integration tests for the PythonBridge (Phase 3).
 *
 * Tests verify that:
 * - The DataManager is correctly injected into the Python namespace
 * - dm.getData() returns typed wrappers for data created in C++
 * - dm.setData() from Python registers data visible in C++
 * - Observer notifications fire after Python mutations
 * - importNewData() discovers orphan data objects in the namespace
 * - exposeData() / exposeTimeFrame() inject individual objects
 * - Error recovery: the bridge remains usable after exceptions
 *
 * NOTE: Python can only be initialized/finalized once per process, so we
 * keep a single static PythonEngine and call resetNamespace() between tests.
 */

#include "PythonBridge.hpp"
#include "PythonEngine.hpp"
#include "PythonResult.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <string>

// ── Shared engine (one per process) ────────────────────────────────────────

static PythonEngine & engine() {
    static PythonEngine eng;
    return eng;
}

/// Create a fresh DataManager + PythonBridge for each test.
struct BridgeFixture {
    std::shared_ptr<DataManager> dm;
    PythonBridge bridge;

    BridgeFixture()
        : dm(std::make_shared<DataManager>()),
          bridge(dm, engine()) {
        engine().resetNamespace();
    }
};

/// Helper — execute via bridge and REQUIRE success
static PythonResult run(PythonBridge & bridge, std::string const & code) {
    auto r = bridge.execute(code);
    INFO("stdout: " << r.stdout_text);
    INFO("stderr: " << r.stderr_text);
    REQUIRE(r.success);
    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: DataManager Exposure
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: exposeDataManager injects dm and wt", "[bridge]") {
    BridgeFixture f;
    f.bridge.exposeDataManager();

    REQUIRE(f.bridge.isDataManagerExposed());

    // `dm` should be accessible
    auto r = run(f.bridge, "print(type(dm).__name__)");
    REQUIRE(r.stdout_text == "DataManager\n");

    // `wt` module should be available
    r = run(f.bridge, "print(hasattr(wt, 'AnalogTimeSeries'))");
    REQUIRE(r.stdout_text == "True\n");
}

TEST_CASE("Bridge: execute auto-exposes dm", "[bridge]") {
    BridgeFixture f;

    // Don't call exposeDataManager() explicitly — execute() should do it
    auto r = run(f.bridge, "print(type(dm).__name__)");
    REQUIRE(r.success);
    REQUIRE(r.stdout_text == "DataManager\n");
    REQUIRE(f.bridge.isDataManagerExposed());
}

TEST_CASE("Bridge: dm is the same object (shared_ptr identity)", "[bridge]") {
    BridgeFixture f;

    // Register data in C++
    auto ts = std::make_shared<AnalogTimeSeries>();
    f.dm->setData("from_cpp", ts, TimeKey("time"));

    // Access from Python — should see the same data
    auto r = run(f.bridge, "print('from_cpp' in dm.getAllKeys())");
    REQUIRE(r.stdout_text == "True\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: getData from Python
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: getData returns AnalogTimeSeries created in C++", "[bridge]") {
    BridgeFixture f;

    // Create data in C++
    auto ts = std::make_shared<AnalogTimeSeries>(
        std::vector<float>{10.0f, 20.0f, 30.0f},
        std::vector<TimeFrameIndex>{TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)});
    f.dm->setData("angle", ts, TimeKey("time"));

    auto r = run(f.bridge, R"(
data = dm.getData('angle')
print(type(data).__name__)
print(data.getNumSamples())
)");
    REQUIRE(r.stdout_text.find("AnalogTimeSeries") != std::string::npos);
    REQUIRE(r.stdout_text.find("3") != std::string::npos);
}

TEST_CASE("Bridge: getData returns DigitalEventSeries", "[bridge]") {
    BridgeFixture f;

    auto des = std::make_shared<DigitalEventSeries>();
    des->addEvent(TimeFrameIndex(5));
    des->addEvent(TimeFrameIndex(15));
    f.dm->setData("licks", des, TimeKey("time"));

    auto r = run(f.bridge, R"(
ev = dm.getData('licks')
print(type(ev).__name__)
print(ev.size())
)");
    REQUIRE(r.stdout_text.find("DigitalEventSeries") != std::string::npos);
    REQUIRE(r.stdout_text.find("2") != std::string::npos);
}

TEST_CASE("Bridge: getData returns None for unknown key", "[bridge]") {
    BridgeFixture f;
    auto r = run(f.bridge, "print(dm.getData('nonexistent'))");
    REQUIRE(r.stdout_text == "None\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: setData from Python
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: setData from Python registers in C++ DataManager", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
ts = wt.AnalogTimeSeries(
    [1.0, 2.0, 3.0, 4.0, 5.0],
    [wt.TimeFrameIndex(i) for i in range(5)]
)
dm.setData('from_python', ts, 'time')
)");

    // Verify in C++
    auto keys = f.dm->getAllKeys();
    REQUIRE(std::find(keys.begin(), keys.end(), "from_python") != keys.end());
    REQUIRE(f.dm->getType("from_python") == DM_DataType::Analog);

    auto retrieved = f.dm->getData<AnalogTimeSeries>("from_python");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->getNumSamples() == 5);
}

TEST_CASE("Bridge: setData DigitalEventSeries from Python", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
des = wt.DigitalEventSeries([wt.TimeFrameIndex(i) for i in [10, 20, 30]])
dm.setData('events', des, 'time')
)");

    auto retrieved = f.dm->getData<DigitalEventSeries>("events");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->size() == 3);
}

TEST_CASE("Bridge: setData DigitalIntervalSeries from Python", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
dis = wt.DigitalIntervalSeries()
dis.addInterval(0, 100)
dis.addInterval(200, 300)
dm.setData('intervals', dis, 'time')
)");

    auto retrieved = f.dm->getData<DigitalIntervalSeries>("intervals");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->size() == 2);
}

TEST_CASE("Bridge: setData LineData from Python", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
ld = wt.LineData()
ld.addAtTime(wt.TimeFrameIndex(0), wt.Line2D([wt.Point2D(0,0), wt.Point2D(1,1)]))
dm.setData('whisker', ld, 'time')
)");

    auto retrieved = f.dm->getData<LineData>("whisker");
    REQUIRE(retrieved != nullptr);
}

TEST_CASE("Bridge: setData PointData from Python", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
pd = wt.PointData()
pd.addAtTime(wt.TimeFrameIndex(0), wt.Point2D(5.0, 10.0))
dm.setData('landmarks', pd, 'time')
)");

    auto retrieved = f.dm->getData<PointData>("landmarks");
    REQUIRE(retrieved != nullptr);
}

TEST_CASE("Bridge: setData MaskData from Python", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, R"(
md = wt.MaskData()
md.addAtTime(wt.TimeFrameIndex(0), wt.Mask2D([wt.Point2DU32(1,2)]))
dm.setData('roi', md, 'time')
)");

    auto retrieved = f.dm->getData<MaskData>("roi");
    REQUIRE(retrieved != nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: Observer Notifications
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: observer fires when Python calls setData", "[bridge][observer]") {
    BridgeFixture f;

    std::atomic<int> notify_count{0};
    f.dm->addObserver([&notify_count]() {
        notify_count++;
    });

    int before = notify_count.load();

    run(f.bridge, R"(
ts = wt.AnalogTimeSeries([1.0, 2.0], [wt.TimeFrameIndex(0), wt.TimeFrameIndex(1)])
dm.setData('observed', ts, 'time')
)");

    // Observer should have fired at least once
    REQUIRE(notify_count.load() > before);
}

TEST_CASE("Bridge: observer fires when Python calls deleteData", "[bridge][observer]") {
    BridgeFixture f;

    // Pre-populate
    auto ts = std::make_shared<AnalogTimeSeries>();
    f.dm->setData("to_delete", ts, TimeKey("time"));

    std::atomic<int> notify_count{0};
    f.dm->addObserver([&notify_count]() {
        notify_count++;
    });

    int before = notify_count.load();

    run(f.bridge, "dm.deleteData('to_delete')");

    REQUIRE(notify_count.load() > before);

    // Key should be gone
    auto keys = f.dm->getAllKeys();
    REQUIRE(std::find(keys.begin(), keys.end(), "to_delete") == keys.end());
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: importNewData
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: importNewData discovers orphan AnalogTimeSeries", "[bridge][import]") {
    BridgeFixture f;

    // Record existing keys (DataManager may have defaults like 'media')
    auto before_keys = f.dm->getAllKeys();

    run(f.bridge, R"(
orphan_ts = wt.AnalogTimeSeries(
    [1.0, 2.0, 3.0],
    [wt.TimeFrameIndex(0), wt.TimeFrameIndex(1), wt.TimeFrameIndex(2)]
)
)");

    // orphan_ts is NOT in the DataManager yet
    REQUIRE(f.dm->getData<AnalogTimeSeries>("orphan_ts") == nullptr);

    auto imported = f.bridge.importNewData();
    REQUIRE(imported.size() == 1);
    REQUIRE(imported[0] == "orphan_ts");

    // Now it should be in the DataManager
    auto retrieved = f.dm->getData<AnalogTimeSeries>("orphan_ts");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->getNumSamples() == 3);
}

TEST_CASE("Bridge: importNewData skips already-registered data", "[bridge][import]") {
    BridgeFixture f;

    // Register via dm.setData() from Python
    run(f.bridge, R"(
registered = wt.AnalogTimeSeries([1.0], [wt.TimeFrameIndex(0)])
dm.setData('registered', registered, 'time')
)");

    auto imported = f.bridge.importNewData();
    // 'registered' is already in the DataManager, should not be double-imported
    REQUIRE(std::find(imported.begin(), imported.end(), "registered") == imported.end());
}

TEST_CASE("Bridge: importNewData discovers multiple types", "[bridge][import]") {
    BridgeFixture f;

    run(f.bridge, R"(
new_events = wt.DigitalEventSeries([wt.TimeFrameIndex(5)])
new_line = wt.LineData()
x = 42  # plain int — should be skipped
)");

    auto imported = f.bridge.importNewData();
    REQUIRE(imported.size() == 2);

    // Sorted alphabetically
    REQUIRE(std::find(imported.begin(), imported.end(), "new_events") != imported.end());
    REQUIRE(std::find(imported.begin(), imported.end(), "new_line") != imported.end());

    // Plain int should NOT be imported
    REQUIRE(std::find(imported.begin(), imported.end(), "x") == imported.end());
}

TEST_CASE("Bridge: importNewData with custom time key", "[bridge][import]") {
    BridgeFixture f;

    // Pre-register the time key so DataManager can associate it
    auto tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2});
    f.dm->setTime(TimeKey("my_clock"), tf);

    run(f.bridge, R"(
custom_ts = wt.AnalogTimeSeries([1.0], [wt.TimeFrameIndex(0)])
)");

    auto imported = f.bridge.importNewData("my_clock");
    REQUIRE(imported.size() == 1);

    auto tk = f.dm->getTimeKey("custom_ts");
    REQUIRE(tk.str() == "my_clock");
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: exposeData / exposeTimeFrame
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: exposeData injects a single data object", "[bridge][expose]") {
    BridgeFixture f;

    auto ts = std::make_shared<AnalogTimeSeries>(
        std::vector<float>{100.0f},
        std::vector<TimeFrameIndex>{TimeFrameIndex(0)});
    f.dm->setData("single", ts, TimeKey("time"));

    REQUIRE(f.bridge.exposeData("single", "my_ts"));

    auto r = run(f.bridge, "print(my_ts.getNumSamples())");
    REQUIRE(r.stdout_text == "1\n");
}

TEST_CASE("Bridge: exposeData returns false for missing key", "[bridge][expose]") {
    BridgeFixture f;
    REQUIRE_FALSE(f.bridge.exposeData("nonexistent", "nope"));
}

TEST_CASE("Bridge: exposeTimeFrame injects a TimeFrame", "[bridge][expose]") {
    BridgeFixture f;

    std::vector<int> time_vals;
    for (int i = 0; i <= 100; ++i) time_vals.push_back(i);
    auto tf = std::make_shared<TimeFrame>(time_vals);
    f.dm->setTime(TimeKey("my_clock"), tf);

    REQUIRE(f.bridge.exposeTimeFrame("my_clock", "clock"));

    auto r = run(f.bridge, "print(clock.getTotalFrameCount())");
    REQUIRE(r.stdout_text == "101\n");
}

TEST_CASE("Bridge: exposeTimeFrame returns false for missing key", "[bridge][expose]") {
    BridgeFixture f;
    REQUIRE_FALSE(f.bridge.exposeTimeFrame("missing_clock", "nope"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: executeFile
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: executeFile with dm available", "[bridge]") {
    BridgeFixture f;

    // Create a temp script that uses dm
    auto tmp = std::filesystem::temp_directory_path() / "wt_bridge_test.py";
    {
        std::ofstream out(tmp);
        out << "import whiskertoolbox_python as wt\n"
            << "ts = wt.AnalogTimeSeries([1.0, 2.0], "
            << "[wt.TimeFrameIndex(0), wt.TimeFrameIndex(1)])\n"
            << "dm.setData('from_file', ts, 'time')\n"
            << "print('script done')\n";
    }

    auto r = f.bridge.executeFile(tmp);
    REQUIRE(r.success);
    REQUIRE(r.stdout_text.find("script done") != std::string::npos);

    // Verify data was registered
    auto retrieved = f.dm->getData<AnalogTimeSeries>("from_file");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->getNumSamples() == 2);

    std::filesystem::remove(tmp);
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: Error Recovery
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: error in script doesn't break bridge", "[bridge][error]") {
    BridgeFixture f;

    // Execute faulty code
    auto r = f.bridge.execute("raise ValueError('test error')");
    REQUIRE_FALSE(r.success);
    REQUIRE(r.stderr_text.find("ValueError") != std::string::npos);

    // Bridge should still work
    r = run(f.bridge, "print('still alive')");
    REQUIRE(r.stdout_text == "still alive\n");

    // dm should still be accessible
    r = run(f.bridge, "print(type(dm).__name__)");
    REQUIRE(r.stdout_text == "DataManager\n");
}

TEST_CASE("Bridge: resetNamespace then re-expose", "[bridge]") {
    BridgeFixture f;

    run(f.bridge, "x = 42");

    // Reset via engine
    engine().resetNamespace();

    // dm is gone now
    auto r = f.bridge.execute("print(type(dm).__name__)");
    // exposeDataManager() is called by execute(), so dm should be re-injected
    REQUIRE(r.success);
    REQUIRE(r.stdout_text == "DataManager\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Section: End-to-End Workflow
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Bridge: full workflow — C++ create, Python read, Python create, C++ verify", "[bridge][e2e]") {
    BridgeFixture f;

    // 1. Create data in C++
    auto ts = std::make_shared<AnalogTimeSeries>(
        std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f},
        std::vector<TimeFrameIndex>{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2),
            TimeFrameIndex(3), TimeFrameIndex(4)});
    f.dm->setData("raw_signal", ts, TimeKey("time"));

    // 2. Python reads it, creates a filtered version
    run(f.bridge, R"(
# Get the original data
raw = dm.getData('raw_signal')
values = raw.toList()

# Simple "filter": multiply by 2
filtered_vals = [v * 2 for v in values]
filtered_times = [wt.TimeFrameIndex(i) for i in range(len(filtered_vals))]

# Create new data and register it
filtered = wt.AnalogTimeSeries(filtered_vals, filtered_times)
dm.setData('filtered_signal', filtered, 'time')
)");

    // 3. Verify in C++
    auto filtered = f.dm->getData<AnalogTimeSeries>("filtered_signal");
    REQUIRE(filtered != nullptr);
    REQUIRE(filtered->getNumSamples() == 5);

    // Values should be doubled
    auto val0 = filtered->getAtTime(TimeFrameIndex(0));
    REQUIRE(val0.has_value());
    REQUIRE(val0.value() == 2.0f);

    auto val4 = filtered->getAtTime(TimeFrameIndex(4));
    REQUIRE(val4.has_value());
    REQUIRE(val4.value() == 10.0f);
}

TEST_CASE("Bridge: full workflow — Python creates events, C++ reads them", "[bridge][e2e]") {
    BridgeFixture f;

    run(f.bridge, R"(
# Create interval series from Python analysis
intervals = wt.DigitalIntervalSeries()
intervals.addInterval(10, 50)
intervals.addInterval(100, 200)
intervals.addInterval(300, 400)
dm.setData('detected_whisks', intervals, 'time')

# Also create event series
events = wt.DigitalEventSeries([wt.TimeFrameIndex(i) for i in [10, 100, 300]])
dm.setData('whisk_onsets', events, 'time')
)");

    // Verify intervals
    auto intervals = f.dm->getData<DigitalIntervalSeries>("detected_whisks");
    REQUIRE(intervals != nullptr);
    REQUIRE(intervals->size() == 3);

    // Verify events
    auto events = f.dm->getData<DigitalEventSeries>("whisk_onsets");
    REQUIRE(events != nullptr);
    REQUIRE(events->size() == 3);
}
