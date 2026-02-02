/**
 * @file digital_interval_binary_state_integration.test.cpp
 * @brief Integration tests for loading DigitalIntervalSeries from binary state CSV via DataManager JSON config
 * 
 * Tests the binary_state csv_layout where:
 * - Rows represent time points
 * - Columns contain binary state values (0 or 1)
 * - Intervals are extracted from contiguous "on" regions
 * 
 * Tests:
 * 1. Single column loading via csv_layout="binary_state"
 * 2. Batch loading all columns via all_columns=true
 * 3. Custom configuration options (threshold, delimiter, header lines)
 * 4. Integration with real test data (jun_test.dat)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <nlohmann/json.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using json = nlohmann::json;

namespace {

/**
 * @brief Helper class for managing temporary test directories
 */
class TempBinaryStateTestDirectory {
public:
    TempBinaryStateTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_binary_state_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempBinaryStateTestDirectory() {
        if (std::filesystem::exists(temp_path)) {
            std::filesystem::remove_all(temp_path);
        }
    }
    
    std::filesystem::path getPath() const { return temp_path; }
    std::string getPathString() const { return temp_path.string(); }
    
    std::filesystem::path getFilePath(std::string const& filename) const {
        return temp_path / filename;
    }

private:
    std::filesystem::path temp_path;
};

/**
 * @brief Write a binary state CSV file for testing
 * 
 * @param filepath Path to write file
 * @param header_lines Number of header lines before column names
 * @param column_names Names for columns (first is time, rest are data)
 * @param data Matrix of values (rows x columns), first column is time
 * @param delimiter Column separator
 */
void writeBinaryStateCSV(
    std::string const& filepath,
    int header_lines,
    std::vector<std::string> const& column_names,
    std::vector<std::vector<double>> const& data,
    std::string const& delimiter = "\t"
) {
    std::ofstream file(filepath);
    REQUIRE(file.is_open());
    
    // Write header lines (can be dates, times, blank lines, etc.)
    for (int i = 0; i < header_lines; ++i) {
        if (i == 0) file << "Test Header Line " << i << "\n";
        else file << "\n";  // Blank lines
    }
    
    // Write column names
    for (size_t i = 0; i < column_names.size(); ++i) {
        if (i > 0) file << delimiter;
        file << column_names[i];
    }
    file << "\n";
    
    // Write data rows
    for (auto const& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) file << delimiter;
            file << std::fixed << std::setprecision(6) << row[i];
        }
        file << "\n";
    }
    
    file.close();
}

/**
 * @brief Get path to real test data file (jun_test.dat)
 */
std::string getJunTestDataPath() {
    return "data/DigitalIntervals/jun_test.dat";
}

} // anonymous namespace

//=============================================================================
// Test Case 1: Single Column Binary State Loading
//=============================================================================

TEST_CASE("DigitalInterval Binary State - Single Column Loading", 
          "[digitalinterval][csv][binary_state][integration][datamanager]") {
    TempBinaryStateTestDirectory temp_dir;
    
    SECTION("Simple on-off pattern") {
        // Create test data: Time, ch0 
        // Pattern: on for 3 rows, off for 2, on for 2, off for 3
        std::vector<std::vector<double>> data = {
            {0.0, 1.0},  // on
            {0.1, 1.0},  // on
            {0.2, 1.0},  // on
            {0.3, 0.0},  // off
            {0.4, 0.0},  // off
            {0.5, 1.0},  // on
            {0.6, 1.0},  // on
            {0.7, 0.0},  // off
            {0.8, 0.0},  // off
            {0.9, 0.0},  // off
        };
        
        auto csv_path = temp_dir.getFilePath("simple_binary.csv");
        writeBinaryStateCSV(csv_path.string(), 2, {"Time", "ch0"}, data, "\t");
        
        // Create JSON config
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "test_binary_state"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 2},
                {"data_column", 1},
                {"delimiter", "\t"},
                {"binary_threshold", 0.5}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("test_binary_state");
        REQUIRE(loaded != nullptr);
        
        // Should have 2 intervals: [0,2] and [5,6]
        REQUIRE(loaded->size() == 2);
        
        auto intervals = loaded->view();
        CHECK(intervals[0].value().start == 0);
        CHECK(intervals[0].value().end == 2);
        CHECK(intervals[1].value().start == 5);
        CHECK(intervals[1].value().end == 6);
    }
    
    SECTION("All ones - single interval spanning entire data") {
        std::vector<std::vector<double>> data;
        for (int i = 0; i < 10; ++i) {
            data.push_back({static_cast<double>(i) * 0.1, 1.0});
        }
        
        auto csv_path = temp_dir.getFilePath("all_ones.csv");
        writeBinaryStateCSV(csv_path.string(), 2, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "all_ones"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 2},
                {"data_column", 1},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("all_ones");
        REQUIRE(loaded != nullptr);
        
        // Should have exactly 1 interval spanning [0, 9]
        REQUIRE(loaded->size() == 1);
        
        auto intervals = loaded->view();
        CHECK(intervals[0].value().start == 0);
        CHECK(intervals[0].value().end == 9);
    }
    
    SECTION("All zeros - no intervals") {
        std::vector<std::vector<double>> data;
        for (int i = 0; i < 10; ++i) {
            data.push_back({static_cast<double>(i) * 0.1, 0.0});
        }
        
        auto csv_path = temp_dir.getFilePath("all_zeros.csv");
        writeBinaryStateCSV(csv_path.string(), 2, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "all_zeros"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 2},
                {"data_column", 1},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("all_zeros");
        REQUIRE(loaded != nullptr);
        
        // Should have no intervals
        CHECK(loaded->size() == 0);
    }
    
    SECTION("Custom binary threshold") {
        // Use 0.7 as threshold - values 0.5 and 0.6 should be treated as off
        std::vector<std::vector<double>> data = {
            {0.0, 0.8},  // on (>= 0.7)
            {0.1, 0.9},  // on
            {0.2, 0.6},  // off (< 0.7)
            {0.3, 0.5},  // off
            {0.4, 0.7},  // on (= 0.7)
            {0.5, 1.0},  // on
        };
        
        auto csv_path = temp_dir.getFilePath("threshold.csv");
        writeBinaryStateCSV(csv_path.string(), 2, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "threshold_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 2},
                {"data_column", 1},
                {"delimiter", "\t"},
                {"binary_threshold", 0.7}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("threshold_test");
        REQUIRE(loaded != nullptr);
        
        // Should have 2 intervals: [0,1] and [4,5]
        REQUIRE(loaded->size() == 2);
        
        auto intervals = loaded->view();
        CHECK(intervals[0].value().start == 0);
        CHECK(intervals[0].value().end == 1);
        CHECK(intervals[1].value().start == 4);
        CHECK(intervals[1].value().end == 5);
    }
}

