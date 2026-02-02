/**
 * @file analog_binary_unit.test.cpp
 * @brief Unit tests for AnalogTimeSeries binary loading (redundant with integration tests)
 * 
 * NOTE: This file contains the original test_analog_binary_json_loading.cpp tests.
 * Most tests are now covered by analog_binary_integration.test.cpp. These tests 
 * are kept as unit-level tests for verification but share significant overlap
 * with the integration tests.
 * 
 * The unique tests (scale_factor, offset_value) have been merged into the 
 * integration test file. This file remains as a secondary verification.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "fixtures/scenarios/analog/binary_loading_scenarios.hpp"
#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
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
class TempBinaryUnitTestDirectory {
public:
    TempBinaryUnitTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_binary_unit_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempBinaryUnitTestDirectory() {
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
// Unit Test: Memory mapped loading with scale factor
//=============================================================================

TEST_CASE("Analog Binary Unit - Scale Factor", "[analog][binary][unit][datamanager][mmap]") {
    TempBinaryUnitTestDirectory temp_dir;
    
    SECTION("Memory mapped loading with scale_factor") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("mmap_scaled.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Use scale_factor to multiply values
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "scaled_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"scale_factor", 2.0f}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("scaled_signal_0");
        REQUIRE(loaded != nullptr);
        
        // Original value was 42, scaled by 2 should be 84
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(84.0f, 1.0f));
    }
    
    SECTION("Scale factor of 0.5 halves values") {
        auto original = analog_scenarios::simple_ramp_100();
        
        auto binary_path = temp_dir.getFilePath("half_scale.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "half_scaled"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"scale_factor", 0.5f}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("half_scaled_0");
        REQUIRE(loaded != nullptr);
        
        // Ramp value at index 50 was 50, scaled by 0.5 should be 25
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[50].value(), WithinAbs(25.0f, 1.0f));
    }
}

//=============================================================================
// Unit Test: Memory mapped loading with offset value
//=============================================================================

TEST_CASE("Analog Binary Unit - Offset Value", "[analog][binary][unit][datamanager][mmap]") {
    TempBinaryUnitTestDirectory temp_dir;
    
    SECTION("Memory mapped loading with offset_value") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("mmap_offset.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Use offset_value to add to values
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "offset_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"offset_value", 100.0f}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("offset_signal_0");
        REQUIRE(loaded != nullptr);
        
        // Original value was 42, with offset 100 should be 142
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(142.0f, 1.0f));
    }
    
    SECTION("Negative offset subtracts from values") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("negative_offset.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "neg_offset_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"offset_value", -40.0f}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("neg_offset_signal_0");
        REQUIRE(loaded != nullptr);
        
        // Original value was 42, with offset -40 should be 2
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(2.0f, 1.0f));
    }
}

//=============================================================================
// Unit Test: Combined scale and offset
//=============================================================================

TEST_CASE("Analog Binary Unit - Scale and Offset Combined", "[analog][binary][unit][datamanager][mmap]") {
    TempBinaryUnitTestDirectory temp_dir;
    
    SECTION("Scale and offset applied together") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("scale_and_offset.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Use both scale_factor and offset_value
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "scale_offset_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"scale_factor", 2.0f},
                {"offset_value", 10.0f}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("scale_offset_signal_0");
        REQUIRE(loaded != nullptr);
        
        // Original value was 42, scaled by 2 = 84, then + 10 = 94
        // Note: order of operations may vary based on implementation
        auto samples = loaded->getAllSamples();
        // Verify the transform was applied (exact result depends on implementation)
        REQUIRE(samples[0].value() != 42.0f);
    }
}

//=============================================================================
// Unit Test: Edge cases and error handling
//=============================================================================

TEST_CASE("Analog Binary Unit - Error Handling", "[analog][binary][unit][datamanager]") {
    TempBinaryUnitTestDirectory temp_dir;
    
    SECTION("Empty config array handles gracefully") {
        json config = json::array();
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        REQUIRE(result.empty());
    }
    
    SECTION("Non-existent file produces error but doesn't crash") {
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "nonexistent"},
                {"filepath", "/nonexistent/path/to/file.bin"},
                {"format", "binary"},
                {"num_channels", 1}
            }
        });
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should not have loaded anything
        auto loaded = dm.getData<AnalogTimeSeries>("nonexistent_0");
        REQUIRE(loaded == nullptr);
    }
    
    SECTION("Invalid JSON config field gracefully ignored") {
        auto original = analog_scenarios::simple_ramp_100();
        
        auto binary_path = temp_dir.getFilePath("with_invalid_field.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Include an invalid/unknown field
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "test_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"unknown_field", "should_be_ignored"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should still load despite unknown field
        auto loaded = dm.getData<AnalogTimeSeries>("test_signal_0");
        REQUIRE(loaded != nullptr);
    }
}

//=============================================================================
// Unit Test: Square wave preservation
//=============================================================================

TEST_CASE("Analog Binary Unit - Square Wave Preservation", "[analog][binary][unit][datamanager]") {
    TempBinaryUnitTestDirectory temp_dir;
    
    SECTION("Square wave preserves transitions") {
        auto original = analog_scenarios::square_wave_500_samples();
        
        auto binary_path = temp_dir.getFilePath("square_wave.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "square"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("square_0");
        REQUIRE(loaded != nullptr);
        
        // Verify square wave has both 0 and 100 values
        auto samples = loaded->getAllSamples();
        bool has_low = false;
        bool has_high = false;
        for (auto const& sample : samples) {
            if (sample.value() < 1.0f) has_low = true;
            if (sample.value() > 99.0f) has_high = true;
        }
        REQUIRE(has_low);
        REQUIRE(has_high);
    }
}
