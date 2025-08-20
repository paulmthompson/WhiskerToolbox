#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "IO/LoaderRegistry.hpp"
#include "DataManagerTypes.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class LineDataHDF5TestFixture {
public:
    LineDataHDF5TestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_hdf5_output";
        std::filesystem::create_directories(test_dir);
        
        hdf5_filename = "test_line_data.h5";
        hdf5_filepath = test_dir / hdf5_filename;
        
        // Create test LineData with simple lines
        createTestLineData();
    }
    
    ~LineDataHDF5TestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestLineData() {
        original_line_data = std::make_shared<LineData>();
        
        // Create simple test lines at different time frames
        
        // Line 1 at time frame 0: Simple straight line
        std::vector<Point2D<float>> line1_points = {
            {15.0f, 25.0f},
            {35.0f, 45.0f},
            {55.0f, 65.0f}
        };
        Line2D line1(line1_points);
        
        // Line 2 at time frame 0: L-shaped line
        std::vector<Point2D<float>> line2_points = {
            {110.0f, 110.0f},
            {160.0f, 110.0f},
            {160.0f, 160.0f}
        };
        Line2D line2(line2_points);
        
        // Line 3 at time frame 1: Zigzag line
        std::vector<Point2D<float>> line3_points = {
            {210.0f, 210.0f},
            {260.0f, 260.0f},
            {310.0f, 210.0f},
            {360.0f, 260.0f}
        };
        Line2D line3(line3_points);
        
        // Add lines to LineData at different time frames
        original_line_data->addAtTime(TimeFrameIndex(0), line1, false);
        original_line_data->addAtTime(TimeFrameIndex(0), line2, false);
        original_line_data->addAtTime(TimeFrameIndex(1), line3, false);
        
        // Set image size
        original_line_data->setImageSize(ImageSize(800, 600));
    }
    
    bool createMockHDF5File() {
        // Since HDF5 saving is not yet implemented, create a mock file for testing
        // In a real implementation, this would use proper HDF5 saving
        try {
            std::ofstream mock_file(hdf5_filepath, std::ios::binary);
            if (!mock_file.is_open()) {
                return false;
            }
            
            // Write a simple mock HDF5 header to make it look like a real file
            // In reality, this would be a proper HDF5 file with line data
            mock_file << "HDF5_MOCK_LINEDATA";
            mock_file.close();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    std::string getJSONConfig() {
        return R"([
{
    "data_type": "line",
    "name": "test_hdf5_lines",
    "filepath": ")" + hdf5_filepath.string() + R"(",
    "format": "hdf5",
    "frame_key": "frames",
    "x_key": "x",
    "y_key": "y",
    "color": "#00FF00"
}
])";
    }
    
    void cleanup() {
        try {
            if (std::filesystem::exists(hdf5_filepath)) {
                std::filesystem::remove(hdf5_filepath);
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
        REQUIRE(original_line_data->getImageSize().width == loaded_data.getImageSize().width);
        REQUIRE(original_line_data->getImageSize().height == loaded_data.getImageSize().height);
    }

protected:
    std::filesystem::path test_dir;
    std::string hdf5_filename;
    std::filesystem::path hdf5_filepath;
    std::shared_ptr<LineData> original_line_data;
};

TEST_CASE_METHOD(LineDataHDF5TestFixture, "DM - LineData - HDF5 loader registration and JSON config parsing", "[LineData][HDF5][IO][DataManager]") {
    
    SECTION("Test HDF5 loader registration and JSON config handling") {
        // Create a mock HDF5 file
        REQUIRE(createMockHDF5File());
        
        // The actual loading will fail with our mock file, but we can test:
        // 1. JSON config parsing
        // 2. Loader registration
        // 3. Error handling
        
        // Create JSON config
        std::string json_config = getJSONConfig();
        std::filesystem::path json_filepath = test_dir / "config_hdf5.json";
        
        {
            std::ofstream json_file(json_filepath);
            REQUIRE(json_file.is_open());
            json_file << json_config;
        }
        
        // Create DataManager (which triggers loader registration)
        auto data_manager = std::make_unique<DataManager>();
        
        // Check if HDF5 loader is registered
        auto& registry = LoaderRegistry::getInstance();
        
        #ifdef ENABLE_HDF5
        // If HDF5 is enabled, the loader should be registered
        REQUIRE(registry.isFormatSupported("hdf5", IODataType::Line));
        
        // Try to load - this will likely fail with our mock file, but should attempt loading
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // The loading might fail due to invalid HDF5 format, but the JSON parsing should work
        // If it fails, the list should be empty (graceful error handling)
        // If it somehow works (shouldn't with mock data), we can verify it
        INFO("HDF5 loading with mock file - may fail gracefully");
        
        #else
        // If HDF5 is not enabled, the format should not be supported
        REQUIRE_FALSE(registry.isFormatSupported("hdf5", IODataType::Line));
        
        // Loading should fail and return empty list
        auto data_info_list = load_data_from_json_config(data_manager.get(), json_filepath.string());
        REQUIRE(data_info_list.empty());
        #endif
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing HDF5 file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = test_dir / "nonexistent.h5";
        std::string json_config = R"([
{
    "data_type": "line",
    "name": "missing_hdf5_lines", 
    "filepath": ")" + fake_filepath.string() + R"(",
    "format": "hdf5",
    "frame_key": "frames",
    "x_key": "x",
    "y_key": "y"
}
])";
        
        std::filesystem::path json_filepath = test_dir / "config_missing_hdf5.json";
        
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

TEST_CASE_METHOD(LineDataHDF5TestFixture, "DM - LineData - HDF5 loader registration and format support", "[LineData][HDF5][IO]") {
    
    SECTION("Verify HDF5 loader is registered and supports line format") {
        // Create DataManager (which triggers loader registration)
        auto data_manager = std::make_unique<DataManager>();
        
        // Get the registry and check if HDF5 is supported for LineData
        auto& registry = LoaderRegistry::getInstance();
        bool hdf5_supported = registry.isFormatSupported("hdf5", IODataType::Line);
        
        // This should be true if HDF5 is enabled at compile time
        #ifdef ENABLE_HDF5
        REQUIRE(hdf5_supported);
        #else
        // If HDF5 is not enabled, it should not be supported
        REQUIRE_FALSE(hdf5_supported);
        #endif
    }
    
    SECTION("Check HDF5 in supported formats list") {
        auto data_manager = std::make_unique<DataManager>();
        auto& registry = LoaderRegistry::getInstance();
        
        auto supported_formats = registry.getSupportedFormats(IODataType::Line);
        
        #ifdef ENABLE_HDF5
        // HDF5 should be in the list of supported formats
        bool found_hdf5 = std::find(supported_formats.begin(), supported_formats.end(), "hdf5") 
                         != supported_formats.end();
        REQUIRE(found_hdf5);
        #endif
        
        // CSV should always be supported (internal plugin)
        bool found_csv = std::find(supported_formats.begin(), supported_formats.end(), "csv") 
                        != supported_formats.end();
        REQUIRE(found_csv);
    }
}
