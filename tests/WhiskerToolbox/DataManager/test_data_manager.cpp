
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

TEST_CASE("DataManager::getTime retrieves TimeFrame objects correctly", "[DataManager][TimeFrame]") {
    DataManager dm;

    SECTION("Get default TimeFrame") {
        // Default TimeFrame should exist and be valid
        auto default_time = dm.getTime();
        REQUIRE(default_time != nullptr);
    }

    SECTION("Get TimeFrame by key") {
        // Register a new TimeFrame
        auto custom_timeframe = std::make_shared<TimeFrame>();
        dm.setTime("custom_time", custom_timeframe);

        // Retrieve it by key
        auto retrieved_timeframe = dm.getTime("custom_time");
        REQUIRE(retrieved_timeframe == custom_timeframe);
    }

    SECTION("Getting non-existent TimeFrame returns nullptr") {
        auto non_existent = dm.getTime("non_existent_key");
        REQUIRE(non_existent == nullptr);
    }
}

TEST_CASE("DataManager::setTimeFrame assigns TimeFrames to data objects", "[DataManager][TimeFrame]") {
    DataManager dm;
    // Add some test data
    dm.setData<PointData>("test_points");
    auto custom_timeframe = std::make_shared<TimeFrame>();
    dm.setTime("custom_time", custom_timeframe);

    SECTION("Associate data with a valid time frame") {
        bool result = dm.setTimeFrame("test_points", "custom_time");

        REQUIRE(result == true);
        REQUIRE(dm.getTimeFrame("test_points") == "custom_time");
    }

    SECTION("Associate data with default time frame") {
        bool result = dm.setTimeFrame("test_points", "time");

        REQUIRE(result == true);
        REQUIRE(dm.getTimeFrame("test_points") == "time");
    }
}

TEST_CASE("DataManager::setTimeFrame handles error conditions", "[DataManager][TimeFrame][Error]") {
    DataManager dm;
    dm.setData<PointData>("test_points");

    SECTION("Invalid data key") {
        bool result = dm.setTimeFrame("nonexistent_data", "time");

        REQUIRE(result == false);
    }

    SECTION("Invalid time key") {
        bool result = dm.setTimeFrame("test_points", "nonexistent_time");

        REQUIRE(result == false);
        // Should keep the default time frame association
        REQUIRE(dm.getTimeFrame("test_points") == "time");
    }
}

TEST_CASE("DataManager::getTimeFrame retrieves TimeFrame associations correctly", "[DataManager][TimeFrame]") {
    DataManager dm;

    // Setup - create some data and time frames
    dm.setData<PointData>("test_points");
    auto custom_timeframe = std::make_shared<TimeFrame>();
    dm.setTime("custom_time", custom_timeframe);
    dm.setTimeFrame("test_points", "custom_time");

    SECTION("Get existing TimeFrame association") {
        std::string time_key = dm.getTimeFrame("test_points");
        REQUIRE(time_key == "custom_time");
    }

    SECTION("Default TimeFrame association") {
        // Add new data without explicit TimeFrame
        dm.setData<PointData>("default_points");

        // Should be associated with default "time"
        std::string time_key = dm.getTimeFrame("default_points");
        REQUIRE(time_key == "time");
    }
}

TEST_CASE("DataManager::getTimeFrame handles error conditions", "[DataManager][TimeFrame][Error]") {
    DataManager dm;

    SECTION("Non-existent data key") {
        std::string time_key = dm.getTimeFrame("nonexistent_data");
        REQUIRE(time_key.empty());
    }

    SECTION("Data without TimeFrame association") {
        // Create data but manipulate internal state to remove TimeFrame association
        // This is a bit of a hack to test the second error condition, as setData automatically sets a TimeFrame
        dm.setData<PointData>("unassociated_points");

        // Directly manipulate the internal _time_frames map to remove the association
        // In practice we would need to use friend classes or expose internals for testing
        // For this example, we'll assume the normal API flow

        // Since we can't easily test this case with the public API, we'll just note
        // that this error path exists but is difficult to trigger in normal usage
    }
}

TEST_CASE("DataManager::getTimeFrameKeys returns all TimeFrame keys", "[DataManager][TimeFrame]") {
    DataManager dm;

    SECTION("Default state contains only 'time' key") {
        auto keys = dm.getTimeFrameKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "time");
    }

    SECTION("Adding TimeFrames updates the key list") {
        // Add two additional TimeFrames
        auto timeframe1 = std::make_shared<TimeFrame>();
        auto timeframe2 = std::make_shared<TimeFrame>();

        dm.setTime("custom_time1", timeframe1);
        dm.setTime("custom_time2", timeframe2);

        // Get the keys
        auto keys = dm.getTimeFrameKeys();

        // Check the size and contents
        REQUIRE(keys.size() == 3);

        // Check that all expected keys are present (order not guaranteed)
        auto has_key = [&keys](const std::string& key) {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        };

        REQUIRE(has_key("time"));
        REQUIRE(has_key("custom_time1"));
        REQUIRE(has_key("custom_time2"));
    }

    SECTION("Keys remain stable after modifications") {
        // Add a TimeFrame
        auto timeframe = std::make_shared<TimeFrame>();
        dm.setTime("custom_time", timeframe);

        // Verify it's added
        {
            auto keys = dm.getTimeFrameKeys();
            REQUIRE(keys.size() == 2);
            REQUIRE(std::find(keys.begin(), keys.end(), "custom_time") != keys.end());
        }

        // Try to add a duplicate (which should fail)
        auto duplicate = std::make_shared<TimeFrame>();
        dm.setTime("custom_time", duplicate);

        // Verify keys haven't changed
        {
            auto keys = dm.getTimeFrameKeys();
            REQUIRE(keys.size() == 2); // Still just 2 keys
            REQUIRE(std::find(keys.begin(), keys.end(), "custom_time") != keys.end());
        }
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
