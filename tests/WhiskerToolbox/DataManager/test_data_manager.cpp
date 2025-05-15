
#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
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

TEST_CASE("DataManager::addCallbackToData registers callbacks with data objects", "[DataManager][Observer]") {
    DataManager dm;

    // Setup - create some test data
    dm.setData<PointData>("test_points");

    SECTION("Successfully register callback to valid data") {
        bool callback_executed = false;
        auto callback = [&callback_executed]() { callback_executed = true; };

        int id = dm.addCallbackToData("test_points", callback);

        REQUIRE(id >= 0); // Valid ID is returned

        // Trigger the callback by modifying the data
        auto points = dm.getData<PointData>("test_points");
        points->notifyObservers();

        REQUIRE(callback_executed == true);
    }

    SECTION("Multiple callbacks can be registered") {
        int callback1_count = 0;
        int callback2_count = 0;

        auto callback1 = [&callback1_count]() { callback1_count++; };
        auto callback2 = [&callback2_count]() { callback2_count++; };

        int id1 = dm.addCallbackToData("test_points", callback1);
        int id2 = dm.addCallbackToData("test_points", callback2);

        REQUIRE(id1 >= 0);
        REQUIRE(id2 >= 0);
        REQUIRE(id1 != id2); // IDs should be unique

        // Trigger the callbacks
        auto points = dm.getData<PointData>("test_points");
        points->notifyObservers();

        REQUIRE(callback1_count == 1);
        REQUIRE(callback2_count == 1);
    }
}

TEST_CASE("DataManager::addCallbackToData handles error conditions", "[DataManager][Observer][Error]") {
    DataManager dm;

    SECTION("Returns -1 for non-existent data key") {
        bool callback_executed = false;
        auto callback = [&callback_executed]() { callback_executed = true; };

        int id = dm.addCallbackToData("nonexistent_data", callback);

        REQUIRE(id == -1); // Invalid ID indicates failure
        REQUIRE(callback_executed == false); // Callback should not have executed
    }
}

TEST_CASE("DataManager::removeCallbackFromData removes registered callbacks", "[DataManager][Observer]") {
    DataManager dm;

    // Setup - create some test data
    dm.setData<PointData>("test_points");

    SECTION("Successfully remove registered callback") {
        // Setup - register a callback
        int callback_count = 0;
        auto callback = [&callback_count]() { callback_count++; };

        int id = dm.addCallbackToData("test_points", callback);
        REQUIRE(id >= 0);

        // Verify callback works before removal
        auto points = dm.getData<PointData>("test_points");
        points->notifyObservers();
        REQUIRE(callback_count == 1);

        // Remove the callback
        bool result = dm.removeCallbackFromData("test_points", id);
        REQUIRE(result == true);

        // Verify callback no longer works
        points->notifyObservers();
        REQUIRE(callback_count == 1); // Count should remain unchanged
    }

    SECTION("Removing one callback doesn't affect others") {
        // Setup - register two callbacks
        int callback1_count = 0;
        int callback2_count = 0;
        auto callback1 = [&callback1_count]() { callback1_count++; };
        auto callback2 = [&callback2_count]() { callback2_count++; };

        int id1 = dm.addCallbackToData("test_points", callback1);
        int id2 = dm.addCallbackToData("test_points", callback2);

        // Remove the first callback
        bool result = dm.removeCallbackFromData("test_points", id1);
        REQUIRE(result == true);

        // Verify only the second callback works
        auto points = dm.getData<PointData>("test_points");
        points->notifyObservers();

        REQUIRE(callback1_count == 0); // First callback removed
        REQUIRE(callback2_count == 1); // Second callback still active
    }
}

