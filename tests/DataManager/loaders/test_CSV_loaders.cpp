

#include <catch2/catch_test_macros.hpp>

#include "loaders/CSV_Loaders.hpp"

TEST_CASE("Loaders - Load CSV", "[DataManager]") {

auto filename = "data/Analog/single_column.csv";

auto data = CSVLoader::loadSingleColumnCSV(filename);

REQUIRE(data.size() == 6);

REQUIRE(data[0] == 1.0);

REQUIRE(data[5] == 6.0);
}
