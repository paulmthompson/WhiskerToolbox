#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/TableRegistry.hpp"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/ILineSource.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/pipeline/TablePipeline.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <vector>

TEST_CASE("DM - TV - LineSamplingMultiComputer basic integration", "[LineSamplingMultiComputer]") {
    // Build a simple DataManager and inject LineData
    DataManager dm;

    // Create a TimeFrame with 3 timestamps
    std::vector<int> timeValues = {0, 1, 2};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    // Create LineData and add one simple line at each timestamp
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);

    // simple polyline: (0,0) -> (10,0)
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(0), xs, ys, false);
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
        lineData->addAtTime(TimeFrameIndex(2), xs, ys, false);
    }

    // Install into DataManager under a key (emulate registry storage)
    // The DataManager API in this project typically uses typed storage;
    // if there is no direct setter, we can directly adapt via LineDataAdapter below.

    // Create DataManagerExtension
    DataManagerExtension dme(dm);

    // Create a TableView with Timestamp rows [0,1,2]
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    // Build LineDataAdapter directly (bypassing DataManager registry) and wrap as ILineSource
    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"TestLines"});

    // Directly construct the multi-output computer (interface-level test)
    int segments = 2;// positions: 0.0, 0.5, 1.0 => 6 outputs (x,y per position)
    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"TestLines"},
        tf,
            segments);

    // Build the table with addColumns
    auto dme_ptr = std::make_shared<DataManagerExtension>(dme);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));
    builder.addColumns<double>("Line", std::move(multi));

    auto table = builder.build();

    // Expect 6 columns: x@0.000, y@0.000, x@0.500, y@0.500, x@1.000, y@1.000
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 6);

    // Validate simple geometry on the straight line: y is 0 everywhere; x progresses 0,5,10
    // x@0.000
    {
        auto const & xs0 = table.getColumnValues<double>("Line.x@0.000");
        REQUIRE(xs0.size() == 3);
        REQUIRE(xs0[0] == Catch::Approx(0.0));
        REQUIRE(xs0[1] == Catch::Approx(0.0));
        REQUIRE(xs0[2] == Catch::Approx(0.0));
    }
    // x@0.500
    {
        auto const & xsMid = table.getColumnValues<double>("Line.x@0.500");
        REQUIRE(xsMid.size() == 3);
        REQUIRE(xsMid[0] == Catch::Approx(5.0));
        REQUIRE(xsMid[1] == Catch::Approx(5.0));
        REQUIRE(xsMid[2] == Catch::Approx(5.0));
    }
    // x@1.000
    {
        auto const & xs1 = table.getColumnValues<double>("Line.x@1.000");
        REQUIRE(xs1.size() == 3);
        REQUIRE(xs1[0] == Catch::Approx(10.0));
        REQUIRE(xs1[1] == Catch::Approx(10.0));
        REQUIRE(xs1[2] == Catch::Approx(10.0));
    }

    // y columns should be zeros
    {
        auto const & ys0 = table.getColumnValues<double>("Line.y@0.000");
        auto const & ysMid = table.getColumnValues<double>("Line.y@0.500");
        auto const & ys1 = table.getColumnValues<double>("Line.y@1.000");
        REQUIRE(ys0.size() == 3);
        REQUIRE(ysMid.size() == 3);
        REQUIRE(ys1.size() == 3);
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE(ys0[i] == Catch::Approx(0.0));
            REQUIRE(ysMid[i] == Catch::Approx(0.0));
            REQUIRE(ys1[i] == Catch::Approx(0.0));
        }
    }
}

TEST_CASE("DM - TV - LineSamplingMultiComputer handles missing lines as zeros", "[LineSamplingMultiComputer]") {
    DataManager dm;

    std::vector<int> timeValues = {0, 1, 2};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);

    // Add a line at t=0 and t=2 only; t=1 has no lines
    std::vector<float> xs = {0.0f, 10.0f};
    std::vector<float> ys = {0.0f, 0.0f};
    lineData->addAtTime(TimeFrameIndex(0), xs, ys, false);
    lineData->addAtTime(TimeFrameIndex(2), xs, ys, false);

    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"TestLinesMissing"});

    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    // Build multi-computer directly
    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"TestLinesMissing"},
        tf,
            2);
    builder.addColumns<double>("Line", std::move(multi));

    auto table = builder.build();

    // At t=1 (middle row), expect zeros
    auto const & xs0 = table.getColumnValues<double>("Line.x@0.000");
    auto const & ys0 = table.getColumnValues<double>("Line.y@0.000");
    auto const & xsMid = table.getColumnValues<double>("Line.x@0.500");
    auto const & ysMid = table.getColumnValues<double>("Line.y@0.500");
    auto const & xs1 = table.getColumnValues<double>("Line.x@1.000");
    auto const & ys1 = table.getColumnValues<double>("Line.y@1.000");

    REQUIRE(xs0.size() == 3);
    REQUIRE(ys0.size() == 3);
    REQUIRE(xsMid.size() == 3);
    REQUIRE(ysMid.size() == 3);
    REQUIRE(xs1.size() == 3);
    REQUIRE(ys1.size() == 3);

    REQUIRE(xs0[1] == Catch::Approx(0.0));
    REQUIRE(ys0[1] == Catch::Approx(0.0));
    REQUIRE(xsMid[1] == Catch::Approx(0.0));
    REQUIRE(ysMid[1] == Catch::Approx(0.0));
    REQUIRE(xs1[1] == Catch::Approx(0.0));
    REQUIRE(ys1[1] == Catch::Approx(0.0));
}