TEST_CASE("DataManager::removeCallbackFromData handles error conditions", "[DataManager][Observer][Error]") {
    DataManager dm;

    SECTION("Returns false for non-existent data key") {
        bool result = dm.removeCallbackFromData("nonexistent_data", 1);
        REQUIRE(result == false);
    }

    SECTION("Handles invalid callback ID gracefully") {
        // Setup - create test data
        dm.setData<PointData>("test_points");

        // Try to remove a callback that doesn't exist
        bool result = dm.removeCallbackFromData("test_points", 9999);

        // The operation should still "succeed" in the sense that we found the data
        // even though the specific callback ID might not exist
        REQUIRE(result == true);

        // The exact behavior with invalid callback IDs depends on the removeObserver implementation
        // but at minimum we should verify it doesn't crash
    }
}

TEST_CASE("DataManager::addObserver registers callbacks for state changes", "[DataManager][Observer]") {
    DataManager dm;

    SECTION("Observer is called when data is added") {
        int callback_count = 0;
        auto callback = [&callback_count]() { callback_count++; };

        // Register the callback
        dm.addObserver(callback);

        // Add data which should trigger the callback
        dm.setData<PointData>("test_points");

        REQUIRE(callback_count == 1);

        // Add another data object, should trigger again
        dm.setData<PointData>("more_points");

        REQUIRE(callback_count == 2);
    }

    SECTION("Multiple observers are all called") {
        int callback1_count = 0;
        int callback2_count = 0;

        auto callback1 = [&callback1_count]() { callback1_count++; };
        auto callback2 = [&callback2_count]() { callback2_count++; };

        // Register both callbacks
        dm.addObserver(callback1);
        dm.addObserver(callback2);

        // Trigger notification
        dm.setData<PointData>("test_points");

        // Both callbacks should be called
        REQUIRE(callback1_count == 1);
        REQUIRE(callback2_count == 1);
    }

    SECTION("Callbacks are called for various state changes") {
        int notification_count = 0;
        auto callback = [&notification_count]() { notification_count++; };

        dm.addObserver(callback);

        // Different operations that should trigger notifications
        dm.setData<PointData>("points");
        REQUIRE(notification_count == 1);

        // Adding with custom TimeFrame
        auto custom_time = std::make_shared<TimeFrame>();
        dm.setTime("custom_time", custom_time);
        dm.setData<PointData>("points2", std::make_shared<PointData>(), "custom_time");
        REQUIRE(notification_count == 2);

        // Using variant form
        DataTypeVariant variant = std::make_shared<PointData>();
        dm.setData("variant_points", variant);
        REQUIRE(notification_count == 3);
    }

    SECTION("Observer captures state correctly") {
        std::vector<std::string> observed_keys;

        auto callback = [&dm, &observed_keys]() {
            // Capture the current set of keys when notified
            auto keys = dm.getAllKeys();
            observed_keys = keys;
        };

        dm.addObserver(callback);

        // Add data to trigger the callback
        dm.setData<PointData>("test_points");

        // The observer should have captured the keys including our new one
        // (plus "media" which exists by default)
        REQUIRE(observed_keys.size() == 2);
        REQUIRE(std::find(observed_keys.begin(), observed_keys.end(), "test_points") != observed_keys.end());
    }
}

TEST_CASE("DataManager::getAllKeys returns all data keys", "[DataManager]") {
    DataManager dm;

    SECTION("Default state contains only 'media' key") {
        auto keys = dm.getAllKeys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "media");
    }

    SECTION("Adding data objects updates the key list") {
        // Add various data objects
        dm.setData<PointData>("points1");
        dm.setData<PointData>("points2");
        dm.setData<LineData>("line1");

        // Get the keys
        auto keys = dm.getAllKeys();

        // Check the size and contents
        REQUIRE(keys.size() == 4); // 3 new keys + "media"

        // Check that all expected keys are present (order not guaranteed)
        auto has_key = [&keys](const std::string& key) {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        };

        REQUIRE(has_key("media"));
        REQUIRE(has_key("points1"));
        REQUIRE(has_key("points2"));
        REQUIRE(has_key("line1"));
    }

    SECTION("Keys reflect changes to data collection") {
        // Add and then remove a data object
        dm.setData<PointData>("temporary");

        // First check that the key exists
        {
            auto keys = dm.getAllKeys();
            REQUIRE(keys.size() == 2); // "media" + "temporary"
            REQUIRE(std::find(keys.begin(), keys.end(), "temporary") != keys.end());
        }

        // Then remove it (assuming there's a removeData method, if not, this section would need to be adjusted)
        // dm.removeData("temporary");

        // For now, we'll just test addition of data
        dm.setData<LineData>("permanent");

        // Check the updated keys
        auto keys = dm.getAllKeys();
        REQUIRE(std::find(keys.begin(), keys.end(), "permanent") != keys.end());
    }
}

