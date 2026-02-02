/**
 * @file point_csv_unit.test.cpp
 * @brief Unit tests for Point CSV/DLC loading functions
 * 
 * Tests the underlying loader functions directly:
 * - load_dlc_csv() with DLCPointLoaderOptions
 * - load_multiple_PointData_from_dlc() JSON interface
 * - Error handling for edge cases
 * 
 * These tests use the actual dlc_test.csv fixture file to verify
 * correct parsing of real DeepLabCut output format.
 * 
 * For integration tests through DataManager JSON config, see
 * point_csv_integration.test.cpp
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Points/Point_Data.hpp"
#include "Points/IO/CSV/Point_Data_CSV.hpp"
#include "Points/IO/JSON/Point_Data_JSON.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

/**
 * @brief Test fixture for DLC CSV unit tests
 * 
 * Uses the actual dlc_test.csv fixture file which contains real
 * DeepLabCut output format with 10 bodyparts and 5 frames.
 */
class DLCPointCSVUnitTestFixture {
public:
    DLCPointCSVUnitTestFixture() {
        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "dlc_point_csv_unit_test";
        std::filesystem::create_directories(test_dir);
        
        // Copy test file to temporary location for testing
        test_csv_path = test_dir / "dlc_test.csv";
        original_csv_path = std::filesystem::path(__FILE__).parent_path() / "../../../../data/Points/dlc_test.csv";
        
        if (std::filesystem::exists(original_csv_path)) {
            std::filesystem::copy_file(original_csv_path, test_csv_path, 
                                       std::filesystem::copy_options::overwrite_existing);
        }
    }

    ~DLCPointCSVUnitTestFixture() {
        // Cleanup
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }

protected:
    std::filesystem::path test_dir;
    std::filesystem::path test_csv_path;
    std::filesystem::path original_csv_path;
    
    // Expected bodyparts from the dlc_test.csv fixture file
    std::vector<std::string> getExpectedBodyparts() const {
        return {"wp_post_left", "wp_cent_left", "wp_ant_left", "nose_left", "nose_tip", 
                "nose_right", "wp_ant_right", "wp_cent_right", "wp_p_right", "cuetip"};
    }
};

//=============================================================================
// Unit Tests: load_dlc_csv() function
//=============================================================================

TEST_CASE_METHOD(DLCPointCSVUnitTestFixture, 
                 "load_dlc_csv - Direct loader function", 
                 "[DLC][CSV][Points][unit]") {
    
    REQUIRE(std::filesystem::exists(test_csv_path));
    
    SECTION("Load with default threshold returns all bodyparts") {
        DLCPointLoaderOptions opts;
        opts.filepath = test_csv_path.string();
        
        auto result = load_dlc_csv(opts);
        
        // Check that we loaded the expected bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size());
        
        for (auto const& bodypart : expected_bodyparts) {
            REQUIRE(result.find(bodypart) != result.end());
        }
        
        // Check that each bodypart has 5 frames of data (frames 0-4)
        for (auto const& [bodypart, points] : result) {
            REQUIRE(points.size() == 5);
            
            // Verify frame indices
            for (int i = 0; i < 5; ++i) {
                REQUIRE(points.find(TimeFrameIndex(i)) != points.end());
            }
        }
    }
    
    SECTION("Load with high likelihood threshold filters bodyparts") {
        DLCPointLoaderOptions opts;
        opts.filepath = test_csv_path.string();
        opts.likelihood_threshold = 0.9f;  // High threshold
        
        auto result = load_dlc_csv(opts);
        
        // With 0.9 threshold, wp_post_left, wp_p_right, and cuetip should be filtered
        // wp_post_left: all frames ~0.0003 (well below 0.9)
        // wp_p_right: all frames 0.65-0.74 (below 0.9)
        // cuetip: all frames ~0.0015 (well below 0.9)
        auto expected_bodyparts = getExpectedBodyparts();
        
        // Should filter 3 bodyparts, leaving 7
        REQUIRE(result.size() == expected_bodyparts.size() - 3);
        
        // Verify filtered bodyparts are not present
        REQUIRE(result.find("wp_post_left") == result.end());
        REQUIRE(result.find("cuetip") == result.end());
        REQUIRE(result.find("wp_p_right") == result.end());
        
        // wp_cent_left should be present but with only 1 point (frame 1 has likelihood 0.911 > 0.9)
        REQUIRE(result.find("wp_cent_left") != result.end());
        REQUIRE(result.at("wp_cent_left").size() == 1);
    }
    
    SECTION("Verify specific point coordinates from fixture") {
        DLCPointLoaderOptions opts;
        opts.filepath = test_csv_path.string();
        opts.likelihood_threshold = 0.0f;
        
        auto result = load_dlc_csv(opts);
        
        // Verify nose_tip at frame 0 has expected coordinates
        REQUIRE(result.find("nose_tip") != result.end());
        auto& nose_tip_points = result.at("nose_tip");
        REQUIRE(nose_tip_points.find(TimeFrameIndex(0)) != nose_tip_points.end());
        
        auto& point = nose_tip_points.at(TimeFrameIndex(0));
        REQUIRE(point.x == Catch::Approx(363.8144531f).margin(0.1f));
        REQUIRE(point.y == Catch::Approx(272.2839661f).margin(0.1f));
    }
}

