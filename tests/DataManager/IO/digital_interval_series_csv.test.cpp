#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "TimeFrame/interval_data.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class DigitalIntervalSeriesCSVTestFixture {
public:
    DigitalIntervalSeriesCSVTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_digital_interval_csv_output";
        std::filesystem::create_directories(test_dir);
        
        csv_filename = "test_intervals.csv";
        csv_filepath = test_dir / csv_filename;
        
        // Create test DigitalIntervalSeries with sample intervals
        createTestIntervalData();
    }
    
    ~DigitalIntervalSeriesCSVTestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestIntervalData() {
        // Create test intervals with start and end times
        std::vector<Interval> test_intervals = {
            Interval{10, 25},     // First interval: 10-25
            Interval{50, 75},     // Second interval: 50-75
            Interval{100, 150},   // Third interval: 100-150
            Interval{200, 220},   // Fourth interval: 200-220
            Interval{300, 350}    // Fifth interval: 300-350
        };
        
        original_interval_data = std::make_shared<DigitalIntervalSeries>(test_intervals);
    }
    
    bool saveCSVIntervalData() {
        CSVIntervalSaverOptions save_opts;
        save_opts.filename = csv_filename;
        save_opts.parent_dir = test_dir.string();
        save_opts.save_header = true;
        save_opts.header = "Start,End";
        save_opts.delimiter = ",";
        
        save(original_interval_data.get(), save_opts);
        return std::filesystem::exists(csv_filepath);
    }
    
    std::string createJSONConfig() {
        return R"([
{
    "data_type": "digital_interval",
    "name": "test_csv_intervals",
    "filepath": ")" + csv_filepath.string() + R"(",
    "format": "csv",
    "color": "#FF00FF",
    "skip_header": true,
    "delimiter": ","
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
    
    void verifyIntervalDataEquality(const DigitalIntervalSeries& loaded_data) const {
        // Get intervals from both original and loaded data
        const std::vector<Interval>& original_intervals = original_interval_data->getDigitalIntervalSeries();
        const std::vector<Interval>& loaded_intervals = loaded_data.getDigitalIntervalSeries();
        
        // Check number of intervals
        REQUIRE(loaded_intervals.size() == original_intervals.size());
        
        // Check each interval
        for (size_t i = 0; i < original_intervals.size(); ++i) {
            REQUIRE(original_intervals[i].start == loaded_intervals[i].start);
            REQUIRE(original_intervals[i].end == loaded_intervals[i].end);
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
    std::shared_ptr<DigitalIntervalSeries> original_interval_data;
};

TEST_CASE_METHOD(DigitalIntervalSeriesCSVTestFixture, "DM - IO - DigitalIntervalSeries - CSV save and load through direct functions", "[DigitalIntervalSeries][CSV][IO]") {
    
    SECTION("Save DigitalIntervalSeries to CSV format") {
        bool save_success = saveCSVIntervalData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);
        
        // Verify CSV file contents by reading a few lines
        std::ifstream file(csv_filepath);
        std::string line;
        
        // Check header
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Start,End");
        
        // Check first data line
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "10,25");
        
        // Check second data line
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "50,75");
        
        file.close();
    }
    
    SECTION("Load DigitalIntervalSeries from CSV format using new loader") {
        // First save the data
        REQUIRE(saveCSVIntervalData());
        
        // Load using CSV loader
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 0;
        load_opts.end_column = 1;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(!loaded_intervals.empty());
        REQUIRE(loaded_intervals.size() == 5);
        
        // Create DigitalIntervalSeries from loaded intervals and verify
        auto loaded_interval_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);
        verifyIntervalDataEquality(*loaded_interval_data);
    }
    
    SECTION("Load DigitalIntervalSeries from CSV format using legacy loader") {
        // Create a simple space-delimited CSV file for testing the legacy loader
        std::filesystem::path legacy_filepath = test_dir / "legacy_intervals.csv";
        std::ofstream legacy_file(legacy_filepath);
        legacy_file << "10 25\n50 75\n100 150\n";
        legacy_file.close();
        
        // Test the legacy load function
        auto loaded_intervals = load_digital_series_from_csv(legacy_filepath.string(), ' ');
        
        REQUIRE(loaded_intervals.size() == 3);
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
        REQUIRE(loaded_intervals[1].start == 50);
        REQUIRE(loaded_intervals[1].end == 75);
        REQUIRE(loaded_intervals[2].start == 100);
        REQUIRE(loaded_intervals[2].end == 150);
        
        // Clean up
        std::filesystem::remove(legacy_filepath);
    }
    
    SECTION("Load DigitalIntervalSeries with different column ordering") {
        // Create CSV with end column first, then start column
        std::filesystem::path custom_filepath = test_dir / "custom_order.csv";
        std::ofstream custom_file(custom_filepath);
        custom_file << "End,Start\n25,10\n75,50\n150,100\n";
        custom_file.close();
        
        // Load using custom column ordering
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = custom_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 1;  // Start is in column 1
        load_opts.end_column = 0;    // End is in column 0
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(!loaded_intervals.empty());
        REQUIRE(loaded_intervals.size() == 3);
        
        // Verify the intervals are loaded correctly
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
        REQUIRE(loaded_intervals[1].start == 50);
        REQUIRE(loaded_intervals[1].end == 75);
        REQUIRE(loaded_intervals[2].start == 100);
        REQUIRE(loaded_intervals[2].end == 150);
        
        // Clean up
        std::filesystem::remove(custom_filepath);
    }
}

TEST_CASE_METHOD(DigitalIntervalSeriesCSVTestFixture, "DM - IO - DigitalIntervalSeries - CSV load through DataManager JSON config", "[DigitalIntervalSeries][CSV][IO][DataManager]") {
    
    SECTION("Load CSV DigitalIntervalSeries through DataManager") {
        // First save the data
        REQUIRE(saveCSVIntervalData());
        
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
        load_data_from_json_config(data_manager.get(), json_filepath.string());
        
        // Get the loaded DigitalIntervalSeries and verify its contents
        auto loaded_interval_data = data_manager->getData<DigitalIntervalSeries>("test_csv_intervals");
        REQUIRE(loaded_interval_data != nullptr);
        
        verifyIntervalDataEquality(*loaded_interval_data);
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing CSV file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = test_dir / "nonexistent.csv";
        std::string json_config = R"([
{
    "data_type": "digital_interval",
    "name": "missing_csv_intervals", 
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
    
    SECTION("Error handling with invalid interval data") {
        // Create CSV with invalid intervals (start > end)
        std::filesystem::path invalid_filepath = test_dir / "invalid_intervals.csv";
        std::ofstream invalid_file(invalid_filepath);
        invalid_file << "Start,End\n100,50\n200,250\n";  // First interval has start > end, second is valid
        invalid_file.close();
        
        // Load using CSV loader - should handle gracefully
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = invalid_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 0;
        load_opts.end_column = 1;
        
        auto loaded_intervals = load(load_opts);
        
        // Should only load the valid interval (100-50 gets rejected because start > end)
        REQUIRE(loaded_intervals.size() == 1);  // Only the valid interval should be loaded
        REQUIRE(loaded_intervals[0].start == 200);
        REQUIRE(loaded_intervals[0].end == 250);
        
        // Clean up
        std::filesystem::remove(invalid_filepath);
    }
}