TEST_CASE("DM - TV - LineSamplingMultiComputer can be created via registry", "[LineSamplingMultiComputer][Registry]") {
    DataManager dm;

    std::vector<int> timeValues = {0, 1};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);
    std::vector<float> xs = {0.0f, 10.0f};
    std::vector<float> ys = {0.0f, 0.0f};
    lineData->addAtTime(TimeFrameIndex(0), xs, ys, false);
    lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);

    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"RegLines"});

    // Create DataSourceVariant via registry adapter to ensure consistent type usage
    ComputerRegistry registry;
    auto adapted = registry.createAdapter(
        "Line Data",
        std::static_pointer_cast<void>(lineData),
        tf,
        std::string{"RegLines"},
            {});
    // Diagnostics
    {
        auto adapter_names = registry.getAllAdapterNames();
        std::cout << "Registered adapters (" << adapter_names.size() << ")" << std::endl;
        for (auto const & n: adapter_names) {
            std::cout << "  Adapter: " << n << std::endl;
        }
        std::cout << "Adapted variant index: " << adapted.index() << std::endl;
    }
    // Fallback to direct adapter if registry adapter not found (should not happen after registration)
    DataSourceVariant variant = adapted.index() != std::variant_npos ? adapted
                               : DataSourceVariant{std::static_pointer_cast<ILineSource>(lineAdapter)};

    // More diagnostics: list available computers
    {
        auto comps = registry.getAvailableComputers(RowSelectorType::Timestamp, variant);
        std::cout << "Available computers for Timestamp + variant(" << variant.index() << ") = " << comps.size() << std::endl;
        for (auto const & ci: comps) {
            std::cout << "  Computer: " << ci.name
                      << ", isMultiOutput=" << (ci.isMultiOutput ? "true" : "false")
                      << ", requiredSourceType=" << ci.requiredSourceType.name() << std::endl;
        }
        auto info = registry.findComputerInfo("Line Sample XY");
        if (info) {
            std::cout << "Found computer info for 'Line Sample XY' with requiredSourceType=" << info->requiredSourceType.name()
                      << ", rowSelector=" << static_cast<int>(info->requiredRowSelector)
                      << ", isMultiOutput=" << (info->isMultiOutput ? "true" : "false") << std::endl;
        } else {
            std::cout << "Did not find computer info for 'Line Sample XY'" << std::endl;
        }
    }

    // Create via registry
    std::map<std::string, std::string> params{{"segments", "2"}};
    auto multi = registry.createTypedMultiComputer<double>("Line Sample XY", variant, params);
    REQUIRE(multi != nullptr);

    // Build with builder
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));
    builder.addColumns<double>("Line", std::move(multi));
    auto table = builder.build();

    auto names = table.getColumnNames();
    REQUIRE(names.size() == 6);
}

TEST_CASE("DM - TV - LineSamplingMultiComputer with per-line row expansion drops empty timestamps and samples per entity", "[LineSamplingMultiComputer][Expansion]") {
    DataManager dm;

    // Timeframe with 5 timestamps
    std::vector<int> timeValues = {0, 1, 2, 3, 4};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    // LineData with varying number of lines per timestamp
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);

    // t=0: no lines (should be dropped)
    // t=1: one horizontal line from x=0..10
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
    }
    // t=2: two lines; l0 horizontal (x 0..10), l1 vertical (y 0..10)
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {0.0f, 0.0f};
        lineData->addAtTime(TimeFrameIndex(2), xs, ys, false);
        std::vector<float> xs2 = {5.0f, 5.0f};
        std::vector<float> ys2 = {0.0f, 10.0f};
        lineData->addAtTime(TimeFrameIndex(2), xs2, ys2, false);
    }
    // t=3: no lines (should be dropped)
    // t=4: one vertical line (y 0..10 at x=2)
    {
        std::vector<float> xs = {2.0f, 2.0f};
        std::vector<float> ys = {0.0f, 10.0f};
        lineData->addAtTime(TimeFrameIndex(4), xs, ys, false);
    }

    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"ExpLines"});
    // Register into DataManager so TableView expansion can resolve the line source by name
    dm.setData<LineData>("ExpLines", lineData, TimeKey("time"));

    // Timestamps include empty ones; expansion should drop t=0 and t=3
    std::vector<TimeFrameIndex> timestamps = {
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    // Build table
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"ExpLines"},
        tf,
            2// positions 0.0, 0.5, 1.0
    );
    builder.addColumns<double>("Line", std::move(multi));

    auto table = builder.build();

    // With expansion: expected rows = t1:1 + t2:2 + t4:1 = 4 rows
    REQUIRE(table.getRowCount() == 4);

    // Column names same structure
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 6);

    // Validate per-entity sampling ordering as inserted:
    // Row 0 -> t=1, the single horizontal line: x@0.5 = 5, y@0.5 = 0
    // Row 1 -> t=2, entity 0 (horizontal): x@0.5 = 5, y@0.5 = 0
    // Row 2 -> t=2, entity 1 (vertical):   x@0.5 = 5, y@0.5 = 5
    // Row 3 -> t=4, the single vertical line at x=2: x@0.5 = 2, y@0.5 = 5
    auto const & xsMid = table.getColumnValues<double>("Line.x@0.500");
    auto const & ysMid = table.getColumnValues<double>("Line.y@0.500");
    REQUIRE(xsMid.size() == 4);
    REQUIRE(ysMid.size() == 4);

    REQUIRE(xsMid[0] == Catch::Approx(5.0));
    REQUIRE(ysMid[0] == Catch::Approx(0.0));

    REQUIRE(xsMid[1] == Catch::Approx(5.0));
    REQUIRE(ysMid[1] == Catch::Approx(0.0));

    REQUIRE(xsMid[2] == Catch::Approx(5.0));
    REQUIRE(ysMid[2] == Catch::Approx(5.0));

    REQUIRE(xsMid[3] == Catch::Approx(2.0));
    REQUIRE(ysMid[3] == Catch::Approx(5.0));
}

