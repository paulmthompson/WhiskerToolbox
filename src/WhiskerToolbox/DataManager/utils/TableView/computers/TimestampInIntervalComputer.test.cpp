#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "utils/TableView/computers/TimestampInIntervalComputer.h"
#include "DataManager.hpp"

TEST_CASE("TimestampInIntervalComputer basic integration", "[TimestampInIntervalComputer]") {
    DataManager dm;

    // Build a simple timeframe 0..9
    std::vector<int> times(10); for (int i=0;i<10;++i) times[i]=i;
    auto tf = std::make_shared<TimeFrame>(times);
    dm.setTime(TimeKey("cam"), tf, true);

    // Create digital interval series: [2,4] and [7,8]
    auto dis = std::make_shared<DigitalIntervalSeries>();
    dis->addEvent(Interval{2,4});
    dis->addEvent(Interval{7,8});
    dm.setData<DigitalIntervalSeries>("Intervals", dis, TimeKey("cam"));

    auto dme = std::make_shared<DataManagerExtension>(dm);

    // Timestamps: 1,3,5,8
    std::vector<TimeFrameIndex> ts = {TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(5), TimeFrameIndex(8)};
    auto selector = std::make_unique<TimestampSelector>(ts, tf);

    // Build computer directly
    auto src = dme->getIntervalSource("Intervals");
    REQUIRE(src != nullptr);
    auto comp = std::make_unique<TimestampInIntervalComputer>(src, src->getName());

    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(selector));
    builder.addColumn<bool>("Inside", std::move(comp));
    auto table = builder.build();

    auto const & vals = table.getColumnValues<bool>("Inside");
    REQUIRE(vals.size() == 4);
    // 1:false, 3:true, 5:false, 8:true
    REQUIRE(vals[0] == false);
    REQUIRE(vals[1] == true);
    REQUIRE(vals[2] == false);
    REQUIRE(vals[3] == true);
}

TEST_CASE("TimestampInIntervalComputer via registry", "[TimestampInIntervalComputer][Registry]") {
    DataManager dm;
    std::vector<int> times(6); for (int i=0;i<6;++i) times[i]=i;
    auto tf = std::make_shared<TimeFrame>(times);
    dm.setTime(TimeKey("cam"), tf, true);

    auto dis = std::make_shared<DigitalIntervalSeries>();
    dis->addEvent(Interval{0,2});
    dis->addEvent(Interval{4,5});
    dm.setData<DigitalIntervalSeries>("DInt", dis, TimeKey("cam"));

    auto dme = std::make_shared<DataManagerExtension>(dm);
    auto src = dme->getIntervalSource("DInt");
    REQUIRE(src != nullptr);

    ComputerRegistry registry;
    DataSourceVariant variant = DataSourceVariant{src};

    auto typed = registry.createTypedComputer<bool>("Timestamp In Interval", variant, {});
    REQUIRE(typed != nullptr);

    // Build table over timestamps 0..5
    std::vector<TimeFrameIndex> ts = {TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    auto selector = std::make_unique<TimestampSelector>(ts, tf);

    TableViewBuilder builder(dme);
    builder.setRowSelector(std::move(selector));
    builder.addColumn<bool>("InInt", std::move(typed));
    auto table = builder.build();

    auto const & vals = table.getColumnValues<bool>("InInt");
    REQUIRE(vals.size() == 6);
    // [0,2] => true,true,true,false,true,true
    REQUIRE(vals[0] == true);
    REQUIRE(vals[1] == true);
    REQUIRE(vals[2] == true);
    REQUIRE(vals[3] == false);
    REQUIRE(vals[4] == true);
    REQUIRE(vals[5] == true);
}


