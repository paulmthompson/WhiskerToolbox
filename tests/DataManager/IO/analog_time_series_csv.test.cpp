#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class AnalogTimeSeriesCSVTestFixture {
public:
    AnalogTimeSeriesCSVTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_analog_csv_output";
        std::filesystem::create_directories(test_dir);
        
        csv_filename = "test_analog_data.csv";
        csv_filepath = test_dir / csv_filename;
        
        // Create test AnalogTimeSeries with sample data
        createTestAnalogData();
    }
    
    ~AnalogTimeSeriesCSVTestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestAnalogData() {
        // Create test data with time and values
        std::vector<float> test_values = {
            1.5f, 2.3f, 3.7f, 4.1f, 5.9f, 6.2f, 7.8f, 8.4f, 9.1f, 10.6f
        };
        
        std::vector<TimeFrameIndex> test_times = {
            TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20), 
            TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50),
            TimeFrameIndex(60), TimeFrameIndex(70), TimeFrameIndex(80), 
            TimeFrameIndex(90)
        };
        
        original_analog_data = std::make_shared<AnalogTimeSeries>(test_values, test_times);
    }
    
    bool saveCSVAnalogData() {
        CSVAnalogSaverOptions save_opts;
        save_opts.filename = csv_filename;
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;
        save_opts.save_header = true;
        save_opts.header = "Time,Data";
        save_opts.delimiter = ",";
        
        save(original_analog_data.get(), save_opts);
        return std::filesystem::exists(csv_filepath);
    }
    
    std::string createJSONConfig() {
        return R"([
{
    "data_type": "analog",
    "name": "test_csv_analog",
    "filepath": ")" + csv_filepath.string() + R"(",
    "format": "csv",
    "color": "#0000FF",
    "delimiter": ",",
    "has_header": true,
    "single_column_format": false,
    "time_column": 0,
    "data_column": 1
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
    
    void verifyAnalogDataEquality(const AnalogTimeSeries& loaded_data) const {
        // Check number of samples
        REQUIRE(loaded_data.getNumSamples() == original_analog_data->getNumSamples());
        
        size_t num_samples = original_analog_data->getNumSamples();
        
        // Check each data point and timestamp
        for (size_t i = 0; i < num_samples; ++i) {
            DataArrayIndex data_idx(i);
            
            // Check data values
            float original_value = original_analog_data->getDataAtDataArrayIndex(data_idx);
            float loaded_value = loaded_data.getDataAtDataArrayIndex(data_idx);
            REQUIRE_THAT(original_value, WithinAbs(loaded_value, 0.01f));
            
            // Check time frame indices
            TimeFrameIndex original_time = original_analog_data->getTimeFrameIndexAtDataArrayIndex(data_idx);
            TimeFrameIndex loaded_time = loaded_data.getTimeFrameIndexAtDataArrayIndex(data_idx);
            REQUIRE(original_time.getValue() == loaded_time.getValue());
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
    std::shared_ptr<AnalogTimeSeries> original_analog_data;
};

TEST_CASE_METHOD(AnalogTimeSeriesCSVTestFixture, "DM - AnalogTimeSeries - CSV save and load through direct functions", "[AnalogTimeSeries][CSV][IO]") {
    
    SECTION("Save AnalogTimeSeries to CSV format") {
        bool save_success = saveCSVAnalogData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);
        
        // Verify CSV file contents by reading a few lines
        std::ifstream file(csv_filepath);
        std::string line;
        
        // Check header
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Time,Data");
        
        // Check first data line
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "0,1.50");
        
        file.close();
    }
    
    SECTION("Load AnalogTimeSeries from CSV format") {
        // First save the data
        REQUIRE(saveCSVAnalogData());
        
        // Load using CSV loader
        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.single_column_format = false;
        load_opts.time_column = 0;
        load_opts.data_column = 1;
        
        auto loaded_analog_data = load(load_opts);
        REQUIRE(loaded_analog_data != nullptr);
        
        // Verify the loaded data matches the original
        verifyAnalogDataEquality(*loaded_analog_data);
    }
    
    SECTION("Load AnalogTimeSeries from single column CSV format") {
        // Create a single column CSV file for testing
        std::filesystem::path single_col_filepath = test_dir / "single_column.csv";
        std::ofstream single_col_file(single_col_filepath);
        single_col_file << "1.5\n2.3\n3.7\n4.1\n5.9\n";
        single_col_file.close();
        
        // Load using single column format
        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = single_col_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = false;
        load_opts.single_column_format = true;
        
        auto loaded_analog_data = load(load_opts);
        REQUIRE(loaded_analog_data != nullptr);
        REQUIRE(loaded_analog_data->getNumSamples() == 5);
        
        // Check first few values
        REQUIRE_THAT(loaded_analog_data->getDataAtDataArrayIndex(DataArrayIndex(0)), WithinAbs(1.5f, 0.01f));
        REQUIRE_THAT(loaded_analog_data->getDataAtDataArrayIndex(DataArrayIndex(1)), WithinAbs(2.3f, 0.01f));
        REQUIRE_THAT(loaded_analog_data->getDataAtDataArrayIndex(DataArrayIndex(2)), WithinAbs(3.7f, 0.01f));
        
        // Clean up
        std::filesystem::remove(single_col_filepath);
    }
}

TEST_CASE_METHOD(AnalogTimeSeriesCSVTestFixture, "DM - AnalogTimeSeries - CSV load through DataManager JSON config", "[AnalogTimeSeries][CSV][IO][DataManager]") {
    
    SECTION("Load CSV AnalogTimeSeries through DataManager") {
        // First save the data
        REQUIRE(saveCSVAnalogData());
        
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
        
        // Get the loaded AnalogTimeSeries and verify its contents
        auto loaded_analog_data = data_manager->getData<AnalogTimeSeries>("test_csv_analog_0");
        REQUIRE(loaded_analog_data != nullptr);
        
        verifyAnalogDataEquality(*loaded_analog_data);
        
        // Clean up JSON config file
        std::filesystem::remove(json_filepath);
    }
    
    SECTION("DataManager handles missing CSV file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = test_dir / "nonexistent.csv";
        std::string json_config = R"([
{
    "data_type": "analog",
    "name": "missing_csv_analog", 
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
    
    SECTION("Test legacy CSV loader function") {
        // Create a simple single column CSV file
        std::filesystem::path legacy_filepath = test_dir / "legacy.csv";
        std::ofstream legacy_file(legacy_filepath);
        legacy_file << "1.0\n2.0\n3.0\n4.0\n5.0\n6.0\n";
        legacy_file.close();
        
        // Test the legacy load function
        auto data = load_analog_series_from_csv(legacy_filepath.string());
        
        REQUIRE(data.size() == 6);
        REQUIRE_THAT(data[0], WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(data[1], WithinAbs(2.0f, 0.01f));
        REQUIRE_THAT(data[5], WithinAbs(6.0f, 0.01f));
        
        // Clean up
        std::filesystem::remove(legacy_filepath);
    }
}