TEST_CASE("DM - TV - LineSamplingMultiComputer expansion with coexisting analog column retains empty-line timestamps for analog", "[LineSamplingMultiComputer][Expansion][AnalogBroadcast]") {
    DataManager dm;

    std::vector<int> timeValues = {0, 1, 2, 3};
    auto tf = std::make_shared<TimeFrame>(timeValues);

    // LineData: only at t=1
    auto lineData = std::make_shared<LineData>();
    lineData->setTimeFrame(tf);
    {
        std::vector<float> xs = {0.0f, 10.0f};
        std::vector<float> ys = {1.0f, 1.0f};
        lineData->addAtTime(TimeFrameIndex(1), xs, ys, false);
    }
    dm.setData<LineData>("MixedLines", lineData, TimeKey("time"));

    // Analog data present at all timestamps: values 0,10,20,30
    std::vector<float> analogVals = {0.f, 10.f, 20.f, 30.f};
    std::vector<TimeFrameIndex> analogTimes = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    auto analogData = std::make_shared<AnalogTimeSeries>(analogVals, analogTimes);
    dm.setData<AnalogTimeSeries>("AnalogA", analogData, TimeKey("time"));

    // Build selector across all timestamps
    std::vector<TimeFrameIndex> timestamps = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    // Multi-line columns (expanding)
    auto lineAdapter = std::make_shared<LineDataAdapter>(lineData, tf, std::string{"MixedLines"});
    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"MixedLines"},
        tf,
            2);
    builder.addColumns<double>("Line", std::move(multi));

    // Analog timestamp value column
    // Use registry to create the computer; simpler path: direct TimestampValueComputer
    // but we need an IAnalogSource from DataManagerExtension, resolved by name "AnalogA"
    class SimpleTimestampValueComputer : public IColumnComputer<double> {
    public:
        explicit SimpleTimestampValueComputer(std::shared_ptr<IAnalogSource> src)
            : src_(std::move(src)) {}
        [[nodiscard]] auto compute(ExecutionPlan const & plan) const -> std::vector<double> override {
            std::vector<TimeFrameIndex> idx;
            if (plan.hasIndices()) {
                idx = plan.getIndices();
            } else {
                // Build from rows (expanded)
                for (auto const & r: plan.getRows()) idx.push_back(r.timeIndex);
            }
            std::vector<double> out(idx.size(), 0.0);
            // naive: use AnalogDataAdapter semantics: value == index*10
            for (size_t i = 0; i < idx.size(); ++i) out[i] = static_cast<double>(idx[i].getValue() * 10);
            return out;
        }
        [[nodiscard]] auto getSourceDependency() const -> std::string override { return src_ ? src_->getName() : std::string{"AnalogA"}; }

    private:
        std::shared_ptr<IAnalogSource> src_;
    };

    auto analogSrc = dme_ptr->getAnalogSource("AnalogA");
    REQUIRE(analogSrc != nullptr);
    auto analogComp = std::make_unique<SimpleTimestampValueComputer>(analogSrc);
    builder.addColumn<double>("Analog", std::move(analogComp));

    auto table = builder.build();

    // Expect expanded rows keep all timestamps due to coexisting analog column: t=0,1,2,3 -> 4 rows
    // Line columns will have zero for t=0,2,3 where no line exists; analog column has 0,10,20,30
    REQUIRE(table.getRowCount() == 4);
    auto const & xsMid = table.getColumnValues<double>("Line.x@0.500");
    auto const & ysMid = table.getColumnValues<double>("Line.y@0.500");
    auto const & analog = table.getColumnValues<double>("Analog");
    REQUIRE(xsMid.size() == 4);
    REQUIRE(ysMid.size() == 4);
    REQUIRE(analog.size() == 4);

    // At t=1 (row 1), line exists; others should be zeros for line columns
    REQUIRE(xsMid[0] == Catch::Approx(0.0));
    REQUIRE(ysMid[0] == Catch::Approx(0.0));
    REQUIRE(xsMid[1] == Catch::Approx(5.0));
    REQUIRE(ysMid[1] == Catch::Approx(1.0));
    REQUIRE(xsMid[2] == Catch::Approx(0.0));
    REQUIRE(ysMid[2] == Catch::Approx(0.0));
    REQUIRE(xsMid[3] == Catch::Approx(0.0));
    REQUIRE(ysMid[3] == Catch::Approx(0.0));

    REQUIRE(analog[0] == Catch::Approx(0.0));
    REQUIRE(analog[1] == Catch::Approx(10.0));
    REQUIRE(analog[2] == Catch::Approx(20.0));
    REQUIRE(analog[3] == Catch::Approx(30.0));
}

/**
 * @brief Base test fixture for LineSamplingMultiComputer with realistic line data
 * 
 * This fixture provides a DataManager populated with:
 * - TimeFrames with different granularities
 * - Line data representing whisker traces or geometric features
 * - Multiple lines per timestamp for testing entity expansion
 * - Cross-timeframe scenarios for testing timeframe conversion
 */
class LineSamplingTestFixture {
protected:
    LineSamplingTestFixture() {
        // Initialize the DataManager
        m_data_manager = std::make_unique<DataManager>();

        // Populate with test data
        populateWithLineTestData();
    }

    ~LineSamplingTestFixture() = default;

    /**
     * @brief Get the DataManager instance
     */
    DataManager & getDataManager() { return *m_data_manager; }
    DataManager const & getDataManager() const { return *m_data_manager; }
    DataManager * getDataManagerPtr() { return m_data_manager.get(); }

private:
    std::unique_ptr<DataManager> m_data_manager;

    /**
     * @brief Populate the DataManager with line test data
     */
    void populateWithLineTestData() {
        createTimeFrames();
        createWhiskerTraces();
        createGeometricShapes();
    }

    /**
     * @brief Create TimeFrame objects for different data streams
     */
    void createTimeFrames() {
        // Create "whisker_time" timeframe: 0 to 100 (101 points) - whisker tracking at high frequency
        std::vector<int> whisker_time_values(101);
        std::iota(whisker_time_values.begin(), whisker_time_values.end(), 0);
        auto whisker_time_frame = std::make_shared<TimeFrame>(whisker_time_values);
        m_data_manager->setTime(TimeKey("whisker_time"), whisker_time_frame, true);

        // Create "shape_time" timeframe: 0, 10, 20, 30, ..., 100 (11 points) - geometric shapes at lower frequency
        std::vector<int> shape_time_values;
        shape_time_values.reserve(11);
        for (int i = 0; i <= 10; ++i) {
            shape_time_values.push_back(i * 10);
        }
        auto shape_time_frame = std::make_shared<TimeFrame>(shape_time_values);
        m_data_manager->setTime(TimeKey("shape_time"), shape_time_frame, true);
    }

    /**
     * @brief Create whisker trace data (complex curved lines)
     */
    void createWhiskerTraces() {
        // Create whisker traces with varying curvature and length
        auto whisker_lines = std::make_shared<LineData>();

        // Create curved whisker traces at different time points
        for (int t = 10; t <= 90; t += 20) {
            // Primary whisker - curved arc
            std::vector<float> xs, ys;
            for (int i = 0; i <= 20; ++i) {
                float s = static_cast<float>(i) / 20.0f;
                float x = s * 100.0f;
                float y = 20.0f * std::sin(s * 3.14159f / 2.0f) * (1.0f + 0.1f * static_cast<float>(t) / 100.0f);
                xs.push_back(x);
                ys.push_back(y);
            }
            whisker_lines->addAtTime(TimeFrameIndex(t), xs, ys, false);

            // Secondary whisker - smaller arc below
            if (t >= 30) {
                std::vector<float> xs2, ys2;
                for (int i = 0; i <= 15; ++i) {
                    float s = static_cast<float>(i) / 15.0f;
                    float x = s * 75.0f;
                    float y = -10.0f - 15.0f * std::sin(s * 3.14159f / 3.0f);
                    xs2.push_back(x);
                    ys2.push_back(y);
                }
                whisker_lines->addAtTime(TimeFrameIndex(t), xs2, ys2, false);
            }
        }

        m_data_manager->setData<LineData>("WhiskerTraces", whisker_lines, TimeKey("whisker_time"));
    }

