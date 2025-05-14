
#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Points/Point_Data.hpp"

TEST_CASE("DataManager - Create", "[DataManager]") {

    auto dm = DataManager();

    REQUIRE(dm.getKeys<PointData>().size() == 0);
}

TEST_CASE("DataManager::setTime successfully registers TimeFrame objects", "[DataManager][TimeFrame]") {
    DataManager dm;

    SECTION("Register a new TimeFrame with a unique key") {
        auto timeframe = std::make_shared<TimeFrame>();
        bool result = dm.setTime("test_time", timeframe);

        REQUIRE(result == true);
        REQUIRE(dm.getTime("test_time") == timeframe);
        REQUIRE(dm.getTimeFrameKeys().size() == 2); // "time" exists by default + our new one
    }

    SECTION("Register multiple TimeFrames with different keys") {
        auto timeframe1 = std::make_shared<TimeFrame>();
        auto timeframe2 = std::make_shared<TimeFrame>();

        bool result1 = dm.setTime("time1", timeframe1);
        bool result2 = dm.setTime("time2", timeframe2);

        REQUIRE(result1 == true);
        REQUIRE(result2 == true);
        REQUIRE(dm.getTime("time1") == timeframe1);
        REQUIRE(dm.getTime("time2") == timeframe2);
        REQUIRE(dm.getTimeFrameKeys().size() == 3); // "time" exists by default + our 2 new ones
    }
}

TEST_CASE("DataManager::setTime handles error conditions", "[DataManager][TimeFrame][Error]") {
    DataManager dm;

    SECTION("Reject nullptr TimeFrame") {
        std::shared_ptr<TimeFrame> null_timeframe = nullptr;
        bool result = dm.setTime("null_time", null_timeframe);

        REQUIRE(result == false);
        REQUIRE(dm.getTime("null_time") == nullptr);
        REQUIRE(dm.getTimeFrameKeys().size() == 1); // Only "time" exists by default
    }

    SECTION("Reject duplicate key") {
        auto timeframe1 = std::make_shared<TimeFrame>();
        auto timeframe2 = std::make_shared<TimeFrame>();

        bool result1 = dm.setTime("duplicate", timeframe1);
        bool result2 = dm.setTime("duplicate", timeframe2);

        REQUIRE(result1 == true);
        REQUIRE(result2 == false);
        REQUIRE(dm.getTime("duplicate") == timeframe1); // First one should remain
        REQUIRE(dm.getTimeFrameKeys().size() == 2); // Only "time" and "duplicate" exist
    }
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
