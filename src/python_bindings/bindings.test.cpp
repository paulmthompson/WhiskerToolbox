/**
 * @file bindings.test.cpp
 * @brief Round-trip tests for the whiskertoolbox_python embedded module.
 *
 * Tests verify that all bound C++ types can be created, queried and
 * manipulated from the embedded Python interpreter.
 *
 * NOTE: Python can only be initialized/finalized once per process, so we
 * keep a single static PythonEngine and call resetNamespace() between tests.
 */

#include "PythonEngine.hpp"
#include "PythonResult.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

// ── shared engine ──────────────────────────────────────────────────────────

static PythonEngine & engine() {
    static PythonEngine eng;
    return eng;
}

struct Fixture {
    Fixture() { engine().resetNamespace(); }
};

// Helper — execute and REQUIRE success
static PythonResult run(std::string const & code) {
    auto r = engine().execute(code);
    INFO("stdout: " << r.stdout_text);
    INFO("stderr: " << r.stderr_text);
    REQUIRE(r.success);
    return r;
}

// ── Module import ──────────────────────────────────────────────────────────

TEST_CASE("Module imports successfully", "[bindings]") {
    Fixture f;
    auto r = run("import whiskertoolbox_python as wt");
    (void)r;
}

// ── Geometry ───────────────────────────────────────────────────────────────

TEST_CASE("Point2D creation and access", "[bindings][geometry]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("p = wt.Point2D(1.5, 2.5)");
    auto r = run("print(p.x, p.y)");
    REQUIRE(r.stdout_text == "1.5 2.5\n");
}

TEST_CASE("Point2DU32 creation", "[bindings][geometry]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("p = wt.Point2DU32(10, 20)");
    auto r = run("print(p.x, p.y)");
    REQUIRE(r.stdout_text == "10 20\n");
}

TEST_CASE("ImageSize creation", "[bindings][geometry]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("s = wt.ImageSize(640, 480)");
    auto r = run("print(s.width, s.height)");
    REQUIRE(r.stdout_text == "640 480\n");
}

// ── TimeFrame ──────────────────────────────────────────────────────────────

TEST_CASE("TimeFrameIndex arithmetic and hash", "[bindings][timeframe]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("a = wt.TimeFrameIndex(10)");
    run("b = wt.TimeFrameIndex(3)");
    auto r = run("print((a + b).getValue(), (a - b).getValue())");
    REQUIRE(r.stdout_text == "13 7\n");

    // __hash__ and __int__
    run("print(int(a))");
    auto r2 = engine().execute("print(hash(a) == hash(wt.TimeFrameIndex(10)))");
    REQUIRE(r2.success);
    REQUIRE(r2.stdout_text == "True\n");
}

TEST_CASE("TimeFrame basics", "[bindings][timeframe]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("tf = wt.TimeFrame([0, 100, 200, 300])");
    auto r = run("print(tf.getTotalFrameCount())");
    REQUIRE(r.stdout_text == "4\n");
    r = run("print(tf.getTimeAtIndex(wt.TimeFrameIndex(1)))");
    REQUIRE(r.stdout_text == "100\n");
}

TEST_CASE("Interval creation", "[bindings][timeframe]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("iv = wt.Interval(10, 50)");
    auto r = run("print(iv.start, iv.end)");
    REQUIRE(r.stdout_text == "10 50\n");
}

// ── EntityId ───────────────────────────────────────────────────────────────

TEST_CASE("EntityId creation and hash", "[bindings][entity]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("e = wt.EntityId(42)");
    auto r = run("print(e.id)");
    REQUIRE(r.stdout_text == "42\n");
    r = run("print(e == wt.EntityId(42))");
    REQUIRE(r.stdout_text == "True\n");
    r = run("print(hash(e))");
    REQUIRE(r.success);
}

// ── AnalogTimeSeries ───────────────────────────────────────────────────────

TEST_CASE("AnalogTimeSeries empty construction", "[bindings][analog]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run("ts = wt.AnalogTimeSeries()");
    auto r = run("print(ts.getNumSamples())");
    REQUIRE(r.stdout_text == "0\n");
    r = run("print(len(ts))");
    REQUIRE(r.stdout_text == "0\n");
}

TEST_CASE("AnalogTimeSeries from vectors", "[bindings][analog]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run(R"(
vals = [1.0, 2.0, 3.0, 4.0, 5.0]
times = [wt.TimeFrameIndex(i) for i in range(5)]
ts = wt.AnalogTimeSeries(vals, times)
)");
    auto r = run("print(ts.getNumSamples())");
    REQUIRE(r.stdout_text == "5\n");

    // toList (always works, no numpy)
    r = run("print(ts.toList())");
    REQUIRE(r.stdout_text.find("1.0") != std::string::npos);
    REQUIRE(r.stdout_text.find("5.0") != std::string::npos);
}

