/**
 * @file test_analog_binary_json_loading.cpp
 * @brief Tests for loading binary analog data via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Loading a single binary file
 * 2. Loading multiple binary files at once  
 * 3. Loading with memory mapping
 * 
 * Uses builder-based scenarios to create test data, writes to temporary
 * binary files, then loads via DataManager JSON config.
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
class TempBinaryTestDirectory {
public:
    TempBinaryTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_binary_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempBinaryTestDirectory() {
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
// Test Case 1: Loading a single binary file
//=============================================================================

TEST_CASE("Analog Binary JSON Loading - Single File", "[analog][binary][json][datamanager]") {
    TempBinaryTestDirectory temp_dir;
    
    SECTION("Simple ramp signal - int16 format") {
        // Create test data using scenario
        auto original = analog_scenarios::simple_ramp_100();
        
        // Write to binary file
        auto binary_path = temp_dir.getFilePath("ramp_signal.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Create JSON config
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "test_ramp"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        // Load via DataManager
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Get loaded data (note: channel suffix is appended)
        auto loaded = dm.getData<AnalogTimeSeries>("test_ramp_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify values match
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            // int16 round-trip should preserve integer values
            REQUIRE(loaded_samples[i].value() == static_cast<int16_t>(original_samples[i].value()));
        }
    }
    
    SECTION("Signal with header bytes") {
        auto original = analog_scenarios::constant_value_100();
        
        size_t const header_size = 256;
        auto binary_path = temp_dir.getFilePath("signal_with_header.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string(), header_size));
        
        // Create JSON config with header_size
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "header_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", static_cast<int>(header_size)}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("header_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify constant value is preserved
        auto loaded_samples = loaded->getAllSamples();
        for (auto const& sample : loaded_samples) {
            REQUIRE(sample.value() == 42.0f);
        }
    }
    
    SECTION("Sine wave signal for precision test") {
        auto original = analog_scenarios::sine_wave_1000_samples();
        
        auto binary_path = temp_dir.getFilePath("sine_wave.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "sine_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("sine_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 1000);
        
        // Verify sine wave shape is preserved (within int16 quantization)
        auto loaded_samples = loaded->getAllSamples();
        // Check that we have positive and negative values (sine wave characteristic)
        bool has_positive = false;
        bool has_negative = false;
        for (auto const& sample : loaded_samples) {
            if (sample.value() > 500.0f) has_positive = true;
            if (sample.value() < -500.0f) has_negative = true;
        }
        REQUIRE(has_positive);
        REQUIRE(has_negative);
    }
}

//=============================================================================
// Test Case 2: Loading multiple binary files simultaneously
//=============================================================================

TEST_CASE("Analog Binary JSON Loading - Multiple Files", "[analog][binary][json][datamanager]") {
    TempBinaryTestDirectory temp_dir;
    
    SECTION("Load two independent binary files") {
        // Create two different signals
        auto ramp = analog_scenarios::simple_ramp_100();
        auto constant = analog_scenarios::constant_value_100();
        
        // Write both to separate files
        auto ramp_path = temp_dir.getFilePath("ramp.bin");
        auto constant_path = temp_dir.getFilePath("constant.bin");
        
        REQUIRE(analog_scenarios::writeBinaryInt16(ramp.get(), ramp_path.string()));
        REQUIRE(analog_scenarios::writeBinaryInt16(constant.get(), constant_path.string()));
        
        // Create JSON config with both files
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "ramp_signal"},
                {"filepath", ramp_path.string()},
                {"format", "binary"},
                {"num_channels", 1}
            },
            {
                {"data_type", "analog"},
                {"name", "constant_signal"},
                {"filepath", constant_path.string()},
                {"format", "binary"},
                {"num_channels", 1}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Both should be loaded - verify by checking DataManager directly
        auto loaded_ramp = dm.getData<AnalogTimeSeries>("ramp_signal_0");
        auto loaded_constant = dm.getData<AnalogTimeSeries>("constant_signal_0");
        
        REQUIRE(loaded_ramp != nullptr);
        REQUIRE(loaded_constant != nullptr);
        
        REQUIRE(loaded_ramp->getNumSamples() == 100);
        REQUIRE(loaded_constant->getNumSamples() == 100);
        
        // Verify first file is ramp (increasing values)
        auto ramp_samples = loaded_ramp->getAllSamples();
        REQUIRE(ramp_samples[0].value() < ramp_samples[99].value());
        
        // Verify second file is constant
        auto constant_samples = loaded_constant->getAllSamples();
        REQUIRE(constant_samples[0].value() == constant_samples[50].value());
    }
    
    SECTION("Load multi-channel binary file") {
        // Create 2-channel interleaved data
        auto channels = analog_scenarios::two_channel_ramps();
        
        auto binary_path = temp_dir.getFilePath("multichannel.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(channels, binary_path.string()));
        
        // Load with num_channels = 2
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "multichannel"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 2}
            }
        });
        
        DataManager dm;
        auto result = load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Should create 2 data entries (multichannel_0 and multichannel_1)
        auto channel0 = dm.getData<AnalogTimeSeries>("multichannel_0");
        auto channel1 = dm.getData<AnalogTimeSeries>("multichannel_1");
        
        REQUIRE(channel0 != nullptr);
        REQUIRE(channel1 != nullptr);
        
        REQUIRE(channel0->getNumSamples() == 100);
        REQUIRE(channel1->getNumSamples() == 100);
        
        // Channel 0 should be increasing (0 -> 99)
        auto ch0_samples = channel0->getAllSamples();
        REQUIRE(ch0_samples[0].value() < ch0_samples[99].value());
        REQUIRE_THAT(ch0_samples[0].value(), WithinAbs(0.0f, 1.0f));
        
        // Channel 1 should be decreasing (99 -> 0)
        auto ch1_samples = channel1->getAllSamples();
        REQUIRE(ch1_samples[0].value() > ch1_samples[99].value());
        REQUIRE_THAT(ch1_samples[0].value(), WithinAbs(99.0f, 1.0f));
    }
    
    SECTION("Load 4-channel binary file") {
        auto channels = analog_scenarios::four_channel_constants();
        
        auto binary_path = temp_dir.getFilePath("four_channel.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(channels, binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "quad"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 4}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Verify all 4 channels loaded with correct constant values
        for (int ch = 0; ch < 4; ++ch) {
            std::string key = "quad_" + std::to_string(ch);
            auto channel = dm.getData<AnalogTimeSeries>(key);
            REQUIRE(channel != nullptr);
            REQUIRE(channel->getNumSamples() == 50);
            
            float expected_value = static_cast<float>((ch + 1) * 10);
            auto samples = channel->getAllSamples();
            REQUIRE_THAT(samples[0].value(), WithinAbs(expected_value, 1.0f));
        }
    }
}

//=============================================================================
// Test Case 3: Loading with memory mapping
//=============================================================================

TEST_CASE("Analog Binary JSON Loading - Memory Mapped", "[analog][binary][json][datamanager][mmap]") {
    TempBinaryTestDirectory temp_dir;
    
    SECTION("Memory mapped loading - single channel") {
        auto original = analog_scenarios::simple_ramp_100();
        
        auto binary_path = temp_dir.getFilePath("mmap_test.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        // Enable memory mapping via JSON config
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "mmap_signal"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("mmap_signal_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify data is correct
        auto loaded_samples = loaded->getAllSamples();
        REQUIRE_THAT(loaded_samples[0].value(), WithinAbs(0.0f, 1.0f));
        REQUIRE_THAT(loaded_samples[99].value(), WithinAbs(99.0f, 1.0f));
    }
    
    SECTION("Memory mapped loading - with scale factor") {
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
    
    SECTION("Memory mapped loading - with offset") {
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
    
    SECTION("Memory mapped loading - multi-channel with stride") {
        auto channels = analog_scenarios::two_channel_ramps();
        
        auto binary_path = temp_dir.getFilePath("mmap_multichannel.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(channels, binary_path.string()));
        
        // Load 2 channels with memory mapping
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "mmap_multi"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 2},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto ch0 = dm.getData<AnalogTimeSeries>("mmap_multi_0");
        auto ch1 = dm.getData<AnalogTimeSeries>("mmap_multi_1");
        
        REQUIRE(ch0 != nullptr);
        REQUIRE(ch1 != nullptr);
        
        // Verify correct deinterleaving
        auto ch0_samples = ch0->getAllSamples();
        auto ch1_samples = ch1->getAllSamples();
        
        // Channel 0: 0 -> 99 (increasing)
        REQUIRE(ch0_samples[0].value() < ch0_samples[ch0->getNumSamples() - 1].value());
        
        // Channel 1: 99 -> 0 (decreasing)
        REQUIRE(ch1_samples[0].value() > ch1_samples[ch1->getNumSamples() - 1].value());
    }
    
    SECTION("Memory mapped loading - with header") {
        auto original = analog_scenarios::simple_ramp_100();
        
        size_t const header_size = 512;
        auto binary_path = temp_dir.getFilePath("mmap_header.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string(), header_size));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "mmap_header_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"use_memory_mapped", true},
                {"binary_data_type", "int16"},
                {"header_size", static_cast<int>(header_size)}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("mmap_header_test_0");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify header was skipped correctly
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(0.0f, 1.0f));
        REQUIRE_THAT(samples[99].value(), WithinAbs(99.0f, 1.0f));
    }
}

//=============================================================================
// Test Case 4: Edge cases and error handling
//=============================================================================

TEST_CASE("Analog Binary JSON Loading - Edge Cases", "[analog][binary][json][datamanager]") {
    TempBinaryTestDirectory temp_dir;
    
    SECTION("Empty data array handles gracefully") {
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
