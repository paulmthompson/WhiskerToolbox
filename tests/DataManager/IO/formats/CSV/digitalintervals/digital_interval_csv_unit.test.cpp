/**
 * @file digital_interval_csv_unit.test.cpp
 * @brief Unit tests for DigitalIntervalSeries CSV direct function calls and legacy APIs
 * 
 * These tests exercise the CSV loading/saving functions directly without going through
 * the DataManager JSON config interface. They complement the integration tests
 * in digital_interval_csv_integration.test.cpp.
 * 
 * Tests include:
 * 1. Direct save via CSVIntervalSaverOptions
 * 2. Direct load via CSVIntervalLoaderOptions
 * 3. Legacy load_digital_series_from_csv function
 * 4. Column ordering via start_column/end_column
 * 5. Validation of invalid intervals (start > end)
 */

#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DigitalTimeSeries/IO/CSV/Digital_Interval_Series_CSV.hpp"
#include "TimeFrame/interval_data.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

class DigitalIntervalCSVUnitTestFixture {
public:
    DigitalIntervalCSVUnitTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_digital_interval_csv_unit_output";
        std::filesystem::create_directories(test_dir);
        
        csv_filename = "test_intervals.csv";
        csv_filepath = test_dir / csv_filename;
        
        // Create test DigitalIntervalSeries with sample intervals
        createTestIntervalData();
    }
    
    ~DigitalIntervalCSVUnitTestFixture() {
        cleanup();
    }