//=============================================================================
// Test Case 2: Multi-Column Batch Loading
//=============================================================================

TEST_CASE("DigitalInterval Binary State - Batch Loading All Columns", 
          "[digitalinterval][csv][binary_state][batch][integration][datamanager]") {
    TempBinaryStateTestDirectory temp_dir;
    
    SECTION("Load all data columns") {
        // Create test data: Time, v0, v1, v2
        // v0: on for first 5, off for rest
        // v1: alternating
        // v2: all zeros
        std::vector<std::vector<double>> data = {
            {0.0, 1.0, 1.0, 0.0},
            {0.1, 1.0, 0.0, 0.0},
            {0.2, 1.0, 1.0, 0.0},
            {0.3, 1.0, 0.0, 0.0},
            {0.4, 1.0, 1.0, 0.0},
            {0.5, 0.0, 0.0, 0.0},
            {0.6, 0.0, 1.0, 0.0},
            {0.7, 0.0, 0.0, 0.0},
        };
        
        auto csv_path = temp_dir.getFilePath("multi_column.csv");
        writeBinaryStateCSV(csv_path.string(), 3, {"Time", "v0", "v1", "v2"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "batch_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 3},
                {"all_columns", true},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should have loaded v0, v1, v2 (but not Time column)
        // Names are prefixed with the base name from config
        auto v0 = dm.getData<DigitalIntervalSeries>("batch_intervals_v0");
        auto v1 = dm.getData<DigitalIntervalSeries>("batch_intervals_v1");
        auto v2 = dm.getData<DigitalIntervalSeries>("batch_intervals_v2");
        
        REQUIRE(v0 != nullptr);
        REQUIRE(v1 != nullptr);
        REQUIRE(v2 != nullptr);
        
        // v0: one interval [0,4]
        REQUIRE(v0->size() == 1);
        CHECK(v0->view()[0].value().start == 0);
        CHECK(v0->view()[0].value().end == 4);
        
        // v1: four single-point intervals [0,0], [2,2], [4,4], [6,6]
        REQUIRE(v1->size() == 4);
        
        // v2: no intervals
        CHECK(v2->size() == 0);
    }
}

//=============================================================================
// Test Case 3: Real Test Data File (jun_test.dat)
//=============================================================================

TEST_CASE("DigitalInterval Binary State - Real Test Data (jun_test.dat)", 
          "[digitalinterval][csv][binary_state][integration][datamanager]") {
    
    std::string test_file = getJunTestDataPath();
    
    // Skip test if file doesn't exist
    if (!std::filesystem::exists(test_file)) {
        SKIP("Test file not found: " << test_file);
    }
    
    SECTION("Load single column v0 (all ones)") {
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "v0_intervals"},
                {"filepath", test_file},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 5},
                {"data_column", 1},  // v0 column
                {"delimiter", "\t"},
                {"binary_threshold", 0.5}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, ".");
        
        auto loaded = dm.getData<DigitalIntervalSeries>("v0_intervals");
        REQUIRE(loaded != nullptr);
        
        // v0 is all 1s in jun_test.dat, so should be one interval
        REQUIRE(loaded->size() == 1);
        
        // First interval should start at 0
        CHECK(loaded->view()[0].value().start == 0);
    }
    
    SECTION("Load column v1 (all zeros)") {
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "v1_intervals"},
                {"filepath", test_file},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 5},
                {"data_column", 2},  // v1 column
                {"delimiter", "\t"},
                {"binary_threshold", 0.5}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, ".");
        
        auto loaded = dm.getData<DigitalIntervalSeries>("v1_intervals");
        REQUIRE(loaded != nullptr);
        
        // v1 is all 0s in jun_test.dat, so should have no intervals
        CHECK(loaded->size() == 0);
    }
    
    SECTION("Batch load all columns from real file") {
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "all_channels"},
                {"filepath", test_file},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 5},
                {"all_columns", true},
                {"delimiter", "\t"},
                {"binary_threshold", 0.5}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, ".");
        
        // Should have loaded v0 (has data) - named from column header with prefix
        auto v0 = dm.getData<DigitalIntervalSeries>("all_channels_v0");
        REQUIRE(v0 != nullptr);
        REQUIRE(v0->size() >= 1);  // v0 is all 1s, so one interval
        
        // v1, v2, v3 should be loaded but have no intervals (all zeros)
        auto v1 = dm.getData<DigitalIntervalSeries>("all_channels_v1");
        REQUIRE(v1 != nullptr);
        CHECK(v1->size() == 0);
    }
}

