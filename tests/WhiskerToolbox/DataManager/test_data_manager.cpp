
#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"

TEST_CASE("DataManager - Create", "[DataManager]") {

    auto dm = DataManager();

    REQUIRE(dm.getPointKeys().size() == 0);
}

TEST_CASE("DataManager - Load Media", "[DataManager]") {

auto dm = DataManager();

auto filename = "data/Media/test_each_frame_number.mp4";

dm.loadMedia(filename);

auto media = dm.getMediaData();

REQUIRE(media->getHeight() == 480);
REQUIRE(media->getWidth() == 640);
}
