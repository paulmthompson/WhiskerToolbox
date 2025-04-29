
#include <catch2/catch_test_macros.hpp>

#include "AnalogTimeSeries/Analog_Time_Series_Loader.hpp"

TEST_CASE("Analog - Load CSV", "[DataManager]") {

auto filename = "data/Analog/single_column.csv";

auto data = load_analog_series_from_csv(filename);

REQUIRE(data.size() == 6);

REQUIRE(data[0] == 1.0);

REQUIRE(data[5] == 6.0);
}