//=============================================================================
// Test Case 4: Delimiter Variations
//=============================================================================

TEST_CASE("DigitalInterval Binary State - Delimiter Variations", 
          "[digitalinterval][csv][binary_state][integration][datamanager]") {
    TempBinaryStateTestDirectory temp_dir;
    
    std::vector<std::vector<double>> data = {
        {0.0, 1.0},
        {0.1, 1.0},
        {0.2, 0.0},
        {0.3, 1.0},
    };
    
    SECTION("Comma delimiter") {
        auto csv_path = temp_dir.getFilePath("comma.csv");
        writeBinaryStateCSV(csv_path.string(), 1, {"Time", "ch0"}, data, ",");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "comma_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 1},
                {"data_column", 1},
                {"delimiter", ","}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("comma_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->size() == 2);
    }
    
    SECTION("Space delimiter") {
        auto csv_path = temp_dir.getFilePath("space.csv");
        writeBinaryStateCSV(csv_path.string(), 1, {"Time", "ch0"}, data, " ");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "space_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 1},
                {"data_column", 1},
                {"delimiter", " "}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("space_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->size() == 2);
    }
}

//=============================================================================
// Test Case 5: Edge Cases
//=============================================================================

TEST_CASE("DigitalInterval Binary State - Edge Cases", 
          "[digitalinterval][csv][binary_state][integration][datamanager]") {
    TempBinaryStateTestDirectory temp_dir;
    
    SECTION("Single row - on") {
        std::vector<std::vector<double>> data = {{0.0, 1.0}};
        
        auto csv_path = temp_dir.getFilePath("single_on.csv");
        writeBinaryStateCSV(csv_path.string(), 0, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "single_on"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 0},
                {"data_column", 1},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("single_on");
        REQUIRE(loaded != nullptr);
        
        // Single row that is on should be one interval [0,0]
        REQUIRE(loaded->size() == 1);
        CHECK(loaded->view()[0].value().start == 0);
        CHECK(loaded->view()[0].value().end == 0);
    }
    
    SECTION("Single row - off") {
        std::vector<std::vector<double>> data = {{0.0, 0.0}};
        
        auto csv_path = temp_dir.getFilePath("single_off.csv");
        writeBinaryStateCSV(csv_path.string(), 0, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "single_off"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 0},
                {"data_column", 1},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("single_off");
        REQUIRE(loaded != nullptr);
        CHECK(loaded->size() == 0);
    }
    
    SECTION("Starts off, ends on") {
        std::vector<std::vector<double>> data = {
            {0.0, 0.0},
            {0.1, 0.0},
            {0.2, 1.0},
            {0.3, 1.0},
        };
        
        auto csv_path = temp_dir.getFilePath("ends_on.csv");
        writeBinaryStateCSV(csv_path.string(), 0, {"Time", "ch0"}, data, "\t");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "ends_on"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"csv_layout", "binary_state"},
                {"header_lines_to_skip", 0},
                {"data_column", 1},
                {"delimiter", "\t"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("ends_on");
        REQUIRE(loaded != nullptr);
        
        // Should have one interval [2,3]
        REQUIRE(loaded->size() == 1);
        CHECK(loaded->view()[0].value().start == 2);
        CHECK(loaded->view()[0].value().end == 3);
    }
}