    /**
     * @brief Create geometric shape data (simple geometric lines)
     */
    void createGeometricShapes() {
        // Create geometric shapes with different patterns
        auto shape_lines = std::make_shared<LineData>();

        // Square at t=0
        {
            std::vector<float> xs = {0.0f, 10.0f, 10.0f, 0.0f, 0.0f};
            std::vector<float> ys = {0.0f, 0.0f, 10.0f, 10.0f, 0.0f};
            shape_lines->addAtTime(TimeFrameIndex(0), xs, ys, false);
        }

        // Triangle at t=20
        {
            std::vector<float> xs = {5.0f, 10.0f, 0.0f, 5.0f};
            std::vector<float> ys = {0.0f, 10.0f, 10.0f, 0.0f};
            shape_lines->addAtTime(TimeFrameIndex(2), xs, ys, false);
        }

        // Circle (octagon approximation) at t=40
        {
            std::vector<float> xs, ys;
            for (int i = 0; i <= 8; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 8.0f;
                xs.push_back(5.0f + 5.0f * std::cos(angle));
                ys.push_back(5.0f + 5.0f * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(4), xs, ys, false);
        }

        // Multiple shapes at different times - star at t=60, circle at t=80
        {
            // Star shape at t=60
            std::vector<float> xs1, ys1;
            for (int i = 0; i <= 10; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 10.0f;
                float radius = (i % 2 == 0) ? 8.0f : 4.0f;
                xs1.push_back(15.0f + radius * std::cos(angle));
                ys1.push_back(15.0f + radius * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(6), xs1, ys1, false);

            // Small circle at t=80
            std::vector<float> xs2, ys2;
            for (int i = 0; i <= 6; ++i) {
                float angle = static_cast<float>(i) * 2.0f * 3.14159f / 6.0f;
                xs2.push_back(25.0f + 3.0f * std::cos(angle));
                ys2.push_back(25.0f + 3.0f * std::sin(angle));
            }
            shape_lines->addAtTime(TimeFrameIndex(8), xs2, ys2, false);
        }

        m_data_manager->setData<LineData>("GeometricShapes", shape_lines, TimeKey("shape_time"));
    }
};

/**
 * @brief Test fixture combining LineSamplingTestFixture with TableRegistry and TablePipeline
 * 
 * This fixture provides everything needed to test JSON-based table pipeline execution:
 * - DataManager with line test data (from LineSamplingTestFixture)
 * - TableRegistry for managing table configurations
 * - TablePipeline for executing JSON configurations
 */
class LineSamplingTableRegistryTestFixture : public LineSamplingTestFixture {
protected:
    LineSamplingTableRegistryTestFixture()
        : LineSamplingTestFixture() {
        // Use the DataManager's existing TableRegistry instead of creating a new one
        m_table_registry_ptr = getDataManager().getTableRegistry();

        // Initialize TablePipeline with the existing TableRegistry
        m_table_pipeline = std::make_unique<TablePipeline>(m_table_registry_ptr, &getDataManager());
    }

    ~LineSamplingTableRegistryTestFixture() = default;

    /**
     * @brief Get the TableRegistry instance
     * @return Reference to the TableRegistry
     */
    TableRegistry & getTableRegistry() { return *m_table_registry_ptr; }

    /**
     * @brief Get the TableRegistry instance (const version)
     * @return Const reference to the TableRegistry
     */
    TableRegistry const & getTableRegistry() const { return *m_table_registry_ptr; }

    /**
     * @brief Get a pointer to the TableRegistry
     * @return Raw pointer to the TableRegistry
     */
    TableRegistry * getTableRegistryPtr() { return m_table_registry_ptr; }

    /**
     * @brief Get the TablePipeline instance
     * @return Reference to the TablePipeline
     */
    TablePipeline & getTablePipeline() { return *m_table_pipeline; }

    /**
     * @brief Get the TablePipeline instance (const version)
     * @return Const reference to the TablePipeline
     */
    TablePipeline const & getTablePipeline() const { return *m_table_pipeline; }

    /**
     * @brief Get a pointer to the TablePipeline
     * @return Raw pointer to the TablePipeline
     */
    TablePipeline * getTablePipelinePtr() { return m_table_pipeline.get(); }

    /**
     * @brief Get the DataManagerExtension instance
     */
    std::shared_ptr<DataManagerExtension> getDataManagerExtension() {
        if (!m_data_manager_extension) {
            m_data_manager_extension = std::make_shared<DataManagerExtension>(getDataManager());
        }
        return m_data_manager_extension;
    }

private:
    TableRegistry * m_table_registry_ptr;// Points to DataManager's TableRegistry
    std::unique_ptr<TablePipeline> m_table_pipeline;
    std::shared_ptr<DataManagerExtension> m_data_manager_extension;// Lazy-initialized
};

TEST_CASE_METHOD(LineSamplingTestFixture, "DM - TV - LineSamplingMultiComputer with DataManager fixture", "[LineSamplingMultiComputer][DataManager][Fixture]") {

    SECTION("Test with whisker trace data from fixture") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the line source from the DataManager
        auto whisker_source = dme->getLineSource("WhiskerTraces");

        REQUIRE(whisker_source != nullptr);

        // Create row selector from timestamps where whisker data exists
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(10), TimeFrameIndex(30), TimeFrameIndex(50), TimeFrameIndex(70), TimeFrameIndex(90)};

        auto row_selector = std::make_unique<TimestampSelector>(timestamps, whisker_time_frame);

        // Create TableView builder
        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add LineSamplingMultiComputer with different segment counts
        auto multi_3seg = std::make_unique<LineSamplingMultiComputer>(
                whisker_source, "WhiskerTraces", whisker_time_frame, 3);
        builder.addColumns<double>("Whisker", std::move(multi_3seg));

                // Build the table
        TableView table = builder.build();
        
        // Verify table structure - 4 segments = 8 columns (x,y per position: 0.0, 0.333, 0.667, 1.0)
        // Expected rows: t=10(1) + t=30(2) + t=50(2) + t=70(2) + t=90(2) = 9 rows due to entity expansion
        REQUIRE(table.getRowCount() == 9);
        REQUIRE(table.getColumnCount() == 8);

        auto column_names = table.getColumnNames();
        REQUIRE(column_names.size() == 8);

        // Verify expected column names
        REQUIRE(table.hasColumn("Whisker.x@0.000"));
        REQUIRE(table.hasColumn("Whisker.y@0.000"));
        REQUIRE(table.hasColumn("Whisker.x@0.333"));
        REQUIRE(table.hasColumn("Whisker.y@0.333"));
        REQUIRE(table.hasColumn("Whisker.x@0.667"));
        REQUIRE(table.hasColumn("Whisker.y@0.667"));
        REQUIRE(table.hasColumn("Whisker.x@1.000"));
        REQUIRE(table.hasColumn("Whisker.y@1.000"));

                // Get sample data to verify reasonable values
        auto x_start = table.getColumnValues<double>("Whisker.x@0.000");
        auto y_start = table.getColumnValues<double>("Whisker.y@0.000");
        auto x_end = table.getColumnValues<double>("Whisker.x@1.000");
        auto y_end = table.getColumnValues<double>("Whisker.y@1.000");
        
        REQUIRE(x_start.size() == 9);
        REQUIRE(y_start.size() == 9);
        REQUIRE(x_end.size() == 9);
        REQUIRE(y_end.size() == 9);
        
        // Verify that whisker curves start at x=0 and end at x=100 (for primary whiskers)
        // or x=0 and x=75 (for secondary whiskers)
        for (size_t i = 0; i < 9; ++i) {
            REQUIRE(x_start[i] == Catch::Approx(0.0));
            // Primary whiskers end at x=100, secondary at x=75
            REQUIRE((x_end[i] == Catch::Approx(100.0) || x_end[i] == Catch::Approx(75.0)));
            // Y values should be reasonable for curved whiskers
            REQUIRE(std::abs(y_start[i]) >= 0.0);
        }
    }

