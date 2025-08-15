#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include "DataManager.hpp"
#include "utils/TableView/ComputerRegistry.hpp"
#include "utils/TableView/adapters/AnalogDataAdapter.h"
#include "utils/TableView/adapters/DataManagerExtension.h"
#include "utils/TableView/computers/AnalogTimestampOffsetsMultiComputer.h"
#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/interfaces/IRowSelector.h"

#include "../fixtures/analog_test_fixtures.hpp"

#include <map>
#include <memory>
#include <vector>

TEST_CASE_METHOD(AnalogTestFixture, "DM - TV - AnalogTimestampOffsets with fixture triangular signals", "[AnalogTimestampOffsetsMultiComputer][Fixture]") {
    auto & dm = getDataManager();

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);

    // Test signal A (triangular wave in "time" timeframe)
    auto src_a = dme_ptr->getAnalogSource("A");
    REQUIRE(src_a != nullptr);

    // Test at key points of the triangular wave
    // At t=100: value should be 100 (rising edge)
    // At t=500: value should be 500 (peak)
    // At t=700: value should be 300 (falling edge: 1000-700=300)
    std::vector<TimeFrameIndex> test_times = {
            TimeFrameIndex(100),
            TimeFrameIndex(500),
            TimeFrameIndex(700)};

    auto time_frame = dm.getTime(TimeKey("time"));
    auto selector = std::make_unique<TimestampSelector>(test_times, time_frame);

    std::vector<int> offsets = {-50, 0, 50};
    auto multi = std::make_unique<AnalogTimestampOffsetsMultiComputer>(src_a, "A", offsets);

    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(selector));
    builder.addColumns<double>("Aoffs", std::move(multi));
    auto table = builder.build();

    // Verify column structure
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 3);

    auto const & c_m50 = table.getColumnValues<double>("Aoffs.t-50");
    auto const & c_0 = table.getColumnValues<double>("Aoffs.t+0");
    auto const & c_p50 = table.getColumnValues<double>("Aoffs.t+50");

    REQUIRE(c_m50.size() == 3);
    REQUIRE(c_0.size() == 3);
    REQUIRE(c_p50.size() == 3);

    // Test values at t=100
    REQUIRE(c_m50[0] == Catch::Approx(50.0)); // t=50: value=50
    REQUIRE(c_0[0] == Catch::Approx(100.0));  // t=100: value=100
    REQUIRE(c_p50[0] == Catch::Approx(150.0));// t=150: value=150

    // Test values at t=500 (peak)
    REQUIRE(c_m50[1] == Catch::Approx(450.0));// t=450: value=450
    REQUIRE(c_0[1] == Catch::Approx(500.0));  // t=500: value=500 (peak)
    REQUIRE(c_p50[1] == Catch::Approx(450.0));// t=550: value=1000-550=450

    // Test values at t=700 (falling edge)
    REQUIRE(c_m50[2] == Catch::Approx(350.0));// t=650: value=1000-650=350
    REQUIRE(c_0[2] == Catch::Approx(300.0));  // t=700: value=1000-700=300
    REQUIRE(c_p50[2] == Catch::Approx(250.0));// t=750: value=1000-750=250
}

