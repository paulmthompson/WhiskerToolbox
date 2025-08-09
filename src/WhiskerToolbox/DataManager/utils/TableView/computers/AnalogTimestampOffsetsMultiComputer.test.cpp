#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.h"
#include "DataManager.hpp"

#include <map>
#include <memory>
#include <vector>

TEST_CASE("AnalogTimestampOffsetsMultiComputer basic integration", "[AnalogTimestampOffsetsMultiComputer]") {
    DataManager dm;

    // Create a simple analog series with values 0..9 over timeframe 0..9
    std::vector<float> analogVals(10);
    for (int i = 0; i < 10; ++i) analogVals[i] = static_cast<float>(i);
    auto times = std::vector<int>(10);
    for (int i = 0; i < 10; ++i) times[i] = i;
    auto tf = std::make_shared<TimeFrame>(times);
    auto analog = std::make_shared<AnalogTimeSeries>(analogVals, 10);
    dm.setData<AnalogTimeSeries>("A", analog, TimeKey("time"));

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);

    // Timestamps: 3, 5, 7
    std::vector<TimeFrameIndex> t = {TimeFrameIndex(3), TimeFrameIndex(5), TimeFrameIndex(7)};
    auto selector = std::make_unique<TimestampSelector>(t, tf);

    // Build adapter and computer directly
    auto src = dme_ptr->getAnalogSource("A");
    REQUIRE(src != nullptr);

    std::vector<int> offsets = {-2, -1, 0, 1};
    auto multi = std::make_unique<AnalogTimestampOffsetsMultiComputer>(src, "A", offsets);

    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(selector));
    builder.addColumns<double>("Aoffs", std::move(multi));
    auto table = builder.build();

    // Expect 4 columns in order: .t-2, .t-1, .t+0, .t+1
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 4);

    auto const & c_m2 = table.getColumnValues<double>("Aoffs.t-2");
    auto const & c_m1 = table.getColumnValues<double>("Aoffs.t-1");
    auto const & c_0  = table.getColumnValues<double>("Aoffs.t+0");
    auto const & c_p1 = table.getColumnValues<double>("Aoffs.t+1");

    REQUIRE(c_m2.size() == 3);
    REQUIRE(c_m1.size() == 3);
    REQUIRE(c_0.size() == 3);
    REQUIRE(c_p1.size() == 3);

    // For timestamp 3: values should be 1,2,3,4
    REQUIRE(c_m2[0] == Catch::Approx(1.0));
    REQUIRE(c_m1[0] == Catch::Approx(2.0));
    REQUIRE(c_0[0]  == Catch::Approx(3.0));
    REQUIRE(c_p1[0] == Catch::Approx(4.0));

    // For timestamp 5: 3,4,5,6
    REQUIRE(c_m2[1] == Catch::Approx(3.0));
    REQUIRE(c_m1[1] == Catch::Approx(4.0));
    REQUIRE(c_0[1]  == Catch::Approx(5.0));
    REQUIRE(c_p1[1] == Catch::Approx(6.0));

    // For timestamp 7: 5,6,7,8
    REQUIRE(c_m2[2] == Catch::Approx(5.0));
    REQUIRE(c_m1[2] == Catch::Approx(6.0));
    REQUIRE(c_0[2]  == Catch::Approx(7.0));
    REQUIRE(c_p1[2] == Catch::Approx(8.0));
}

TEST_CASE("AnalogTimestampOffsets via registry", "[AnalogTimestampOffsetsMultiComputer][Registry]") {
    DataManager dm;
    std::vector<float> analogVals(6);
    for (int i = 0; i < 6; ++i) analogVals[i] = static_cast<float>(i * 10);
    std::vector<int> times(6);
    for (int i = 0; i < 6; ++i) times[i] = i;
    auto tf = std::make_shared<TimeFrame>(times);
    auto analog = std::make_shared<AnalogTimeSeries>(analogVals, 6);
    dm.setData<AnalogTimeSeries>("B", analog, TimeKey("time"));

    ComputerRegistry registry;
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);

    // Adapt analog series to IAnalogSource
    // Registry uses DataSourceVariant; our computer will be registered as multi-output
    // with parameters specifying offsets (comma-separated list)
    auto analog_source = dme_ptr->getAnalogSource("B");
    REQUIRE(analog_source != nullptr);
    // DataSourceVariant variant = DataSourceVariant(*analog_source);

    std::map<std::string, std::string> params{{"offsets", "-1,0,2"}};
    auto multi = registry.createTypedMultiComputer<double>("Analog Timestamp Offsets", analog_source, params);
    REQUIRE(multi != nullptr);

    // Build a small table
    std::vector<TimeFrameIndex> t = {TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(4)};
    auto selector = std::make_unique<TimestampSelector>(t, tf);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(selector));
    builder.addColumns<double>("Boffs", std::move(multi));
    auto table = builder.build();

    // Expect 3 outputs
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 3);

    auto const & c_m1 = table.getColumnValues<double>("Boffs.t-1");
    auto const & c_0  = table.getColumnValues<double>("Boffs.t+0");
    auto const & c_p2 = table.getColumnValues<double>("Boffs.t+2");

    REQUIRE(c_m1.size() == 3);
    REQUIRE(c_0.size() == 3);
    REQUIRE(c_p2.size() == 3);

    // Values are multiples of 10
    // at t=1 -> 0,10,30
    REQUIRE(c_m1[0] == Catch::Approx(0.0));
    REQUIRE(c_0[0]  == Catch::Approx(10.0));
    REQUIRE(c_p2[0] == Catch::Approx(30.0));
}