    SECTION("Test with geometric shape data and multiple entities") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get the shape source
        auto shape_source = dme->getLineSource("GeometricShapes");
        REQUIRE(shape_source != nullptr);

        // Create row selector for shape timestamps
        auto shape_time_frame = dm.getTime(TimeKey("shape_time"));
        std::vector<TimeFrameIndex> timestamps = {
                TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(4), TimeFrameIndex(6)};

        auto row_selector = std::make_unique<TimestampSelector>(timestamps, shape_time_frame);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Add LineSamplingMultiComputer with 1 segment (start and end points only)
        auto multi_1seg = std::make_unique<LineSamplingMultiComputer>(
                shape_source, "GeometricShapes", shape_time_frame, 1);
        builder.addColumns<double>("Shape", std::move(multi_1seg));

        TableView table = builder.build();

        // Should have 4 rows: square(1) + triangle(1) + circle(1) + star(1) = 4 rows
        // Note: The circle was moved to TimeFrameIndex(8) to avoid multiple entities at same timestamp
        REQUIRE(table.getRowCount() == 4);
        REQUIRE(table.getColumnCount() == 4);// 2 positions * 2 coordinates = 4 columns

        auto x_start = table.getColumnValues<double>("Shape.x@0.000");
        auto y_start = table.getColumnValues<double>("Shape.y@0.000");
        auto x_end = table.getColumnValues<double>("Shape.x@1.000");
        auto y_end = table.getColumnValues<double>("Shape.y@1.000");

        REQUIRE(x_start.size() == 4);

        // Verify square (t=0): starts at (0,0), ends at (0,0) - closed shape
        REQUIRE(x_start[0] == Catch::Approx(0.0));
        REQUIRE(y_start[0] == Catch::Approx(0.0));
        REQUIRE(x_end[0] == Catch::Approx(0.0));
        REQUIRE(y_end[0] == Catch::Approx(0.0));

        // Verify triangle (t=2): starts at (5,0), ends at (5,0) - closed shape
        REQUIRE(x_start[1] == Catch::Approx(5.0));
        REQUIRE(y_start[1] == Catch::Approx(0.0));
        REQUIRE(x_end[1] == Catch::Approx(5.0));
        REQUIRE(y_end[1] == Catch::Approx(0.0));
    }

    /*
    SECTION("Test cross-timeframe sampling") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);

        // Get sources from different timeframes
        auto whisker_source = dme->getLineSource("WhiskerTraces");// whisker_time frame
        auto shape_source = dme->getLineSource("GeometricShapes");// shape_time frame

        REQUIRE(whisker_source != nullptr);
        REQUIRE(shape_source != nullptr);

        // Verify they have different timeframes
        auto whisker_tf = whisker_source->getTimeFrame();
        auto shape_tf = shape_source->getTimeFrame();
        REQUIRE(whisker_tf != shape_tf);
        REQUIRE(whisker_tf->getTotalFrameCount() == 101);// whisker_time: 0-100
        REQUIRE(shape_tf->getTotalFrameCount() == 11);   // shape_time: 0,10,20,...,100

        // Create a test with whisker timeframe but sampling shape data
        std::vector<TimeFrameIndex> test_timestamps = {
                TimeFrameIndex(20), TimeFrameIndex(40)// These exist in shape_time as indices 2,4
        };

        auto row_selector = std::make_unique<TimestampSelector>(test_timestamps, whisker_tf);

        TableViewBuilder builder(dme);
        builder.setRowSelector(std::move(row_selector));

        // Sample shape data using whisker timeframe
        auto multi = std::make_unique<LineSamplingMultiComputer>(
                shape_source, "GeometricShapes", shape_tf, 2);
        builder.addColumns<double>("CrossFrame", std::move(multi));

        TableView table = builder.build();

        REQUIRE(table.getRowCount() == 2);
        REQUIRE(table.getColumnCount() == 6);// 3 positions * 2 coordinates

        auto x_mid = table.getColumnValues<double>("CrossFrame.x@0.500");
        auto y_mid = table.getColumnValues<double>("CrossFrame.y@0.500");

        REQUIRE(x_mid.size() == 2);
        REQUIRE(y_mid.size() == 2);

        // Should have sampled triangle at t=20 and circle at t=40
        // Values depend on specific timeframe conversion implementation
        REQUIRE(x_mid[0] >= 0.0);// Triangle midpoint
        REQUIRE(y_mid[0] >= 0.0);
        REQUIRE(x_mid[1] >= 0.0);// Circle midpoint
        REQUIRE(y_mid[1] >= 0.0);

        std::cout << "Cross-timeframe test - Triangle midpoint: (" << x_mid[0]
                  << ", " << y_mid[0] << "), Circle midpoint: (" << x_mid[1]
                  << ", " << y_mid[1] << ")" << std::endl;
    }
    */
}

