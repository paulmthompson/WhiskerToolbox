#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/adapters/LineDataAdapter.h"
#include "utils/TableView/computers/LineSamplingMultiComputer.h"
#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "utils/TableView/interfaces/ILineSource.h"

#include <memory>
#include <vector>

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


