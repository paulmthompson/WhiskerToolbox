/**
 * @file analog_csv_integration.test.cpp
 * @brief Integration tests for loading AnalogTimeSeries from CSV via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Two-column CSV (time, value) with header
 * 2. Two-column CSV without header
 * 3. Single-column CSV (value only, time inferred)
 * 4. Custom delimiter (tab, semicolon)
 * 5. Reversed column order (data first, time second)
 * 6. Various edge cases (negative values, precision, etc.)
 * 
 * Uses builder-based scenarios to create test data, writes to temporary
 * CSV files, then loads via DataManager JSON config.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/analog/csv_loading_scenarios.hpp"
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <nlohmann/json.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using json = nlohmann::json;
using Catch::Matchers::WithinAbs;

/**
 * @brief Helper class for managing temporary test directories
 */
class TempCSVTestDirectory {
public:
    TempCSVTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_csv_analog_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempCSVTestDirectory() {
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

//=============================================================================
// Test Case 1: Two-column CSV with header
//=============================================================================

TEST_CASE("Analog CSV Integration - Two Column with Header", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Simple integer values") {
        // Create test data using scenario
        auto original = analog_scenarios::simple_integer_values();
        
        // Write to CSV file
        auto csv_path = temp_dir.getFilePath("integer_values.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        // Create JSON config
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "test_csv_analog"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        // Load via DataManager
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Get loaded data (registry system appends "_0" suffix for analog single-channel loads)
        auto loaded = dm.getData<AnalogTimeSeries>("test_csv_analog_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify values match
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE_THAT(loaded_samples[i].value(), WithinAbs(original_samples[i].value(), 0.001));
            REQUIRE(loaded_samples[i].time_frame_index.getValue() == 
                    original_samples[i].time_frame_index.getValue());
        }
    }
    
    SECTION("Precision test values") {
        auto original = analog_scenarios::precision_test_values();
        
        auto csv_path = temp_dir.getFilePath("precision_values.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string(), ",", true, "Time,Data", 6));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "precision_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("precision_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            // Allow small tolerance for floating point round-trip through text
            REQUIRE_THAT(loaded_samples[i].value(), WithinAbs(original_samples[i].value(), 0.0001));
        }
    }
    
    SECTION("Non-sequential time indices") {
        auto original = analog_scenarios::non_sequential_times();
        
        auto csv_path = temp_dir.getFilePath("non_sequential.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "non_seq_times"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("non_seq_times_0");
        REQUIRE(loaded != nullptr);
        
        // Verify time indices are preserved correctly
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        REQUIRE(loaded_samples[0].time_frame_index.getValue() == 5);
        REQUIRE(loaded_samples[1].time_frame_index.getValue() == 15);
        REQUIRE(loaded_samples[2].time_frame_index.getValue() == 100);
        REQUIRE(loaded_samples[3].time_frame_index.getValue() == 200);
    }
    
    SECTION("Negative values") {
        auto original = analog_scenarios::negative_values();
        
        auto csv_path = temp_dir.getFilePath("negative_values.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "negative_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("negative_test_0");
        REQUIRE(loaded != nullptr);
        
        auto loaded_samples = loaded->getAllSamples();
        REQUIRE_THAT(loaded_samples[0].value(), WithinAbs(-10.5f, 0.01));
        REQUIRE_THAT(loaded_samples[1].value(), WithinAbs(-5.25f, 0.01));
        REQUIRE_THAT(loaded_samples[2].value(), WithinAbs(0.0f, 0.01));
        REQUIRE_THAT(loaded_samples[3].value(), WithinAbs(5.25f, 0.01));
        REQUIRE_THAT(loaded_samples[4].value(), WithinAbs(10.5f, 0.01));
    }
}

//=============================================================================
// Test Case 2: Two-column CSV without header
//=============================================================================

TEST_CASE("Analog CSV Integration - Two Column without Header", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Standard two-column no header") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("no_header.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string(), ",", false));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "no_header_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", false},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("no_header_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE_THAT(loaded_samples[i].value(), WithinAbs(original_samples[i].value(), 0.001));
        }
    }
}

//=============================================================================
// Test Case 3: Single-column CSV (value only)
//=============================================================================

TEST_CASE("Analog CSV Integration - Single Column Format", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Single column without header") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("single_column.csv");
        REQUIRE(analog_scenarios::writeCSVSingleColumn(original.get(), csv_path.string(), false));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "single_col_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"single_column_format", true},
                {"has_header", false}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("single_col_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Values should match
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE_THAT(loaded_samples[i].value(), WithinAbs(original_samples[i].value(), 0.001));
        }
        
        // Time indices should be 0-indexed row numbers
        for (size_t i = 0; i < loaded->getNumSamples(); ++i) {
            REQUIRE(loaded_samples[i].time_frame_index.getValue() == static_cast<int64_t>(i));
        }
    }
    
    SECTION("Single column with header") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("single_column_header.csv");
        REQUIRE(analog_scenarios::writeCSVSingleColumn(original.get(), csv_path.string(), true, "Data"));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "single_col_header_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"single_column_format", true},
                {"has_header", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("single_col_header_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
    }
}

//=============================================================================
// Test Case 4: Custom delimiters
//=============================================================================

TEST_CASE("Analog CSV Integration - Custom Delimiters", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Tab-delimited") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("tab_delimited.tsv");
        REQUIRE(analog_scenarios::writeCSVWithDelimiter(original.get(), csv_path.string(), "\t"));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "tab_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", "\t"},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("tab_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
    }
    
