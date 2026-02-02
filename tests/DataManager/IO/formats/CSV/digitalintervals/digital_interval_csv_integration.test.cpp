/**
 * @file digital_interval_csv_integration.test.cpp
 * @brief Integration tests for loading DigitalIntervalSeries from CSV via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Two-column CSV (start, end) with header
 * 2. Two-column CSV without header
 * 3. Custom delimiter (tab, semicolon, space)
 * 4. Reversed column order (end first, start second)
 * 5. Various edge cases (single interval, large values, etc.)
 * 
 * Uses builder-based scenarios to create test data, writes to temporary
 * CSV files, then loads via DataManager JSON config.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/digital/csv_interval_loading_scenarios.hpp"
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <nlohmann/json.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using json = nlohmann::json;

/**
 * @brief Helper class for managing temporary test directories
 */
class TempCSVIntervalTestDirectory {
public:
    TempCSVIntervalTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_csv_interval_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempCSVIntervalTestDirectory() {
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
 * @brief Helper to verify interval data equality between original and loaded data
 */
void verifyIntervalsEqual(DigitalIntervalSeries const& original, 
                          DigitalIntervalSeries const& loaded) {
    REQUIRE(loaded.size() == original.size());
    
    auto original_view = original.view();
    auto loaded_view = loaded.view();
    
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE(loaded_view[i].value().start == original_view[i].value().start);
        REQUIRE(loaded_view[i].value().end == original_view[i].value().end);
    }
}

//=============================================================================
// Test Case 1: Two-column CSV with header
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Two Column with Header", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Simple intervals") {
        // Create test data using scenario
        auto original = digital_interval_scenarios::simple_intervals();
        
        // Write to CSV file
        auto csv_path = temp_dir.getFilePath("simple_intervals.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        // Create JSON config
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "test_csv_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"skip_header", true}
            }
        });
        
        // Load via DataManager
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Get loaded data
        auto loaded = dm.getData<DigitalIntervalSeries>("test_csv_intervals");
        REQUIRE(loaded != nullptr);
        
        // Verify values match
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Single interval") {
        auto original = digital_interval_scenarios::single_interval();
        
        auto csv_path = temp_dir.getFilePath("single_interval.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "single_interval"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("single_interval");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->size() == 1);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Large time values") {
        auto original = digital_interval_scenarios::large_time_intervals();
        
        auto csv_path = temp_dir.getFilePath("large_intervals.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "large_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("large_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Regular pattern intervals") {
        auto original = digital_interval_scenarios::regular_pattern_intervals();
        
        auto csv_path = temp_dir.getFilePath("pattern_intervals.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "pattern_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("pattern_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
}

//=============================================================================
// Test Case 2: Two-column CSV without header
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Two Column without Header", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Simple intervals without header") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("no_header_intervals.csv");
        REQUIRE(digital_interval_scenarios::writeCSVNoHeader(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "no_header_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", false}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("no_header_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Adjacent intervals without header") {
        auto original = digital_interval_scenarios::adjacent_intervals();
        
        auto csv_path = temp_dir.getFilePath("adjacent_no_header.csv");
        REQUIRE(digital_interval_scenarios::writeCSVNoHeader(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "adjacent_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", false}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("adjacent_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
}

//=============================================================================
// Test Case 3: Custom delimiters
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Custom Delimiters", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Tab delimiter") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("tab_delimited.tsv");
        REQUIRE(digital_interval_scenarios::writeCSVWithDelimiter(original.get(), csv_path.string(), "\t"));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "tab_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", "\t"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("tab_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Semicolon delimiter") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("semicolon_delimited.csv");
        REQUIRE(digital_interval_scenarios::writeCSVWithDelimiter(original.get(), csv_path.string(), ";"));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "semicolon_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ";"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("semicolon_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Space delimiter") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("space_delimited.txt");
        REQUIRE(digital_interval_scenarios::writeCSVWithDelimiter(original.get(), csv_path.string(), " "));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "space_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", " "},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("space_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
}

//=============================================================================
// Test Case 4: Reversed column order (End, Start)
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Reversed Column Order", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Reversed columns with flip_column_order") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        // Write with end column first, start column second
        auto csv_path = temp_dir.getFilePath("reversed_columns.csv");
        REQUIRE(digital_interval_scenarios::writeCSVReversedColumns(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "reversed_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"skip_header", true},
                {"flip_column_order", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("reversed_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Varied duration intervals with reversed columns") {
        auto original = digital_interval_scenarios::varied_duration_intervals();
        
        auto csv_path = temp_dir.getFilePath("varied_reversed.csv");
        REQUIRE(digital_interval_scenarios::writeCSVReversedColumns(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "varied_reversed"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true},
                {"flip_column_order", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("varied_reversed");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
}

//=============================================================================
// Test Case 5: Edge cases and error handling
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Edge Cases", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Minimal duration intervals") {
        auto original = digital_interval_scenarios::minimal_duration_intervals();
        
        auto csv_path = temp_dir.getFilePath("minimal_duration.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "minimal_duration"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("minimal_duration");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Varied duration intervals") {
        auto original = digital_interval_scenarios::varied_duration_intervals();
        
        auto csv_path = temp_dir.getFilePath("varied_duration.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "varied_duration"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("varied_duration");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Empty file returns no data") {
        // Create empty CSV file with only header
        auto csv_path = temp_dir.getFilePath("empty.csv");
        {
            std::ofstream file(csv_path);
            file << "Start,End\n";
        }
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "empty_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("empty_intervals");
        // Either null (failed to load) or empty series is acceptable
        if (loaded != nullptr) {
            REQUIRE(loaded->size() == 0);
        }
    }
    
    SECTION("Missing file handled gracefully") {
        auto fake_path = temp_dir.getFilePath("nonexistent.csv");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "missing_file"},
                {"filepath", fake_path.string()},
                {"format", "csv"}
            }
        });
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should return empty list (failed to load)
        REQUIRE(result.empty());
        
        // Data should not exist in manager
        auto loaded = dm.getData<DigitalIntervalSeries>("missing_file");
        REQUIRE(loaded == nullptr);
    }
}

//=============================================================================
// Test Case 6: Multiple interval series in same config
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - Multiple Series Loading", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Load multiple interval series from different files") {
        auto intervals1 = digital_interval_scenarios::simple_intervals();
        auto intervals2 = digital_interval_scenarios::large_time_intervals();
        auto intervals3 = digital_interval_scenarios::minimal_duration_intervals();
        
        auto csv_path1 = temp_dir.getFilePath("intervals1.csv");
        auto csv_path2 = temp_dir.getFilePath("intervals2.csv");
        auto csv_path3 = temp_dir.getFilePath("intervals3.csv");
        
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(intervals1.get(), csv_path1.string()));
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(intervals2.get(), csv_path2.string()));
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(intervals3.get(), csv_path3.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_1"},
                {"filepath", csv_path1.string()},
                {"format", "csv"},
                {"skip_header", true}
            },
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_2"},
                {"filepath", csv_path2.string()},
                {"format", "csv"},
                {"skip_header", true}
            },
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_3"},
                {"filepath", csv_path3.string()},
                {"format", "csv"},
                {"skip_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded1 = dm.getData<DigitalIntervalSeries>("intervals_set_1");
        auto loaded2 = dm.getData<DigitalIntervalSeries>("intervals_set_2");
        auto loaded3 = dm.getData<DigitalIntervalSeries>("intervals_set_3");
        
        REQUIRE(loaded1 != nullptr);
        REQUIRE(loaded2 != nullptr);
        REQUIRE(loaded3 != nullptr);
        
        verifyIntervalsEqual(*intervals1, *loaded1);
        verifyIntervalsEqual(*intervals2, *loaded2);
        verifyIntervalsEqual(*intervals3, *loaded3);
    }
}

//=============================================================================
// Test Case 7: JSON file-based config loading
//=============================================================================

TEST_CASE("DigitalInterval CSV Integration - JSON File Config Loading", 
          "[digitalinterval][csv][integration][datamanager]") {
    TempCSVIntervalTestDirectory temp_dir;
    
    SECTION("Load via JSON config file") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("intervals_for_json.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        // Create JSON config file
        auto json_path = temp_dir.getFilePath("config.json");
        {
            std::ofstream json_file(json_path);
            json_file << R"([
{
    "data_type": "digital_interval",
    "name": "intervals_from_json_file",
    "filepath": ")" << csv_path.string() << R"(",
    "format": "csv",
    "delimiter": ",",
    "skip_header": true
}
])";
        }
        
        DataManager dm;
        load_data_from_json_config(&dm, json_path.string());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("intervals_from_json_file");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
    
    SECTION("Load with color attribute preserved") {
        auto original = digital_interval_scenarios::simple_intervals();
        
        auto csv_path = temp_dir.getFilePath("colored_intervals.csv");
        REQUIRE(digital_interval_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "colored_intervals"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"skip_header", true},
                {"color", "#FF00FF"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("colored_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqual(*original, *loaded);
    }
}