protected:
    void createTestIntervalData() {
        std::vector<Interval> test_intervals = {
            Interval{10, 25},
            Interval{50, 75},
            Interval{100, 150},
            Interval{200, 220},
            Interval{300, 350}
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
    
    void cleanup() {
        try {
            if (std::filesystem::exists(test_dir)) {
                std::filesystem::remove_all(test_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }
    
    void verifyIntervalDataEquality(const DigitalIntervalSeries& loaded_data) const {
        auto original_intervals = original_interval_data->view();
        auto loaded_intervals = loaded_data.view();
        
        REQUIRE(loaded_data.size() == original_interval_data->size());
        
        for (size_t i = 0; i < original_interval_data->size(); ++i) {
            REQUIRE(original_intervals[i].value().start == loaded_intervals[i].value().start);
            REQUIRE(original_intervals[i].value().end == loaded_intervals[i].value().end);
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
    std::shared_ptr<DigitalIntervalSeries> original_interval_data;
};

//=============================================================================
// Direct Save Tests (CSVIntervalSaverOptions)
//=============================================================================

TEST_CASE_METHOD(DigitalIntervalCSVUnitTestFixture, 
                 "DigitalInterval CSV Unit - Save to CSV format", 
                 "[digitalinterval][csv][unit][save]") {
    
    SECTION("Save with default options") {
        bool save_success = saveCSVIntervalData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);
        
        // Verify CSV file contents
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
    
    SECTION("Save with custom delimiter") {
        CSVIntervalSaverOptions save_opts;
        save_opts.filename = "tab_delimited.tsv";
        save_opts.parent_dir = test_dir.string();
        save_opts.save_header = true;
        save_opts.header = "Start\tEnd";
        save_opts.delimiter = "\t";
        
        save(original_interval_data.get(), save_opts);
        
        auto filepath = test_dir / "tab_delimited.tsv";
        REQUIRE(std::filesystem::exists(filepath));
        
        std::ifstream file(filepath);
        std::string line;
        
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Start\tEnd");
        
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "10\t25");
    }
    
    SECTION("Save without header") {
        CSVIntervalSaverOptions save_opts;
        save_opts.filename = "no_header.csv";
        save_opts.parent_dir = test_dir.string();
        save_opts.save_header = false;
        save_opts.delimiter = ",";
        
        save(original_interval_data.get(), save_opts);
        
        auto filepath = test_dir / "no_header.csv";
        REQUIRE(std::filesystem::exists(filepath));
        
        std::ifstream file(filepath);
        std::string line;
        
        // First line should be data, not header
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "10,25");
    }
}

//=============================================================================
// Direct Load Tests (CSVIntervalLoaderOptions)
//=============================================================================

TEST_CASE_METHOD(DigitalIntervalCSVUnitTestFixture, 
                 "DigitalInterval CSV Unit - Load via CSVIntervalLoaderOptions", 
                 "[digitalinterval][csv][unit][load]") {
    
    SECTION("Load with header") {
        REQUIRE(saveCSVIntervalData());
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 0;
        load_opts.end_column = 1;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(!loaded_intervals.empty());
        REQUIRE(loaded_intervals.size() == 5);
        
        auto loaded_interval_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);
        verifyIntervalDataEquality(*loaded_interval_data);
    }
    
    SECTION("Load without header") {
        // Create CSV without header
        auto no_header_path = test_dir / "no_header_load.csv";
        {
            std::ofstream file(no_header_path);
            file << "10,25\n50,75\n100,150\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = no_header_path.string();
        load_opts.delimiter = ",";
        load_opts.has_header = false;
        load_opts.start_column = 0;
        load_opts.end_column = 1;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(loaded_intervals.size() == 3);
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
    }
    
    SECTION("Load with custom column ordering (end first, start second)") {
        auto custom_path = test_dir / "custom_order.csv";
        {
            std::ofstream file(custom_path);
            file << "End,Start\n25,10\n75,50\n150,100\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = custom_path.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 1;  // Start is in column 1
        load_opts.end_column = 0;    // End is in column 0
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(loaded_intervals.size() == 3);
        
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
        REQUIRE(loaded_intervals[1].start == 50);
        REQUIRE(loaded_intervals[1].end == 75);
        REQUIRE(loaded_intervals[2].start == 100);
        REQUIRE(loaded_intervals[2].end == 150);
    }
    
    SECTION("Load with semicolon delimiter") {
        auto semicolon_path = test_dir / "semicolon.csv";
        {
            std::ofstream file(semicolon_path);
            file << "Start;End\n10;25\n50;75\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = semicolon_path.string();
        load_opts.delimiter = ";";
        load_opts.has_header = true;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(loaded_intervals.size() == 2);
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
    }
}

//=============================================================================
// Legacy Loader Tests
//=============================================================================

TEST_CASE_METHOD(DigitalIntervalCSVUnitTestFixture, 
                 "DigitalInterval CSV Unit - Legacy load_digital_series_from_csv", 
                 "[digitalinterval][csv][unit][legacy]") {
    
    SECTION("Load space-delimited file") {
        auto legacy_path = test_dir / "legacy_space.csv";
        {
            std::ofstream file(legacy_path);
            file << "10 25\n50 75\n100 150\n";
        }
        
        auto loaded_intervals = load_digital_series_from_csv(legacy_path.string(), ' ');
        
        REQUIRE(loaded_intervals.size() == 3);
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
        REQUIRE(loaded_intervals[1].start == 50);
        REQUIRE(loaded_intervals[1].end == 75);
        REQUIRE(loaded_intervals[2].start == 100);
        REQUIRE(loaded_intervals[2].end == 150);
    }
    
    SECTION("Load comma-delimited file") {
        auto legacy_path = test_dir / "legacy_comma.csv";
        {
            std::ofstream file(legacy_path);
            file << "10,25\n50,75\n";
        }
        
        auto loaded_intervals = load_digital_series_from_csv(legacy_path.string(), ',');
        
        REQUIRE(loaded_intervals.size() == 2);
        REQUIRE(loaded_intervals[0].start == 10);
        REQUIRE(loaded_intervals[0].end == 25);
    }
}

//=============================================================================
// Validation and Error Handling Tests
//=============================================================================

TEST_CASE_METHOD(DigitalIntervalCSVUnitTestFixture, 
                 "DigitalInterval CSV Unit - Validation and error handling", 
                 "[digitalinterval][csv][unit][validation]") {
    
    SECTION("Invalid intervals (start > end) are rejected") {
        auto invalid_path = test_dir / "invalid_intervals.csv";
        {
            std::ofstream file(invalid_path);
            // First interval: start > end (invalid)
            // Second interval: valid
            file << "Start,End\n100,50\n200,250\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = invalid_path.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.start_column = 0;
        load_opts.end_column = 1;
        
        auto loaded_intervals = load(load_opts);
        
        // Only the valid interval should be loaded
        REQUIRE(loaded_intervals.size() == 1);
        REQUIRE(loaded_intervals[0].start == 200);
        REQUIRE(loaded_intervals[0].end == 250);
    }
    
    SECTION("Empty file returns empty vector") {
        auto empty_path = test_dir / "empty.csv";
        {
            std::ofstream file(empty_path);
            file << "Start,End\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = empty_path.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(loaded_intervals.empty());
    }
    
    SECTION("All invalid intervals returns empty vector") {
        auto all_invalid_path = test_dir / "all_invalid.csv";
        {
            std::ofstream file(all_invalid_path);
            file << "Start,End\n100,50\n200,100\n300,200\n";
        }
        
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = all_invalid_path.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        
        auto loaded_intervals = load(load_opts);
        REQUIRE(loaded_intervals.empty());
    }
}

//=============================================================================
// Round-trip Tests (Save then Load)
//=============================================================================

TEST_CASE_METHOD(DigitalIntervalCSVUnitTestFixture, 
                 "DigitalInterval CSV Unit - Round-trip save and load", 
                 "[digitalinterval][csv][unit][roundtrip]") {
    
    SECTION("Data integrity preserved through save/load cycle") {
        // Save
        REQUIRE(saveCSVIntervalData());
        
        // Load
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        
        auto loaded_intervals = load(load_opts);
        auto loaded_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);
        
        // Verify
        verifyIntervalDataEquality(*loaded_data);
    }
    
    SECTION("Round-trip with custom delimiter") {
        // Save with tab delimiter
        CSVIntervalSaverOptions save_opts;
        save_opts.filename = "roundtrip_tab.tsv";
        save_opts.parent_dir = test_dir.string();
        save_opts.save_header = true;
        save_opts.header = "Start\tEnd";
        save_opts.delimiter = "\t";
        
        save(original_interval_data.get(), save_opts);
        
        // Load with tab delimiter
        CSVIntervalLoaderOptions load_opts;
        load_opts.filepath = (test_dir / "roundtrip_tab.tsv").string();
        load_opts.delimiter = "\t";
        load_opts.has_header = true;
        
        auto loaded_intervals = load(load_opts);
        auto loaded_data = std::make_shared<DigitalIntervalSeries>(loaded_intervals);
        
        verifyIntervalDataEquality(*loaded_data);
    }
}
