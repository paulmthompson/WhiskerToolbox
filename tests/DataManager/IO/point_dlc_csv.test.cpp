#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "DataManager.hpp"
#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"
#include "Points/IO/JSON/Point_Data_JSON.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

class DLCPointCSVTestFixture {
public:
    DLCPointCSVTestFixture() {
        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "dlc_point_csv_test";
        std::filesystem::create_directories(test_dir);
        
        // Copy test file to temporary location for testing
        test_csv_path = test_dir / "dlc_test.csv";
        original_csv_path = std::filesystem::path(__FILE__).parent_path() / "../data/Points/dlc_test.csv";
        
        if (std::filesystem::exists(original_csv_path)) {
            std::filesystem::copy_file(original_csv_path, test_csv_path);
        }
    }

    ~DLCPointCSVTestFixture() {
        // Cleanup
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }

protected:
    std::filesystem::path test_dir;
    std::filesystem::path test_csv_path;
    std::filesystem::path original_csv_path;
    
    const char* getJSONConfig() const {
        return R"([
            {
                "filepath": "dlc_test.csv",
                "data_type": "points",
                "name": "face_points",
                "format": "dlc_csv",
                "frame_column": 0,
                "likelihood_threshold": 0.5
            }
        ])";
    }
    
    const char* getJSONConfigLowThreshold() const {
        return R"([
            {
                "filepath": "dlc_test.csv",
                "data_type": "points",
                "name": "face_points",
                "format": "dlc_csv",
                "frame_column": 0,
                "likelihood_threshold": 0.0
            }
        ])";
    }
    
    // Expected bodyparts from the test CSV file
    std::vector<std::string> getExpectedBodyparts() const {
        return {"wp_post_left", "wp_cent_left", "wp_ant_left", "nose_left", "nose_tip", 
                "nose_right", "wp_ant_right", "wp_cent_right", "wp_p_right", "cuetip"};
    }
};

TEST_CASE_METHOD(DLCPointCSVTestFixture, "DLC CSV - Direct loader function test", "[DLC][CSV][Points]") {
    
    SECTION("Load DLC CSV with default threshold") {
        DLCPointLoaderOptions opts;
        opts.filename = test_csv_path.string();
        opts.frame_column = 0;
        opts.likelihood_threshold = 0.0f;
        
        auto result = load_dlc_csv(opts);
        
        // Check that we loaded the expected bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size());
        
        for (const auto& bodypart : expected_bodyparts) {
            REQUIRE(result.find(bodypart) != result.end());
        }
        
        // Check that each bodypart has 5 frames of data (frames 0-4)
        for (const auto& [bodypart, points] : result) {
            REQUIRE(points.size() == 5); // 5 frames in test data
            
            // Verify frame indices
            for (int i = 0; i < 5; ++i) {
                REQUIRE(points.find(TimeFrameIndex(i)) != points.end());
            }
        }
    }
    
    SECTION("Load DLC CSV with likelihood threshold") {
        DLCPointLoaderOptions opts;
        opts.filename = test_csv_path.string();
        opts.frame_column = 0;

        // High threshold this eliminates all points from cuetip and wp_post_right
        opts.likelihood_threshold = 0.8f;
        
        auto result = load_dlc_csv(opts);
        
        // Should still have all bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size() - 2);
        
        // But some points should be filtered out due to low likelihood
        // Check specific cases based on test data
        for (const auto& [bodypart, points] : result) {
            // Each bodypart should have fewer points due to threshold filtering
            INFO("Bodypart: " << bodypart << " has " << points.size() << " points");
        }
    }
}

TEST_CASE_METHOD(DLCPointCSVTestFixture, "DLC CSV - JSON loader test", "[DLC][CSV][Points][JSON]") {
    
    SECTION("Load multiple PointData from DLC CSV") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv",
            "frame_column": 0,
            "likelihood_threshold": 0.0
        })");
        
        auto result = load_multiple_PointData_from_dlc(test_csv_path.string(), config);
        
        // Check that we got PointData objects for all bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size()); // cuetip filtered out by threshold
        
        for (const auto& bodypart : expected_bodyparts) {
            REQUIRE(result.find(bodypart) != result.end());
            REQUIRE(result.at(bodypart) != nullptr);
            
            // Check that each PointData has some data
            auto point_data = result.at(bodypart);
            // Note: We can't easily check the exact number of points without
            // knowing the specific likelihood values in the test data
        }
    }
    
    SECTION("Load single PointData from DLC CSV (backward compatibility)") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv",
            "frame_column": 0,
            "likelihood_threshold": 0.1
        })");
        
        auto result = load_into_PointData(test_csv_path.string(), config);
        
        REQUIRE(result != nullptr);
        // The function should return the first bodypart's data
    }
}

TEST_CASE_METHOD(DLCPointCSVTestFixture, "DLC CSV - DataManager JSON config test", "[DLC][CSV][Points][DataManager]") {
    
    SECTION("Load DLC CSV through DataManager with standard threshold") {
        // Create JSON config file for loading
        std::string json_config = getJSONConfig();
        std::filesystem::path json_filepath = test_dir / "config.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager and load data using JSON config
        auto data_manager = std::make_unique<DataManager>();
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Verify that data was loaded successfully
        // Each bodypart should create a separate entry in data_info_list
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(data_info_list.size() == expected_bodyparts.size() - 1); // cuetip filtered out by threshold

        auto point_keys = data_manager->getKeys<PointData>();
        REQUIRE(point_keys.size() == expected_bodyparts.size() - 1); // cuetip filtered out by threshold
        
       
    }
    
    SECTION("Load DLC CSV with low likelihood threshold") {
        // Create JSON config file with low threshold
        std::string json_config = getJSONConfigLowThreshold();
        std::filesystem::path json_filepath = test_dir / "config_low_threshold.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager and load data using JSON config
        auto data_manager = std::make_unique<DataManager>();
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Should still load all bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(data_info_list.size() == expected_bodyparts.size());
        
        // Verify specific point data
        std::string nose_tip_name = "face_points_nose_tip";
        auto nose_tip_data = data_manager->getData<PointData>(nose_tip_name);
        REQUIRE(nose_tip_data != nullptr);
        
        // Check that we have data at frame 0
        auto points_at_frame_0 = nose_tip_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(!points_at_frame_0.empty());
        
        // The nose_tip at frame 0 should be approximately (363.8144531, 272.2839661)
        // based on the test CSV data
        auto& first_point = points_at_frame_0[0];
        REQUIRE(first_point.x == Catch::Approx(363.8144531f).margin(0.1f));
        REQUIRE(first_point.y == Catch::Approx(272.2839661f).margin(0.1f));
    }
}

TEST_CASE_METHOD(DLCPointCSVTestFixture, "DLC CSV - Error handling", "[DLC][CSV][Points][Error]") {
    
    SECTION("Handle missing file gracefully") {
        DLCPointLoaderOptions opts;
        opts.filename = "non_existent_file.csv";
        opts.frame_column = 0;
        opts.likelihood_threshold = 0.0f;
        
        auto result = load_dlc_csv(opts);
        REQUIRE(result.empty());
    }
    
    SECTION("Handle invalid JSON config") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv"
        })");
        
        // Missing required fields, should use defaults
        auto result = load_multiple_PointData_from_dlc(test_csv_path.string(), config);
        
        // Should still work with default values
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size());
    }
}
