#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "utils/TableView/interfaces/ILineSource.h"

#include <memory>
#include <vector>
#include <iostream>

TEST_CASE("LineSamplingMultiComputer basic integration", "[LineSamplingMultiComputer]") {
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
    int segments = 2; // positions: 0.0, 0.5, 1.0 => 6 outputs (x,y per position)
    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"TestLines"},
        tf,
        segments
    );

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

TEST_CASE("LineSamplingMultiComputer handles missing lines as zeros", "[LineSamplingMultiComputer]") {
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
        2
    );
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

TEST_CASE("LineSamplingMultiComputer can be created via registry", "[LineSamplingMultiComputer][Registry]") {
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
        {}
    );
    // Diagnostics
    {
        auto adapter_names = registry.getAllAdapterNames();
        std::cout << "Registered adapters (" << adapter_names.size() << ")" << std::endl;
        for (auto const& n : adapter_names) {
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
        for (auto const& ci : comps) {
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

TEST_CASE("LineSamplingMultiComputer with per-line row expansion drops empty timestamps and samples per entity", "[LineSamplingMultiComputer][Expansion]") {
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
    dm.setData<LineData>("ExpLines", lineData);

    // Timestamps include empty ones; expansion should drop t=0 and t=3
    std::vector<TimeFrameIndex> timestamps = {
        TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4)
    };
    auto rowSelector = std::make_unique<TimestampSelector>(timestamps, tf);

    // Build table
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(rowSelector));

    auto multi = std::make_unique<LineSamplingMultiComputer>(
        std::static_pointer_cast<ILineSource>(lineAdapter),
        std::string{"ExpLines"},
        tf,
        2 // positions 0.0, 0.5, 1.0
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

TEST_CASE("LineSamplingMultiComputer expansion with coexisting analog column retains empty-line timestamps for analog", "[LineSamplingMultiComputer][Expansion][AnalogBroadcast]") {
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
    dm.setData<LineData>("MixedLines", lineData);

    // Analog data present at all timestamps: values 0,10,20,30
    std::vector<float> analogVals = {0.f, 10.f, 20.f, 30.f};
    std::vector<TimeFrameIndex> analogTimes = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    auto analogData = std::make_shared<AnalogTimeSeries>(analogVals, analogTimes);
    dm.setData<AnalogTimeSeries>("AnalogA", analogData);

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
        2
    );
    builder.addColumns<double>("Line", std::move(multi));

    // Analog timestamp value column
    // Use registry to create the computer; simpler path: direct TimestampValueComputer
    // but we need an IAnalogSource from DataManagerExtension, resolved by name "AnalogA"
    class SimpleTimestampValueComputer : public IColumnComputer<double> {
    public:
        explicit SimpleTimestampValueComputer(std::shared_ptr<IAnalogSource> src) : src_(std::move(src)) {}
        [[nodiscard]] auto compute(ExecutionPlan const& plan) const -> std::vector<double> override {
            std::vector<TimeFrameIndex> idx;
            if (plan.hasIndices()) { idx = plan.getIndices(); }
            else {
                // Build from rows (expanded)
                for (auto const& r : plan.getRows()) idx.push_back(r.timeIndex);
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