TEST_CASE_METHOD(AnalogTestFixture, "DM - TV - AnalogTimestampOffsets cross-timeframe test", "[AnalogTimestampOffsetsMultiComputer][CrossTimeframe]") {
    auto & dm = getDataManager();

    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);

    // Test signal B (triangular wave in "time_10" timeframe)
    auto src_b = dme_ptr->getAnalogSource("B");
    REQUIRE(src_b != nullptr);

    // Use timestamps from "time" timeframe but query signal B which is in "time_10"
    // This tests cross-timeframe conversion
    std::vector<TimeFrameIndex> test_times = {
            TimeFrameIndex(100),// Should map to index 10 in time_10 frame
            TimeFrameIndex(500),// Should map to index 50 in time_10 frame
            TimeFrameIndex(700) // Should map to index 70 in time_10 frame
    };

    auto time_frame = dm.getTime(TimeKey("time"));
    auto selector = std::make_unique<TimestampSelector>(test_times, time_frame);

    std::vector<int> offsets = {-100, 0, 100};// Larger offsets to test cross-timeframe
    auto multi = std::make_unique<AnalogTimestampOffsetsMultiComputer>(src_b, "B", offsets);

    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(selector));
    builder.addColumns<double>("Boffs", std::move(multi));
    auto table = builder.build();

    // Verify column structure
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 3);

    auto const & c_m100 = table.getColumnValues<double>("Boffs.t-100");
    auto const & c_0 = table.getColumnValues<double>("Boffs.t+0");
    auto const & c_p100 = table.getColumnValues<double>("Boffs.t+100");

    REQUIRE(c_m100.size() == 3);
    REQUIRE(c_0.size() == 3);
    REQUIRE(c_p100.size() == 3);

    // Test values - signal B has same triangular pattern but sampled at time_10 resolution
    // At t=100 in time frame -> value should be 100
    REQUIRE(c_0[0] == Catch::Approx(100.0));

    // At t=500 in time frame -> value should be 500 (peak)
    REQUIRE(c_0[1] == Catch::Approx(500.0));

    // At t=700 in time frame -> value should be 300 (1000-700)
    REQUIRE(c_0[2] == Catch::Approx(300.0));
}


TEST_CASE_METHOD(AnalogTestFixture, "DM - TV - AnalogTimestampOffsets via registry", "[AnalogTimestampOffsetsMultiComputer][Registry]") {
    auto & dm = getDataManager();

    ComputerRegistry registry;
    auto dme_ptr = std::make_shared<DataManagerExtension>(dm);

    // Use signal A from the fixture (triangular wave in "time" timeframe)
    auto analog_source = dme_ptr->getAnalogSource("A");
    REQUIRE(analog_source != nullptr);

    // Test with offsets that will sample the triangular wave at different points
    std::map<std::string, std::string> params{{"offsets", "-100,0,100"}};
    auto multi = registry.createTypedMultiComputer<double>("Analog Timestamp Offsets", analog_source, params);
    REQUIRE(multi != nullptr);

    // Build a table with test timestamps at key points of the triangular wave
    std::vector<TimeFrameIndex> test_times = {
            TimeFrameIndex(200),// Rising edge: value=200
            TimeFrameIndex(500),// Peak: value=500
            TimeFrameIndex(800) // Falling edge: value=1000-800=200
    };

    auto time_frame = dm.getTime(TimeKey("time"));
    auto selector = std::make_unique<TimestampSelector>(test_times, time_frame);
    TableViewBuilder builder(dme_ptr);
    builder.setRowSelector(std::move(selector));
    builder.addColumns<double>("Aoffs", std::move(multi));
    auto table = builder.build();

    // Expect 3 outputs for the 3 offsets
    auto names = table.getColumnNames();
    REQUIRE(names.size() == 3);

    auto const & c_m100 = table.getColumnValues<double>("Aoffs.t-100");
    auto const & c_0 = table.getColumnValues<double>("Aoffs.t+0");
    auto const & c_p100 = table.getColumnValues<double>("Aoffs.t+100");

    REQUIRE(c_m100.size() == 3);
    REQUIRE(c_0.size() == 3);
    REQUIRE(c_p100.size() == 3);

    // Test values at t=200 (rising edge)
    REQUIRE(c_m100[0] == Catch::Approx(100.0));// t=100: value=100
    REQUIRE(c_0[0] == Catch::Approx(200.0));   // t=200: value=200
    REQUIRE(c_p100[0] == Catch::Approx(300.0));// t=300: value=300

    // Test values at t=500 (peak)
    REQUIRE(c_m100[1] == Catch::Approx(400.0));// t=400: value=400
    REQUIRE(c_0[1] == Catch::Approx(500.0));   // t=500: value=500 (peak)
    REQUIRE(c_p100[1] == Catch::Approx(400.0));// t=600: value=1000-600=400

    // Test values at t=800 (falling edge)
    REQUIRE(c_m100[2] == Catch::Approx(300.0));// t=700: value=1000-700=300
    REQUIRE(c_0[2] == Catch::Approx(200.0));   // t=800: value=1000-800=200
    REQUIRE(c_p100[2] == Catch::Approx(100.0));// t=900: value=1000-900=100
}