    SECTION("Semicolon-delimited") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("semicolon_delimited.csv");
        REQUIRE(analog_scenarios::writeCSVWithDelimiter(original.get(), csv_path.string(), ";"));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "semicolon_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ";"},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("semicolon_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
    }
}

//=============================================================================
// Test Case 5: Reversed column order
//=============================================================================

TEST_CASE("Analog CSV Integration - Custom Column Order", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Data column first, time column second") {
        auto original = analog_scenarios::simple_integer_values();
        
        auto csv_path = temp_dir.getFilePath("reversed_columns.csv");
        REQUIRE(analog_scenarios::writeCSVReversedColumns(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "reversed_col_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 1},   // Time is in column 1
                {"data_column", 0}    // Data is in column 0
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("reversed_col_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE_THAT(loaded_samples[i].value(), WithinAbs(original_samples[i].value(), 0.001));
            REQUIRE(loaded_samples[i].time_frame_index.getValue() == 
                    original_samples[i].time_frame_index.getValue());
        }
    }
}

//=============================================================================
// Test Case 6: Larger data files
//=============================================================================

TEST_CASE("Analog CSV Integration - Large Data Files", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("500 sample ramp") {
        auto original = analog_scenarios::ramp_500_samples();
        
        auto csv_path = temp_dir.getFilePath("large_ramp.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "large_ramp"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("large_ramp_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 500);
        
        // Spot check first, middle, and last values
        auto loaded_samples = loaded->getAllSamples();
        REQUIRE_THAT(loaded_samples[0].value(), WithinAbs(0.0f, 0.001));
        REQUIRE_THAT(loaded_samples[249].value(), WithinAbs(249.0f, 0.001));
        REQUIRE_THAT(loaded_samples[499].value(), WithinAbs(499.0f, 0.001));
    }
}

//=============================================================================
// Test Case 7: Edge cases
//=============================================================================

TEST_CASE("Analog CSV Integration - Edge Cases", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Single sample") {
        auto original = analog_scenarios::single_sample();
        
        auto csv_path = temp_dir.getFilePath("single_sample.csv");
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original.get(), csv_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "single_sample_test"},
                {"filepath", csv_path.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("single_sample_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 1);
        
        auto loaded_samples = loaded->getAllSamples();
        REQUIRE_THAT(loaded_samples[0].value(), WithinAbs(42.5f, 0.01));
        REQUIRE(loaded_samples[0].time_frame_index.getValue() == 0);
    }
}

//=============================================================================
// Test Case 8: Error handling
//=============================================================================

TEST_CASE("Analog CSV Integration - Error Handling", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("DataManager handles missing CSV file gracefully") {
        // Create JSON config pointing to non-existent file
        std::filesystem::path fake_filepath = temp_dir.getPath() / "nonexistent.csv";
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "missing_csv_analog"},
                {"filepath", fake_filepath.string()},
                {"format", "csv"}
            }
        });
        
        // Create DataManager and attempt to load - should handle gracefully
        DataManager dm;
        auto data_info_list = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should return empty list due to missing file
        REQUIRE(data_info_list.empty());
        
        // Verify data was not loaded
        auto loaded = dm.getData<AnalogTimeSeries>("missing_csv_analog_0");
        REQUIRE(loaded == nullptr);
    }
    
    SECTION("Empty config array handles gracefully") {
        json config = json::array();
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        REQUIRE(result.empty());
    }
}

//=============================================================================
// Test Case 9: Loading multiple analog series from same config
//=============================================================================

TEST_CASE("Analog CSV Integration - Multiple Files", "[analog][csv][integration][datamanager]") {
    TempCSVTestDirectory temp_dir;
    
    SECTION("Load two separate CSV files in one config") {
        auto original1 = analog_scenarios::simple_integer_values();
        auto original2 = analog_scenarios::negative_values();
        
        auto csv_path1 = temp_dir.getFilePath("analog1.csv");
        auto csv_path2 = temp_dir.getFilePath("analog2.csv");
        
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original1.get(), csv_path1.string()));
        REQUIRE(analog_scenarios::writeCSVTwoColumn(original2.get(), csv_path2.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "analog_series_1"},
                {"filepath", csv_path1.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            },
            {
                {"data_type", "analog"},
                {"name", "analog_series_2"},
                {"filepath", csv_path2.string()},
                {"format", "csv"},
                {"delimiter", ","},
                {"has_header", true},
                {"single_column_format", false},
                {"time_column", 0},
                {"data_column", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded1 = dm.getData<AnalogTimeSeries>("analog_series_1_0");
        auto loaded2 = dm.getData<AnalogTimeSeries>("analog_series_2_0");
        
        REQUIRE(loaded1 != nullptr);
        REQUIRE(loaded2 != nullptr);
        REQUIRE(loaded1->getNumSamples() == original1->getNumSamples());
        REQUIRE(loaded2->getNumSamples() == original2->getNumSamples());
        
        // Verify first values
        auto samples1 = loaded1->getAllSamples();
        auto samples2 = loaded2->getAllSamples();
        
        REQUIRE_THAT(samples1[0].value(), WithinAbs(10.0f, 0.001));
        REQUIRE_THAT(samples2[0].value(), WithinAbs(-10.5f, 0.01));
    }
}
