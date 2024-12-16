
#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"

TEST_CASE("DataManager - Create", "[DataManager]") {

    auto dm = DataManager();

    REQUIRE(dm.getKeys<PointData>().size() == 0);
}

TEST_CASE("DataManager - Load Media", "[DataManager]") {

auto dm = DataManager();

auto filename = "data/Media/test_each_frame_number.mp4";

auto media = std::make_shared<VideoData>();
media->LoadMedia(filename);
dm.setMedia(media);

auto dm_media = dm.getData<MediaData>("media");

REQUIRE(dm_media->getHeight() == 480);
REQUIRE(dm_media->getWidth() == 640);
}