TEST_CASE("AnalogTimeSeries getAtTime", "[bindings][analog]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    run(R"(
ts = wt.AnalogTimeSeries([10.0, 20.0, 30.0], [wt.TimeFrameIndex(i) for i in range(3)])
val = ts.getAtTime(wt.TimeFrameIndex(1))
print(val)
)");
    // getAtTime returns optional — should print 20.0
    auto r = engine().execute("print(val)");
    REQUIRE(r.success);
    REQUIRE(r.stdout_text.find("20") != std::string::npos);
}

// ── DigitalEventSeries ─────────────────────────────────────────────────────

TEST_CASE("DigitalEventSeries construction and mutation", "[bindings][digital_event]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run("des = wt.DigitalEventSeries()");
    run("des.addEvent(wt.TimeFrameIndex(10))");
    run("des.addEvent(20)"); // int convenience
    auto r = run("print(des.size())");
    REQUIRE(r.stdout_text == "2\n");

    // Remove
    run("des.removeEvent(10)");
    r = run("print(des.size())");
    REQUIRE(r.stdout_text == "1\n");

    // Construct from list
    run("des2 = wt.DigitalEventSeries([wt.TimeFrameIndex(i) for i in [5, 15, 25]])");
    r = run("print(des2.size())");
    REQUIRE(r.stdout_text == "3\n");

    // toList
    r = run("print(len(des2.toList()))");
    REQUIRE(r.stdout_text == "3\n");
}

// ── DigitalIntervalSeries ──────────────────────────────────────────────────

TEST_CASE("DigitalIntervalSeries construction", "[bindings][digital_interval]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run("dis = wt.DigitalIntervalSeries()");
    run("dis.addInterval(0, 100)"); // int convenience
    run("dis.addEvent(wt.Interval(200, 300))");
    auto r = run("print(dis.size())");
    REQUIRE(r.stdout_text == "2\n");

    // toList
    r = run(R"(
ivs = dis.toList()
print(ivs[0].start, ivs[0].end)
)");
    REQUIRE(r.stdout_text.find("0") != std::string::npos);
    REQUIRE(r.stdout_text.find("100") != std::string::npos);
}

// ── Line2D / LineData ──────────────────────────────────────────────────────

TEST_CASE("Line2D construction and access", "[bindings][line]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run("line = wt.Line2D([wt.Point2D(0,0), wt.Point2D(1,1), wt.Point2D(2,4)])");
    auto r = run("print(line.size())");
    REQUIRE(r.stdout_text == "3\n");

    r = run("print(line[1].x, line[1].y)");
    REQUIRE(r.stdout_text == "1.0 1.0\n");

    // from x/y vectors
    run("line2 = wt.Line2D([0.0, 1.0], [0.0, 2.0])");
    r = run("print(line2.size())");
    REQUIRE(r.stdout_text == "2\n");
}

TEST_CASE("LineData add and retrieve", "[bindings][line]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
ld = wt.LineData()
ld.addAtTime(wt.TimeFrameIndex(0), wt.Line2D([wt.Point2D(0,0), wt.Point2D(1,1)]))
ld.addAtTime(wt.TimeFrameIndex(5), wt.Line2D([wt.Point2D(2,2), wt.Point2D(3,3)]))
)");
    auto r = run("print(ld.getTimeCount())");
    REQUIRE(r.stdout_text == "2\n");

    r = run("print(len(ld.getAtTime(wt.TimeFrameIndex(0))))");
    REQUIRE(r.stdout_text == "1\n");
}

// ── Mask2D / MaskData ──────────────────────────────────────────────────────

TEST_CASE("Mask2D construction", "[bindings][mask]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run("mask = wt.Mask2D([wt.Point2DU32(1,2), wt.Point2DU32(3,4)])");
    auto r = run("print(mask.size())");
    REQUIRE(r.stdout_text == "2\n");

    // from x/y coordinate vectors
    run("mask2 = wt.Mask2D([10, 20], [30, 40])");
    r = run("print(mask2.size())");
    REQUIRE(r.stdout_text == "2\n");
}

TEST_CASE("MaskData add and retrieve", "[bindings][mask]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
md = wt.MaskData()
md.addAtTime(wt.TimeFrameIndex(0), wt.Mask2D([wt.Point2DU32(1,1)]))
)");
    auto r = run("print(md.getTimeCount())");
    REQUIRE(r.stdout_text == "1\n");
}

// ── PointData ──────────────────────────────────────────────────────────────