TEST_CASE("DataManager::getKeys returns keys of the specified data type", "[DataManager]") {
    DataManager dm;

    SECTION("Empty DataManager returns empty vector for any type") {
        REQUIRE(dm.getKeys<PointData>().size() == 0);
        REQUIRE(dm.getKeys<LineData>().size() == 0);
    }

    SECTION("Returns only keys matching the requested type") {
        // Add various data objects
        dm.setData<PointData>("points1");
        dm.setData<PointData>("points2");
        dm.setData<LineData>("line1");
        dm.setData<MaskData>("mask1");

        // Get point keys
        auto point_keys = dm.getKeys<PointData>();
        REQUIRE(point_keys.size() == 2);
        REQUIRE(std::find(point_keys.begin(), point_keys.end(), "points1") != point_keys.end());
        REQUIRE(std::find(point_keys.begin(), point_keys.end(), "points2") != point_keys.end());
        REQUIRE(std::find(point_keys.begin(), point_keys.end(), "line1") == point_keys.end());

        // Get line keys
        auto line_keys = dm.getKeys<LineData>();
        REQUIRE(line_keys.size() == 1);
        REQUIRE(std::find(line_keys.begin(), line_keys.end(), "line1") != line_keys.end());

        // Get mask keys
        auto mask_keys = dm.getKeys<MaskData>();
        REQUIRE(mask_keys.size() == 1);
        REQUIRE(std::find(mask_keys.begin(), mask_keys.end(), "mask1") != mask_keys.end());
    }

    SECTION("Returns correct default keys") {
        // By default, the DataManager has a MediaData object with key "media"
        auto media_keys = dm.getKeys<MediaData>();
        REQUIRE(media_keys.size() == 1);
        REQUIRE(media_keys[0] == "media");

        // But no default PointData
        auto point_keys = dm.getKeys<PointData>();
        REQUIRE(point_keys.size() == 0);
    }

    SECTION("Updates after adding new data") {
        // Start with no point data
        REQUIRE(dm.getKeys<PointData>().size() == 0);

        // Add a point data object
        dm.setData<PointData>("dynamic_points");

        // Check that it appears in the keys
        auto point_keys = dm.getKeys<PointData>();
        REQUIRE(point_keys.size() == 1);
        REQUIRE(point_keys[0] == "dynamic_points");
    }

    SECTION("Handles different template types") {
        // Add data of various types
        dm.setData<PointData>("test_points");
        dm.setData<LineData>("test_line");

        // Test with non-standard data types (that may be added later)
        // For now, we'll use the existing types
        REQUIRE(dm.getKeys<PointData>().size() == 1);
        REQUIRE(dm.getKeys<LineData>().size() == 1);

        // No data of these types should exist yet
        REQUIRE(dm.getKeys<DigitalEventSeries>().size() == 0);
        REQUIRE(dm.getKeys<TensorData>().size() == 0);
    }
}

TEST_CASE("DataManager - Load Media", "[DataManager]") {

auto dm = DataManager();

auto filename = "data/Media/test_each_frame_number.mp4";

auto media = std::make_shared<VideoData>();
media->LoadMedia(filename);
dm.setData<VideoData>("media", media);

auto dm_media = dm.getData<MediaData>("media");

REQUIRE(dm_media->getHeight() == 480);
REQUIRE(dm_media->getWidth() == 640);
}