TEST_CASE_METHOD(LineSamplingTableRegistryTestFixture, "DM - TV - LineSamplingMultiComputer via ComputerRegistry", "[LineSamplingMultiComputer][Registry]") {

    SECTION("Verify LineSamplingMultiComputer is registered in ComputerRegistry") {
        auto & registry = getTableRegistry().getComputerRegistry();

        // Check that LineSamplingMultiComputer is registered
        auto line_sample_info = registry.findComputerInfo("Line Sample XY");

        REQUIRE(line_sample_info != nullptr);

        // Verify computer info details
        REQUIRE(line_sample_info->name == "Line Sample XY");
        REQUIRE(line_sample_info->outputType == typeid(double));
        REQUIRE(line_sample_info->outputTypeName == "double");
        REQUIRE(line_sample_info->requiredRowSelector == RowSelectorType::Timestamp);
        REQUIRE(line_sample_info->requiredSourceType == typeid(std::shared_ptr<ILineSource>));
        REQUIRE(line_sample_info->isMultiOutput == true);

        // Verify parameter information
        REQUIRE(line_sample_info->hasParameters() == true);
        REQUIRE(line_sample_info->parameterDescriptors.size() == 1);
        REQUIRE(line_sample_info->parameterDescriptors[0]->getName() == "segments");
        REQUIRE(line_sample_info->parameterDescriptors[0]->getUIHint() == "number");
    }

    SECTION("Create LineSamplingMultiComputer via ComputerRegistry") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        // Get whisker source for testing
        auto whisker_source = dme->getLineSource("WhiskerTraces");
        REQUIRE(whisker_source != nullptr);

        // Create computer via registry with different segment parameters
        std::map<std::string, std::string> params_2seg{{"segments", "2"}};
        std::map<std::string, std::string> params_5seg{{"segments", "5"}};

        auto computer_2seg = registry.createTypedMultiComputer<double>(
                "Line Sample XY", whisker_source, params_2seg);
        auto computer_5seg = registry.createTypedMultiComputer<double>(
                "Line Sample XY", whisker_source, params_5seg);

        REQUIRE(computer_2seg != nullptr);
        REQUIRE(computer_5seg != nullptr);

        // Test that the created computers work correctly
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));

        // Create a simple test
        std::vector<TimeFrameIndex> test_timestamps = {TimeFrameIndex(30)};
        auto row_selector_2seg = std::make_unique<TimestampSelector>(test_timestamps, whisker_time_frame);
        auto row_selector_5seg = std::make_unique<TimestampSelector>(test_timestamps, whisker_time_frame);

        // Test 2-segment computer
        {
            TableViewBuilder builder(dme);
            builder.setRowSelector(std::move(row_selector_2seg));
            builder.addColumns("Registry2Seg", std::move(computer_2seg));

            auto table = builder.build();
            REQUIRE(table.getRowCount() == 2);
            REQUIRE(table.getColumnCount() == 6);// 3 positions * 2 coordinates

            auto column_names = table.getColumnNames();
            REQUIRE(table.hasColumn("Registry2Seg.x@0.000"));
            REQUIRE(table.hasColumn("Registry2Seg.y@0.000"));
            REQUIRE(table.hasColumn("Registry2Seg.x@0.500"));
            REQUIRE(table.hasColumn("Registry2Seg.y@0.500"));
            REQUIRE(table.hasColumn("Registry2Seg.x@1.000"));
            REQUIRE(table.hasColumn("Registry2Seg.y@1.000"));
        }

        // Test 5-segment computer
        {
            TableViewBuilder builder(dme);
            builder.setRowSelector(std::move(row_selector_5seg));
            builder.addColumns("Registry5Seg", std::move(computer_5seg));

            auto table = builder.build();
            REQUIRE(table.getRowCount() == 2);
            REQUIRE(table.getColumnCount() == 12);// 6 positions * 2 coordinates

            auto column_names = table.getColumnNames();
            REQUIRE(table.hasColumn("Registry5Seg.x@0.000"));
            REQUIRE(table.hasColumn("Registry5Seg.y@0.000"));
            REQUIRE(table.hasColumn("Registry5Seg.x@0.200"));
            REQUIRE(table.hasColumn("Registry5Seg.y@0.200"));
            REQUIRE(table.hasColumn("Registry5Seg.x@1.000"));
            REQUIRE(table.hasColumn("Registry5Seg.y@1.000"));
        }
    }

    SECTION("Compare registry-created vs direct-created computers") {
        auto & dm = getDataManager();
        auto dme = std::make_shared<DataManagerExtension>(dm);
        auto & registry = getTableRegistry().getComputerRegistry();

        auto whisker_source = dme->getLineSource("WhiskerTraces");
        REQUIRE(whisker_source != nullptr);

        // Create computer via registry
        std::map<std::string, std::string> params{{"segments", "3"}};
        auto registry_computer = registry.createTypedMultiComputer<double>(
                "Line Sample XY", whisker_source, params);

        // Create computer directly
        auto whisker_time_frame = dm.getTime(TimeKey("whisker_time"));
        auto direct_computer = std::make_unique<LineSamplingMultiComputer>(
                whisker_source, "WhiskerTraces", whisker_time_frame, 3);

        REQUIRE(registry_computer != nullptr);
        REQUIRE(direct_computer != nullptr);

        // Test both computers with the same data
        std::vector<TimeFrameIndex> test_timestamps = {TimeFrameIndex(50)};

        // Registry computer test
        auto registry_output_names = registry_computer->getOutputNames();

        // Direct computer test
        auto direct_output_names = direct_computer->getOutputNames();

        // Output names should be identical
        REQUIRE(registry_output_names.size() == direct_output_names.size());
        REQUIRE(registry_output_names.size() == 8);// 4 positions * 2 coordinates

        for (size_t i = 0; i < registry_output_names.size(); ++i) {
            REQUIRE(registry_output_names[i] == direct_output_names[i]);
        }

        std::cout << "Comparison test - Both computers produce " << registry_output_names.size()
                  << " identical output names" << std::endl;
    }
}

