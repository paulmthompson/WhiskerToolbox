#include <catch2/catch_test_macros.hpp>

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"
#include "Media/Image_Data.hpp"
#include "Points/Point_Data.hpp"

TEST_CASE("DataManager - Load Media VideoData", "[DataManager][Video]") {
    auto dm = DataManager();

    SECTION("Load VideoData") {
        dm.setData<VideoData>("my_video", TimeKey("time"));
        //REQUIRE(dm.getKeys<VideoData>().size() == 1);
        //REQUIRE(dm.getKeys<VideoData>()[0] == "my_video");
        REQUIRE(dm.getKeys<MediaData>().size() == 1);
        REQUIRE(dm.getKeys<MediaData>()[0] == "my_video");
    }
}

TEST_CASE("DataManager::getType VideoData tests", "[DataManager][Video]") {
    DataManager dm;

    SECTION("VideoData type identification") {
        auto video_data = std::make_shared<VideoData>();
        dm.setData<VideoData>("test_video", video_data, TimeKey("time"));
        
        // Should be correctly identified as Video
        REQUIRE(dm.getType("test_video") == DM_DataType::Video);
        
        // Should not be identified as other types
        REQUIRE(dm.getType("test_video") != DM_DataType::Images);
        REQUIRE(dm.getType("test_video") != DM_DataType::Points);
        REQUIRE(dm.getType("test_video") != DM_DataType::Line);
        REQUIRE(dm.getType("test_video") != DM_DataType::Mask);
        REQUIRE(dm.getType("test_video") != DM_DataType::Unknown);
    }

    SECTION("Multiple media types coexisting") {
        // Test both VideoData and ImageData in the same DataManager
        auto video_data = std::make_shared<VideoData>();
        auto image_data = std::make_shared<ImageData>();
        
        dm.setData<VideoData>("my_video", video_data, TimeKey("time"));
        dm.setData<ImageData>("my_images", image_data, TimeKey("time"));

        // Each should be correctly identified
        REQUIRE(dm.getType("my_video") == DM_DataType::Video);
        REQUIRE(dm.getType("my_images") == DM_DataType::Images);
        
        // Cross-check that they don't interfere with each other
        REQUIRE(dm.getType("my_video") != dm.getType("my_images"));
    }

    SECTION("Media type inheritance and polymorphism") {
        // Test that both VideoData and ImageData are correctly handled
        // even though they inherit from MediaData
        
        auto video_data = std::make_shared<VideoData>();
        auto image_data = std::make_shared<ImageData>();
        
        // Store them as their base class (MediaData) to test polymorphism
        dm.setData<MediaData>("poly_video", video_data, TimeKey("time"));
        dm.setData<MediaData>("poly_images", image_data, TimeKey("time"));
        
        // getType should still correctly identify the derived types via dynamic_cast
        REQUIRE(dm.getType("poly_video") == DM_DataType::Video);
        REQUIRE(dm.getType("poly_images") == DM_DataType::Images);
    }

    SECTION("Replacing data updates type correctly") {
        // Start with one type
        auto video_data = std::make_shared<VideoData>();
        dm.setData<VideoData>("replaceable", video_data, TimeKey("time"));
        REQUIRE(dm.getType("replaceable") == DM_DataType::Video);
        
        // Replace with different type
        auto image_data = std::make_shared<ImageData>();
        dm.setData<ImageData>("replaceable", image_data, TimeKey("time"));
        REQUIRE(dm.getType("replaceable") == DM_DataType::Images);
        
        // Replace with completely different type
        dm.setData<PointData>("replaceable", TimeKey("time"));
        REQUIRE(dm.getType("replaceable") == DM_DataType::Points);
    }
}
