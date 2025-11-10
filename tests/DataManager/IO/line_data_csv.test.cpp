#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "IO/LoaderRegistry.hpp"
#include "IO/interface/IOTypes.hpp"
#include "ConcreteDataFactory.hpp"
#include "CoreGeometry/lines.hpp"


#include "DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include "IO/LoaderRegistry.hpp"
#include "IO/interface/IOTypes.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class LineDataCSVTestFixture {
public:
    LineDataCSVTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_csv_output";
        std::filesystem::create_directories(test_dir);
        
        csv_filename = "test_line_data.csv";
        csv_filepath = test_dir / csv_filename;
        
        // Create test LineData with simple lines
        createTestLineData();
    }
    
    ~LineDataCSVTestFixture() {
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
        original_line_data->addAtTime(TimeFrameIndex(0), line1, NotifyObservers::No);
        original_line_data->addAtTime(TimeFrameIndex(0), line2, NotifyObservers::No);
        original_line_data->addAtTime(TimeFrameIndex(1), line3, NotifyObservers::No);
        
        // Set image size
        original_line_data->setImageSize(ImageSize(800, 600));
    }
    
    bool saveCSVLineData() {
        CSVSingleFileLineSaverOptions save_opts;
        save_opts.filename = csv_filename;
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;
        save_opts.save_header = true;
        
        save(original_line_data.get(), save_opts);
        return std::filesystem::exists(csv_filepath);
    }
    
    std::string createJSONConfig() {
        return R"([
{
    "data_type": "line",
    "name": "test_csv_lines",
    "filepath": ")" + csv_filepath.string() + R"(",
    "format": "csv",
    "color": "#00FF00",
    "delimiter": ",",
    "coordinate_delimiter": ",",
    "has_header": true,
    "header_identifier": "Frame",
    "image_height": 600,
    "image_width": 800
}
])";
    }
    
    void cleanup() {
        try {
            if (std::filesystem::exists(csv_filepath)) {
                std::filesystem::remove(csv_filepath);
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
                    
                    REQUIRE_THAT(original_point.x, WithinAbs(loaded_point.x, 0.01f));
                    REQUIRE_THAT(original_point.y, WithinAbs(loaded_point.y, 0.01f));
                }
            }
        }
        
        // Check image size
        REQUIRE(original_line_data->getImageSize().width == loaded_data.getImageSize().width);
        REQUIRE(original_line_data->getImageSize().height == loaded_data.getImageSize().height);
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
    std::shared_ptr<LineData> original_line_data;
};

TEST_CASE_METHOD(LineDataCSVTestFixture, "DM - IO - LineData - CSV save and load through direct functions", "[LineData][CSV][IO]") {
    
    SECTION("Save LineData to CSV format") {
        bool save_success = saveCSVLineData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);
    }
    
    SECTION("Load LineData from CSV format") {
        // First save the data
        REQUIRE(saveCSVLineData());
        
        // Load using CSV loader
        CSVSingleFileLineLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.coordinate_delimiter = ",";
        load_opts.has_header = true;
        load_opts.header_identifier = "Frame";
        
        auto line_map = load(load_opts);
        auto loaded_line_data = std::make_shared<LineData>(line_map);
        REQUIRE(loaded_line_data != nullptr);
        
        // We set the size outside of the loader function
        loaded_line_data->setImageSize(ImageSize(800, 600));
        verifyLineDataEquality(*loaded_line_data);
    }
}

TEST_CASE_METHOD(LineDataCSVTestFixture, "DM - IO - LineData - CSV load through DataManager JSON config", "[LineData][CSV][IO][DataManager]") {
    
    SECTION("Load CSV LineData through DataManager") {
        // First save the data
        REQUIRE(saveCSVLineData());
        
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
        REQUIRE(info.key == "test_csv_lines");
        REQUIRE(info.data_class == "LineData");
        REQUIRE(info.color == "#00FF00");
        
        // Get the loaded LineData and verify its contents
        auto loaded_line_data = data_manager->getData<LineData>("test_csv_lines");
        REQUIRE(loaded_line_data != nullptr);
        
        verifyLineDataEquality(*loaded_line_data);
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing CSV file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = test_dir / "nonexistent.csv";
        std::string json_config = R"([
{
    "data_type": "line",
    "name": "missing_csv_lines", 
    "filepath": ")" + fake_filepath.string() + R"(",
    "format": "csv"
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

TEST_CASE_METHOD(LineDataCSVTestFixture, "DM - IO - LineData - CSV save/load through LoaderRegistry", "[LineData][CSV][IO][LoaderRegistry]") {
    
    SECTION("Save LineData through LoaderRegistry") {
        // Create DataManager (which triggers loader registration)
        auto data_manager = std::make_unique<DataManager>();
        
        // Check if CSV loader is registered for Line data type
        auto& registry = LoaderRegistry::getInstance();
        REQUIRE(registry.isFormatSupported("csv", IODataType::Line));
        
        // Create a minimal JSON config for saving
        nlohmann::json config;
        config["save_type"] = "single";  // Use single-file mode
        config["parent_dir"] = test_dir.string();
        config["filename"] = csv_filepath.filename().string();
        config["delimiter"] = ",";
        config["line_delim"] = "\n";
        config["save_header"] = true;
        config["header"] = "Frame,X,Y";
        config["precision"] = 2;
        
        // Test saving through registry - this exercises the same code path as the GUI
        auto result = registry.trySave("csv", 
                                     IODataType::Line, 
                                     csv_filepath.string(),
                                     config, 
                                     original_line_data.get());
        
        // Debug: Print the result details
        std::cout << "Save result success: " << result.success << std::endl;
        std::cout << "Save result error: " << result.error_message << std::endl;
        std::cout << "CSV filepath: " << csv_filepath.string() << std::endl;
        
        REQUIRE(result.success);
        REQUIRE(result.error_message.empty());
        
        // Verify the file was actually created and has content
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);
    }
    
    SECTION("Load LineData through LoaderRegistry") {
        // First save the data using the direct method to ensure we have a file
        REQUIRE(saveCSVLineData());
        
        // Now try to load through registry - this would test the loader interface
        auto data_manager = std::make_unique<DataManager>();
        auto& registry = LoaderRegistry::getInstance();
        
        // Create JSON config for loading
        nlohmann::json config;
        config["filepath"] = csv_filepath.string();
        config["delimiter"] = ",";
        config["coordinate_delimiter"] = ",";
        config["has_header"] = true;
        config["header_identifier"] = "Frame";
        config["image_height"] = 600;
        config["image_width"] = 800;

        ConcreteDataFactory factory;
        auto load_result = registry.tryLoad("csv", 
                                          IODataType::Line, 
                                          csv_filepath.string(), 
                                          config, 
                                          &factory);
        REQUIRE(load_result.success);
        REQUIRE(std::holds_alternative<std::shared_ptr<LineData>>(load_result.data));
        
        // Verify the loaded data matches the original
        auto loaded_line_data = std::get<std::shared_ptr<LineData>>(load_result.data);
        REQUIRE(loaded_line_data != nullptr);
        verifyLineDataEquality(*loaded_line_data);
    }
}