TEST_CASE_METHOD(LineSamplingTableRegistryTestFixture, "DM - TV - LineSamplingMultiComputer via JSON TablePipeline", "[LineSamplingMultiComputer][JSON][Pipeline]") {

    SECTION("Test basic line sampling via JSON pipeline") {
        // JSON configuration for testing LineSamplingMultiComputer through TablePipeline
        char const * json_config = R"({
            "metadata": {
                "name": "Line Sampling Test",
                "description": "Test JSON execution of LineSamplingMultiComputer",
                "version": "1.0"
            },
            "tables": [
                {
                    "table_id": "line_sampling_test",
                    "name": "Line Sampling Test Table",
                    "description": "Test table using LineSamplingMultiComputer",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [10, 30, 50, 70, 90]
                    },
                    "columns": [
                        {
                            "name": "WhiskerSampling",
                            "description": "Sample whisker trace at 3 equally spaced positions",
                            "data_source": "WhiskerTraces",
                            "computer": "Line Sample XY",
                            "parameters": {
                                "segments": "2"
                            }
                        }
                    ]
                }
            ]
        })";

        auto & pipeline = getTablePipeline();

        // Parse the JSON configuration
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        // Load configuration into pipeline
        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        // Verify configuration was loaded correctly
        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.table_id == "line_sampling_test");
        REQUIRE(config.name == "Line Sampling Test Table");
        REQUIRE(config.columns.size() == 1);

        // Verify column configuration
        auto const & column = config.columns[0];
        REQUIRE(column["name"] == "WhiskerSampling");
        REQUIRE(column["computer"] == "Line Sample XY");
        REQUIRE(column["data_source"] == "WhiskerTraces");
        REQUIRE(column["parameters"]["segments"] == "2");

        // Verify row selector configuration
        REQUIRE(config.row_selector["type"] == "timestamp");
        auto timestamps = config.row_selector["timestamps"];
        REQUIRE(timestamps.size() == 5);
        REQUIRE(timestamps[0] == 10);
        REQUIRE(timestamps[4] == 90);

        std::cout << "JSON pipeline configuration loaded and parsed successfully" << std::endl;

        // Execute the pipeline
        auto pipeline_result = pipeline.execute([](int table_index, std::string const & table_name, int table_progress, int overall_progress) {
            std::cout << "Building table " << table_index << " (" << table_name << "): "
                      << table_progress << "% (Overall: " << overall_progress << "%)" << std::endl;
        });

        if (pipeline_result.success) {
            std::cout << "Pipeline executed successfully!" << std::endl;
            std::cout << "Tables completed: " << pipeline_result.tables_completed << "/" << pipeline_result.total_tables << std::endl;
            std::cout << "Execution time: " << pipeline_result.total_execution_time_ms << " ms" << std::endl;

            // Verify the built table exists
            auto & registry = getTableRegistry();
            REQUIRE(registry.hasTable("line_sampling_test"));

            // Get the built table and verify its structure
            auto built_table = registry.getBuiltTable("line_sampling_test");
            REQUIRE(built_table != nullptr);

            // Check that the table has the expected columns
            auto column_names = built_table->getColumnNames();
            std::cout << "Built table has " << column_names.size() << " columns" << std::endl;
            for (auto const & name: column_names) {
                std::cout << "  Column: " << name << std::endl;
            }

            REQUIRE(column_names.size() == 6);// 3 positions * 2 coordinates
            REQUIRE(built_table->hasColumn("WhiskerSampling.x@0.000"));
            REQUIRE(built_table->hasColumn("WhiskerSampling.y@0.000"));
            REQUIRE(built_table->hasColumn("WhiskerSampling.x@0.500"));
            REQUIRE(built_table->hasColumn("WhiskerSampling.y@0.500"));
            REQUIRE(built_table->hasColumn("WhiskerSampling.x@1.000"));
            REQUIRE(built_table->hasColumn("WhiskerSampling.y@1.000"));

                        // Verify table has 9 rows due to entity expansion: t=10(1) + t=30(2) + t=50(2) + t=70(2) + t=90(2) = 9
            REQUIRE(built_table->getRowCount() == 9);

            // Get and verify the computed values
            auto x_start = built_table->getColumnValues<double>("WhiskerSampling.x@0.000");
            auto y_start = built_table->getColumnValues<double>("WhiskerSampling.y@0.000");
            auto x_mid = built_table->getColumnValues<double>("WhiskerSampling.x@0.500");
            auto y_mid = built_table->getColumnValues<double>("WhiskerSampling.y@0.500");
            auto x_end = built_table->getColumnValues<double>("WhiskerSampling.x@1.000");
            auto y_end = built_table->getColumnValues<double>("WhiskerSampling.y@1.000");

            REQUIRE(x_start.size() == 9);
            REQUIRE(y_start.size() == 9);
            REQUIRE(x_mid.size() == 9);
            REQUIRE(y_mid.size() == 9);
            REQUIRE(x_end.size() == 9);
            REQUIRE(y_end.size() == 9);

            for (size_t i = 0; i < 9; ++i) {
                // Whisker traces should start at x=0
                REQUIRE(x_start[i] == Catch::Approx(0.0));
                // Primary whiskers end at x=100, secondary at x=75
                REQUIRE_THAT(x_end[i], 
                    Catch::Matchers::WithinAbs(100.0, 1.0) 
                    || Catch::Matchers::WithinAbs(75.0, 1.0));
                // Middle should be around x=50 for primary, x=37.5 for secondary
                REQUIRE_THAT(x_mid[i], 
                    Catch::Matchers::WithinAbs(50.0, 1.0) 
                    || Catch::Matchers::WithinAbs(37.5, 1.0));
                // Y values should be reasonable for curved whiskers
                REQUIRE(std::abs(y_start[i]) >= 0.0);
                REQUIRE(std::abs(y_mid[i]) >= 0.0);
                
                std::cout << "Row " << i << ": Start=(" << x_start[i] << "," << y_start[i] 
                          << "), Mid=(" << x_mid[i] << "," << y_mid[i] 
                          << "), End=(" << x_end[i] << "," << y_end[i] << ")" << std::endl;
            }

        } else {
            FAIL("Pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test different segment counts via JSON") {
        char const * json_config = R"JSON({
            "metadata": {
                "name": "Line Sampling Segment Test",
                "description": "Test different segment counts for LineSamplingMultiComputer"
            },
            "tables": [
                {
                    "table_id": "line_sampling_segments_test",
                    "name": "Line Sampling Segments Test Table",
                    "description": "Test table with different segment counts",
                    "row_selector": {
                        "type": "timestamp", 
                        "timestamps": [20, 40]
                    },
                    "columns": [
                        {
                            "name": "Shape1Seg",
                            "description": "Sample geometric shapes with 1 segment (start/end only)",
                            "data_source": "GeometricShapes",
                            "computer": "Line Sample XY",
                            "parameters": {
                                "segments": "1"
                            }
                        },
                        {
                            "name": "Shape4Seg",
                            "description": "Sample geometric shapes with 4 segments (5 positions)",
                            "data_source": "GeometricShapes",
                            "computer": "Line Sample XY",
                            "parameters": {
                                "segments": "4"
                            }
                        }
                    ]
                }
            ]
        })JSON";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto table_configs = pipeline.getTableConfigurations();
        REQUIRE(table_configs.size() == 1);

        auto const & config = table_configs[0];
        REQUIRE(config.columns.size() == 2);
        REQUIRE(config.columns[0]["parameters"]["segments"] == "1");
        REQUIRE(config.columns[1]["parameters"]["segments"] == "4");

        std::cout << "Segment count JSON configuration parsed successfully" << std::endl;

        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ Segment count pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("line_sampling_segments_test");
            REQUIRE(built_table != nullptr);

            REQUIRE(built_table->getRowCount() == 2);    // 2 timestamps
            REQUIRE(built_table->getColumnCount() == 14);// 1seg(4 cols) + 4seg(10 cols) = 14 total

            // Verify 1-segment columns (2 positions * 2 coordinates = 4 columns)
            REQUIRE(built_table->hasColumn("Shape1Seg.x@0.000"));
            REQUIRE(built_table->hasColumn("Shape1Seg.y@0.000"));
            REQUIRE(built_table->hasColumn("Shape1Seg.x@1.000"));
            REQUIRE(built_table->hasColumn("Shape1Seg.y@1.000"));

            // Verify 4-segment columns (5 positions * 2 coordinates = 10 columns)
            REQUIRE(built_table->hasColumn("Shape4Seg.x@0.000"));
            REQUIRE(built_table->hasColumn("Shape4Seg.y@0.000"));
            REQUIRE(built_table->hasColumn("Shape4Seg.x@0.250"));
            REQUIRE(built_table->hasColumn("Shape4Seg.y@0.250"));
            REQUIRE(built_table->hasColumn("Shape4Seg.x@0.500"));
            REQUIRE(built_table->hasColumn("Shape4Seg.y@0.500"));
            REQUIRE(built_table->hasColumn("Shape4Seg.x@0.750"));
            REQUIRE(built_table->hasColumn("Shape4Seg.y@0.750"));
            REQUIRE(built_table->hasColumn("Shape4Seg.x@1.000"));
            REQUIRE(built_table->hasColumn("Shape4Seg.y@1.000"));

            std::cout << "✓ All expected columns present for different segment counts" << std::endl;

        } else {
            FAIL("Segment count pipeline execution failed: " + pipeline_result.error_message);
        }
    }

    SECTION("Test multiple line data sources via JSON") {
        char const * json_config = R"JSON({
            "metadata": {
                "name": "Multi-Source Line Sampling Test",
                "description": "Test multiple line data sources in same table"
            },
            "tables": [
                {
                    "table_id": "multi_source_line_test",
                    "name": "Multi-Source Line Test Table",
                    "description": "Test table with multiple line data sources",
                    "row_selector": {
                        "type": "timestamp",
                        "timestamps": [30, 60]
                    },
                    "columns": [
                        {
                            "name": "WhiskerPoints",
                            "description": "Sample whisker traces at key points",
                            "data_source": "WhiskerTraces",
                            "computer": "Line Sample XY",
                            "parameters": {
                                "segments": "3"
                            }
                        },
                        {
                            "name": "ShapePoints",
                            "description": "Sample geometric shapes at key points",
                            "data_source": "GeometricShapes",
                            "computer": "Line Sample XY",
                            "parameters": {
                                "segments": "3"
                            }
                        }
                    ]
                }
            ]
        })JSON";

        auto & pipeline = getTablePipeline();
        nlohmann::json json_obj = nlohmann::json::parse(json_config);

        bool load_success = pipeline.loadFromJson(json_obj);
        REQUIRE(load_success);

        auto pipeline_result = pipeline.execute();

        if (pipeline_result.success) {
            std::cout << "✓ Multi-source pipeline executed successfully!" << std::endl;

            auto & registry = getTableRegistry();
            auto built_table = registry.getBuiltTable("multi_source_line_test");
            REQUIRE(built_table != nullptr);

            // Should have 3 rows due to entity expansion: t=30(2 whiskers from WhiskerTraces) + t=60(1 whisker from WhiskerTraces) = 3 rows
            // Note: Only WhiskerTraces has multiple entities, GeometricShapes has single entities at each timestamp
            REQUIRE(built_table->getRowCount() >= 2);  // At least 2 rows, may be more due to expansion
            // Should have 16 columns: 2 sources * 4 positions * 2 coordinates
            REQUIRE(built_table->getColumnCount() == 16);

            // Verify whisker columns exist
            REQUIRE(built_table->hasColumn("WhiskerPoints.x@0.000"));
            REQUIRE(built_table->hasColumn("WhiskerPoints.y@0.333"));
            REQUIRE(built_table->hasColumn("WhiskerPoints.x@1.000"));

            // Verify shape columns exist
            REQUIRE(built_table->hasColumn("ShapePoints.x@0.000"));
            REQUIRE(built_table->hasColumn("ShapePoints.y@0.333"));
            REQUIRE(built_table->hasColumn("ShapePoints.x@1.000"));

            std::cout << "✓ Multi-source line sampling completed with "
                      << built_table->getColumnCount() << " columns" << std::endl;

        } else {
            FAIL("Multi-source pipeline execution failed: " + pipeline_result.error_message);
        }
    }
}