//=============================================================================
// Unit Tests: load_multiple_PointData_from_dlc() JSON interface
//=============================================================================

TEST_CASE_METHOD(DLCPointCSVUnitTestFixture, 
                 "load_multiple_PointData_from_dlc - JSON interface", 
                 "[DLC][CSV][Points][JSON][unit]") {
    
    REQUIRE(std::filesystem::exists(test_csv_path));
    
    SECTION("Load all bodyparts with zero threshold") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv",
            "frame_column": 0,
            "likelihood_threshold": 0.0
        })");
        
        auto result = load_multiple_PointData_from_dlc(test_csv_path.string(), config);
        
        // Check that we got PointData objects for all bodyparts
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size());
        
        for (auto const& bodypart : expected_bodyparts) {
            REQUIRE(result.find(bodypart) != result.end());
            REQUIRE(result.at(bodypart) != nullptr);
        }
    }
    
    SECTION("Returns shared_ptr<PointData> objects") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv",
            "likelihood_threshold": 0.0
        })");
        
        auto result = load_multiple_PointData_from_dlc(test_csv_path.string(), config);
        
        for (auto const& [bodypart, point_data] : result) {
            REQUIRE(point_data != nullptr);
            REQUIRE(point_data->getTimeCount() == 5);
        }
    }
}

//=============================================================================
// Unit Tests: Error handling
//=============================================================================

TEST_CASE_METHOD(DLCPointCSVUnitTestFixture, 
                 "DLC CSV loader - Error handling", 
                 "[DLC][CSV][Points][error][unit]") {
    
    SECTION("Missing file returns empty result") {
        DLCPointLoaderOptions opts;
        opts.filepath = "non_existent_file.csv";
        
        auto result = load_dlc_csv(opts);
        REQUIRE(result.empty());
    }
    
    SECTION("JSON loader handles missing file") {
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv",
            "likelihood_threshold": 0.0
        })");
        
        auto result = load_multiple_PointData_from_dlc("non_existent_file.csv", config);
        REQUIRE(result.empty());
    }
    
    SECTION("JSON loader works with minimal config (uses defaults)") {
        REQUIRE(std::filesystem::exists(test_csv_path));
        
        nlohmann::json config = nlohmann::json::parse(R"({
            "format": "dlc_csv"
        })");
        
        // Should work with default values
        auto result = load_multiple_PointData_from_dlc(test_csv_path.string(), config);
        
        auto expected_bodyparts = getExpectedBodyparts();
        REQUIRE(result.size() == expected_bodyparts.size());
    }
}

//=============================================================================
// Unit Tests: Simple CSV loader (non-DLC)
//=============================================================================

TEST_CASE("Simple CSV Point loader - Unit tests", "[CSV][Points][unit]") {
    
    // Create temporary test file
    auto temp_dir = std::filesystem::temp_directory_path() / "simple_point_csv_unit_test";
    std::filesystem::create_directories(temp_dir);
    auto csv_path = temp_dir / "simple_points.csv";
    
    // Write test CSV
    {
        std::ofstream file(csv_path);
        file << "0 100.5 200.5\n";
        file << "1 101.0 201.0\n";
        file << "2 102.5 202.5\n";
    }
    
    SECTION("Load simple space-delimited CSV") {
        CSVPointLoaderOptions opts;
        opts.filepath = csv_path.string();
        opts.frame_column = 0;
        opts.x_column = 1;
        opts.y_column = 2;
        opts.column_delim = " ";
        
        auto result = load(opts);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result.find(TimeFrameIndex(0)) != result.end());
        REQUIRE(result.find(TimeFrameIndex(1)) != result.end());
        REQUIRE(result.find(TimeFrameIndex(2)) != result.end());
        
        REQUIRE(result.at(TimeFrameIndex(0)).x == Catch::Approx(100.5f));
        REQUIRE(result.at(TimeFrameIndex(0)).y == Catch::Approx(200.5f));
    }
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}
