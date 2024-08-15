
#include <catch2/catch_test_macros.hpp>

#include "DataManager/DataManager.hpp"

TEST_CASE("DataManager - Create", "[DataManager]") {

    auto dm = DataManager();

    REQUIRE(dm.getPointKeys().size() == 0);
}