TEST_CASE("PointData add and retrieve", "[bindings][point]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
pd = wt.PointData()
pd.addAtTime(wt.TimeFrameIndex(0), wt.Point2D(5.0, 10.0))
pd.addAtTime(wt.TimeFrameIndex(1), wt.Point2D(6.0, 11.0))
)");
    auto r = run("print(pd.getTimeCount())");
    REQUIRE(r.stdout_text == "2\n");

    r = run(R"(
pts = pd.getAtTime(wt.TimeFrameIndex(0))
print(pts[0].x, pts[0].y)
)");
    REQUIRE(r.stdout_text == "5.0 10.0\n");
}

// ── TensorData ─────────────────────────────────────────────────────────────

TEST_CASE("TensorData creation and access", "[bindings][tensor]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
td = wt.TensorData.createOrdinal2D([1.0, 2.0, 3.0, 4.0, 5.0, 6.0], 2, 3)
)");
    auto r = run("print(td.ndim())");
    REQUIRE(r.stdout_text == "2\n");
    r = run("print(td.numRows())");
    REQUIRE(r.stdout_text == "2\n");
    r = run("print(td.numColumns())");
    REQUIRE(r.stdout_text == "3\n");
    r = run("print(td.isEmpty())");
    REQUIRE(r.stdout_text == "False\n");

    // toList
    r = run("print(td.toList())");
    REQUIRE(r.stdout_text.find("1.0") != std::string::npos);

    // Column access
    r = run("print(td.getColumn(0))");
    REQUIRE(r.success);
}

TEST_CASE("TensorData named columns", "[bindings][tensor]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
td = wt.TensorData.createOrdinal2D(
    [1.0, 2.0, 3.0, 4.0], 2, 2, ['col_a', 'col_b'])
)");
    auto r = run("print(td.hasNamedColumns())");
    REQUIRE(r.stdout_text == "True\n");
    r = run("print(td.columnNames())");
    REQUIRE(r.stdout_text.find("col_a") != std::string::npos);
}

// ── DataManager ────────────────────────────────────────────────────────────

TEST_CASE("DataManager basic operations", "[bindings][datamanager]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run("dm = wt.DataManager()");

    // Create and register an AnalogTimeSeries
    run(R"(
ts = wt.AnalogTimeSeries([1.0, 2.0, 3.0], [wt.TimeFrameIndex(i) for i in range(3)])
dm.setData('my_analog', ts, 'time')
)");

    auto r = run("print('my_analog' in dm.getAllKeys())");
    REQUIRE(r.stdout_text == "True\n");

    r = run("print(dm.getType('my_analog'))");
    REQUIRE(r.stdout_text.find("Analog") != std::string::npos);

    // Retrieve typed data
    r = run(R"(
retrieved = dm.getData('my_analog')
print(type(retrieved).__name__)
print(retrieved.getNumSamples())
)");
    REQUIRE(r.stdout_text.find("AnalogTimeSeries") != std::string::npos);
    REQUIRE(r.stdout_text.find("3") != std::string::npos);

    // Delete
    r = run("print(dm.deleteData('my_analog'))");
    REQUIRE(r.stdout_text == "True\n");
    r = run("print(dm.getData('my_analog'))");
    REQUIRE(r.stdout_text == "None\n");
}

TEST_CASE("DataManager type-specific key queries", "[bindings][datamanager]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
dm = wt.DataManager()
dm.setData('ev1', wt.DigitalEventSeries(), 'time')
dm.setData('ev2', wt.DigitalEventSeries(), 'time')
dm.setData('line1', wt.LineData(), 'time')
)");

    auto r = run("print(len(dm.getDigitalEventKeys()))");
    REQUIRE(r.stdout_text == "2\n");

    r = run("print(len(dm.getLineKeys()))");
    REQUIRE(r.stdout_text == "1\n");
}

TEST_CASE("DataManager TimeFrame management", "[bindings][datamanager]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");

    run(R"(
dm = wt.DataManager()
tf = wt.TimeFrame([0, 100, 200])
dm.setTime('my_clock', tf)
)");

    auto r = run("print('my_clock' in dm.getTimeFrameKeys())");
    REQUIRE(r.stdout_text == "True\n");

    r = run("print(dm.getTime('my_clock').getTotalFrameCount())");
    REQUIRE(r.stdout_text == "3\n");
}

// ── DataType enum ──────────────────────────────────────────────────────────

TEST_CASE("DataType enum exposed", "[bindings][datamanager]") {
    Fixture f;
    run("import whiskertoolbox_python as wt");
    auto r = run("print(wt.DataType.Analog)");
    REQUIRE(r.stdout_text.find("Analog") != std::string::npos);
}
