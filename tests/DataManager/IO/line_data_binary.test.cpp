#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "IO/CapnProto/Line_Data_Binary.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class LineDataBinaryTestFixture {
public:
    LineDataBinaryTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_binary_output";
        std::filesystem::create_directories(test_dir);
        
        binary_filename = "test_line_data.capnp";
        binary_filepath = test_dir / binary_filename;
        
        // Create test LineData with simple lines
        createTestLineData();
    }
    
    ~LineDataBinaryTestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestLineData() {
        original_line_data = std::make_shared<LineData>();
        
        // Create simple test lines at different time frames
        
        // Line 1 at time frame 0: Simple straight line
        std::vector<Point2D<float>> line1_points = {
            {10.0f, 20.0f},
            {30.0f, 40.0f},
            {50.0f, 60.0f}
        };
        Line2D line1(line1_points);
        
        // Line 2 at time frame 0: L-shaped line
        std::vector<Point2D<float>> line2_points = {
            {100.0f, 100.0f},
            {150.0f, 100.0f},
            {150.0f, 150.0f}
        };
        Line2D line2(line2_points);
        
        // Line 3 at time frame 1: Zigzag line
        std::vector<Point2D<float>> line3_points = {
            {200.0f, 200.0f},
            {250.0f, 250.0f},
            {300.0f, 200.0f},
            {350.0f, 250.0f}
        };
        Line2D line3(line3_points);
        
        // Add lines to LineData at different time frames
        original_line_data->addAtTime(TimeFrameIndex(0), line1, false);
        original_line_data->addAtTime(TimeFrameIndex(0), line2, false);
        original_line_data->addAtTime(TimeFrameIndex(1), line3, false);
        
        // Set image size
        original_line_data->setImageSize(ImageSize(800, 600));
    }
    
    bool saveBinaryLineData() {
        BinaryLineSaverOptions save_opts;
        save_opts.filename = binary_filename;
        save_opts.parent_dir = test_dir.string();
        
        return save(*original_line_data, save_opts);
    }
    
    std::string createJSONConfig() {
        return R"([
{
    "data_type": "line",
    "name": "test_binary_lines",
    "filepath": ")" + binary_filepath.string() + R"(",
    "format": "capnp",
    "color": "#FF0000"
}
])";
    }
    
    void cleanup() {
        try {
            if (std::filesystem::exists(binary_filepath)) {
                std::filesystem::remove(binary_filepath);
            }
            if (std::filesystem::exists(test_dir) && std::filesystem::is_empty(test_dir)) {
                std::filesystem::remove(test_dir);
            }
        } catch (const std::exception& e) {
            // Best effort cleanup - don't fail test if cleanup fails
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    void verifyLineDataEquality(const LineData& loaded_data) const {
        // Get times with data from both original and loaded
        auto original_times = original_line_data->getTimesWithData();
        auto loaded_times = loaded_data.getTimesWithData();
        
        // Convert to vectors for easier comparison
        std::vector<TimeFrameIndex> original_times_vec(original_times.begin(), original_times.end());
        std::vector<TimeFrameIndex> loaded_times_vec(loaded_times.begin(), loaded_times.end());
        
        REQUIRE(original_times_vec.size() == loaded_times_vec.size());
        
        // Sort both vectors to ensure consistent comparison
        std::sort(original_times_vec.begin(), original_times_vec.end());
        std::sort(loaded_times_vec.begin(), loaded_times_vec.end());
        
        // Check each time frame
        for (size_t i = 0; i < original_times_vec.size(); ++i) {
            REQUIRE(original_times_vec[i] == loaded_times_vec[i]);
            
            TimeFrameIndex time = original_times_vec[i];
            const auto& original_lines = original_line_data->getAtTime(time);
            const auto& loaded_lines = loaded_data.getAtTime(time);
            
            REQUIRE(original_lines.size() == loaded_lines.size());
            
            // Check each line at this time frame
            for (size_t line_idx = 0; line_idx < original_lines.size(); ++line_idx) {
                const Line2D& original_line = original_lines[line_idx];
                const Line2D& loaded_line = loaded_lines[line_idx];
                
                REQUIRE(original_line.size() == loaded_line.size());
                
                // Check each point in the line
                for (size_t point_idx = 0; point_idx < original_line.size(); ++point_idx) {
                    Point2D<float> original_point = original_line[point_idx];
                    Point2D<float> loaded_point = loaded_line[point_idx];
                    
                    REQUIRE_THAT(original_point.x, WithinAbs(loaded_point.x, 0.001f));
                    REQUIRE_THAT(original_point.y, WithinAbs(loaded_point.y, 0.001f));
                }
            }
        }
        
        // Check image size
        //REQUIRE(original_line_data->getImageSize().width == loaded_data.getImageSize().width);
        //REQUIRE(original_line_data->getImageSize().height == loaded_data.getImageSize().height);
    }

protected:
    std::filesystem::path test_dir;
    std::string binary_filename;
    std::filesystem::path binary_filepath;
    std::shared_ptr<LineData> original_line_data;
};

TEST_CASE_METHOD(LineDataBinaryTestFixture, "DM - LineData - Binary save and load through direct functions", "[LineData][Binary][IO]") {
    
    SECTION("Save LineData to binary format") {
        bool save_success = saveBinaryLineData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(binary_filepath));
        REQUIRE(std::filesystem::file_size(binary_filepath) > 0);
    }
    
    SECTION("Load LineData from binary format") {
        // First save the data
        REQUIRE(saveBinaryLineData());
        
        // Load using binary loader
        BinaryLineLoaderOptions load_opts;
        load_opts.file_path = binary_filepath.string();
        
        auto loaded_line_data = load(load_opts);
        REQUIRE(loaded_line_data != nullptr);
        
        // Verify the loaded data matches the original
        verifyLineDataEquality(*loaded_line_data);
    }
}

TEST_CASE_METHOD(LineDataBinaryTestFixture, "DM - LineData - Binary load through DataManager JSON config", "[LineData][Binary][IO][DataManager]") {
    
    SECTION("Load binary LineData through DataManager") {
        // First save the data
        REQUIRE(saveBinaryLineData());
        
        // Create JSON config file for loading
        std::string json_config = createJSONConfig();
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
        REQUIRE(!data_info_list.empty());
        REQUIRE(data_info_list.size() == 1);
        
        // Check that the data was registered correctly
        DataInfo const& info = data_info_list[0];
        REQUIRE(info.key == "test_binary_lines");
        REQUIRE(info.data_class == "LineData");
        REQUIRE(info.color == "#FF0000");
        
        // Get the loaded LineData and verify its contents
        auto loaded_line_data = data_manager->getData<LineData>("test_binary_lines");
        REQUIRE(loaded_line_data != nullptr);
        
        verifyLineDataEquality(*loaded_line_data);
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing binary file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = test_dir / "nonexistent.capnp";
        std::string json_config = R"([
{
    "data_type": "line",
    "name": "missing_binary_lines", 
    "filepath": ")" + fake_filepath.string() + R"(",
    "format": "capnp"
}
])";
        
        std::filesystem::path json_filepath = test_dir / "config_missing.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager and attempt to load - should handle gracefully
        auto data_manager = std::make_unique<DataManager>();
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Should return empty list due to missing file
        REQUIRE(data_info_list.empty());
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
}

