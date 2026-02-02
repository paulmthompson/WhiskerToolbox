/**
 * @file digital_interval_binary_integration.test.cpp
 * @brief Integration tests for loading DigitalIntervalSeries from binary files via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Single-channel uint16 TTL binary files
 * 2. Multiple channels in same file (different bit positions)
 * 3. Rising vs falling edge transition detection
 * 4. Binary files with headers
 * 5. Various edge cases (empty intervals, minimal pulses)
 * 
 * Uses builder-based scenarios to create test data, writes to temporary
 * binary files with TTL pulse patterns, then loads via DataManager JSON config.
 * 
 * Binary format: Each sample is uint16_t where each bit represents a TTL channel.
 * Intervals are detected by edge transitions (rising/falling) in the specified bit.
 */

#include <catch2/catch_test_macros.hpp>

#include "fixtures/scenarios/digital/binary_interval_loading_scenarios.hpp"
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"

#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using json = nlohmann::json;

/**
 * @brief Helper class for managing temporary test directories
 */
class TempBinaryIntervalTestDirectory {
public:
    TempBinaryIntervalTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_binary_interval_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempBinaryIntervalTestDirectory() {
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

namespace {
/**
 * @brief Helper to verify interval data equality between original and loaded data
 */
void verifyIntervalsEqualBinary(DigitalIntervalSeries const& original, 
                                DigitalIntervalSeries const& loaded) {
    REQUIRE(loaded.size() == original.size());
    
    auto original_view = original.view();
    auto loaded_view = loaded.view();
    
    for (size_t i = 0; i < original.size(); ++i) {
        REQUIRE(loaded_view[i].value().start == original_view[i].value().start);
        REQUIRE(loaded_view[i].value().end == original_view[i].value().end);
    }
}
} // anonymous namespace

//=============================================================================
// Test Case 1: Single-channel uint16 binary files with rising edge detection
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Single Channel Rising Edge", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Simple TTL pulses") {
        auto original = digital_interval_binary_scenarios::simple_ttl_pulses();
        
        auto binary_path = temp_dir.getFilePath("simple_pulses.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 200, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "simple_ttl"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("simple_ttl");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
    
    SECTION("Single pulse") {
        auto original = digital_interval_binary_scenarios::single_pulse();
        
        auto binary_path = temp_dir.getFilePath("single_pulse.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 100, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "single_ttl"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("single_ttl");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->size() == 1);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
    
    SECTION("Periodic pulses (timing signal)") {
        auto original = digital_interval_binary_scenarios::periodic_pulses();
        
        auto binary_path = temp_dir.getFilePath("periodic_pulses.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 250, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "periodic_ttl"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("periodic_ttl");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
    
    SECTION("Wide-spaced long pulses") {
        auto original = digital_interval_binary_scenarios::wide_spaced_pulses();
        
        auto binary_path = temp_dir.getFilePath("wide_spaced.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 1500, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "wide_ttl"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("wide_ttl");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
}

//=============================================================================
// Test Case 2: Different channel (bit position) extraction
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Multiple Channels", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Channel 0 and Channel 5 in same file") {
        auto intervals_ch0 = digital_interval_binary_scenarios::simple_ttl_pulses();
        auto intervals_ch5 = digital_interval_binary_scenarios::periodic_pulses();
        
        auto binary_path = temp_dir.getFilePath("multi_channel.bin");
        
        std::vector<std::pair<int, std::shared_ptr<DigitalIntervalSeries>>> channels = {
            {0, intervals_ch0},
            {5, intervals_ch5}
        };
        
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16MultiChannel(
            channels, binary_path.string(), 300));
        
        // Load channel 0
        json config_ch0 = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "ch0_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config_ch0, temp_dir.getPathString());
        
        auto loaded_ch0 = dm.getData<DigitalIntervalSeries>("ch0_intervals");
        REQUIRE(loaded_ch0 != nullptr);
        verifyIntervalsEqualBinary(*intervals_ch0, *loaded_ch0);
        
        // Load channel 5 in same DataManager
        json config_ch5 = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "ch5_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 5},
                {"transition", "rising"}
            }
        });
        
        load_data_from_json_config(&dm, config_ch5, temp_dir.getPathString());
        
        auto loaded_ch5 = dm.getData<DigitalIntervalSeries>("ch5_intervals");
        REQUIRE(loaded_ch5 != nullptr);
        verifyIntervalsEqualBinary(*intervals_ch5, *loaded_ch5);
    }
    
    SECTION("High bit channel (channel 15)") {
        auto original = digital_interval_binary_scenarios::simple_ttl_pulses();
        
        auto binary_path = temp_dir.getFilePath("high_channel.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 200, 15));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "ch15_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 15},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("ch15_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
}

//=============================================================================
// Test Case 3: Falling edge transition detection
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Falling Edge Detection", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Falling edge creates inverted intervals") {
        // With falling edge: interval STARTS at high->low (falling) and ENDS at low->high (rising)
        // For a signal with multiple pulses, falling edge detects the "gaps" between pulses
        //
        // Using simple_ttl_pulses which has pulses at [10,20], [50,60], [100,120]
        // The signal is: LOW(0-9), HIGH(10-19), LOW(20-49), HIGH(50-59), LOW(60-99), HIGH(100-119), LOW(120-end)
        //
        // With falling edge detection:
        // - Falling at sample 20, rising at sample 50 → interval [20, 50]
        // - Falling at sample 60, rising at sample 100 → interval [60, 100]
        // - Falling at sample 120, no subsequent rising → not emitted (incomplete)
        
        auto original = digital_interval_binary_scenarios::simple_ttl_pulses();  // [10,20], [50,60], [100,120]
        
        auto binary_path = temp_dir.getFilePath("falling_test.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 150, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "falling_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "falling"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("falling_intervals");
        REQUIRE(loaded != nullptr);
        
        // With falling edge detection, we get the "gaps" between pulses
        // From simple_ttl_pulses [10,20], [50,60], [100,120]:
        // Gap 1: [20, 50] (falling at 20, rising at 50)
        // Gap 2: [60, 100] (falling at 60, rising at 100)
        REQUIRE(loaded->size() == 2);
        
        auto view = loaded->view();
        REQUIRE(view[0].value().start == 20);
        REQUIRE(view[0].value().end == 50);
        REQUIRE(view[1].value().start == 60);
        REQUIRE(view[1].value().end == 100);
    }
}

//=============================================================================
// Test Case 4: Binary files with headers
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Files with Headers", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("256-byte header") {
        auto original = digital_interval_binary_scenarios::simple_ttl_pulses();
        
        auto binary_path = temp_dir.getFilePath("with_header.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 200, 0, 256));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "header_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"},
                {"header_size", 256}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("header_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
    
    SECTION("Small header (64 bytes)") {
        auto original = digital_interval_binary_scenarios::periodic_pulses();
        
        auto binary_path = temp_dir.getFilePath("small_header.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 250, 0, 64));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "small_header_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"},
                {"header_size", 64}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("small_header_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
}

//=============================================================================
// Test Case 5: Edge cases
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Edge Cases", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Minimal duration pulses (1 sample)") {
        auto original = digital_interval_binary_scenarios::minimal_pulses();
        
        auto binary_path = temp_dir.getFilePath("minimal_pulses.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 100, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "minimal_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("minimal_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
    
    SECTION("Empty file with no pulses (always low)") {
        auto original = digital_interval_binary_scenarios::no_pulses();
        
        auto binary_path = temp_dir.getFilePath("no_pulses.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 100, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "empty_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("empty_intervals");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->size() == 0);
    }
    
    SECTION("Adjacent pulses") {
        auto original = digital_interval_binary_scenarios::adjacent_pulses();
        
        auto binary_path = temp_dir.getFilePath("adjacent_pulses.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 100, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "adjacent_intervals"},
                {"filepath", binary_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("adjacent_intervals");
        REQUIRE(loaded != nullptr);
        
        // Adjacent pulses [5,15], [15,25], [25,35], [35,45] in binary representation 
        // become one continuous pulse since there's no gap between them 
        // (all bits stay high from sample 5 to 44)
        // Expected: single interval [5, 45]
        REQUIRE(loaded->size() == 1);
        
        auto view = loaded->view();
        REQUIRE(view[0].value().start == 5);
        REQUIRE(view[0].value().end == 45);
    }
    
    SECTION("Missing file handled gracefully") {
        auto fake_path = temp_dir.getFilePath("nonexistent.bin");
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "missing_file"},
                {"filepath", fake_path.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should fail gracefully
        auto loaded = dm.getData<DigitalIntervalSeries>("missing_file");
        REQUIRE(loaded == nullptr);
    }
}

//=============================================================================
// Test Case 6: Multiple interval series loading
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Multiple Series Loading", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Load multiple interval series from different files") {
        auto intervals1 = digital_interval_binary_scenarios::simple_ttl_pulses();
        auto intervals2 = digital_interval_binary_scenarios::periodic_pulses();
        auto intervals3 = digital_interval_binary_scenarios::single_pulse();
        
        auto path1 = temp_dir.getFilePath("intervals1.bin");
        auto path2 = temp_dir.getFilePath("intervals2.bin");
        auto path3 = temp_dir.getFilePath("intervals3.bin");
        
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(intervals1.get(), path1.string(), 200, 0));
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(intervals2.get(), path2.string(), 250, 0));
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(intervals3.get(), path3.string(), 100, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_1"},
                {"filepath", path1.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            },
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_2"},
                {"filepath", path2.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
            },
            {
                {"data_type", "digital_interval"},
                {"name", "intervals_set_3"},
                {"filepath", path3.string()},
                {"format", "uint16"},
                {"channel", 0},
                {"transition", "rising"}
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
        
        verifyIntervalsEqualBinary(*intervals1, *loaded1);
        verifyIntervalsEqualBinary(*intervals2, *loaded2);
        verifyIntervalsEqualBinary(*intervals3, *loaded3);
    }
}

//=============================================================================
// Test Case 7: Using "binary" format alias
//=============================================================================

TEST_CASE("DigitalInterval Binary Integration - Format Alias", 
          "[digitalinterval][binary][integration][datamanager]") {
    TempBinaryIntervalTestDirectory temp_dir;
    
    SECTION("Using 'binary' format string (alias for uint16)") {
        auto original = digital_interval_binary_scenarios::simple_ttl_pulses();
        
        auto binary_path = temp_dir.getFilePath("alias_test.bin");
        REQUIRE(digital_interval_binary_scenarios::writeBinaryUint16(
            original.get(), binary_path.string(), 200, 0));
        
        json config = json::array({
            {
                {"data_type", "digital_interval"},
                {"name", "alias_intervals"},
                {"filepath", binary_path.string()},
                {"format", "binary"},  // Using "binary" instead of "uint16"
                {"channel", 0},
                {"transition", "rising"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<DigitalIntervalSeries>("alias_intervals");
        REQUIRE(loaded != nullptr);
        
        verifyIntervalsEqualBinary(*original, *loaded);
    }
}
