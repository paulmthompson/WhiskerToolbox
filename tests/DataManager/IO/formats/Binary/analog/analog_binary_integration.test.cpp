/**
 * @file analog_binary_integration.test.cpp
 * @brief Integration tests for loading AnalogTimeSeries from binary files via DataManager JSON config
 * 
 * Tests the following scenarios:
 * 1. Single-channel binary files with various data types (int16, float32)
 * 2. Multi-channel interleaved binary files
 * 3. Binary files with headers
 * 4. Memory-mapped loading option
 * 5. Various edge cases
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
class TempBinaryAnalogTestDirectory {
public:
    TempBinaryAnalogTestDirectory() {
        temp_path = std::filesystem::temp_directory_path() / 
            ("whiskertoolbox_binary_analog_test_" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(temp_path);
    }
    
    ~TempBinaryAnalogTestDirectory() {
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
// Test Case 1: Single-channel int16 binary files
//=============================================================================

TEST_CASE("Analog Binary Integration - Single Channel Int16", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Simple ramp signal") {
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
        
        // Get loaded data (note: channel suffix is appended for multi-channel support)
        auto loaded = dm.getData<AnalogTimeSeries>("test_ramp");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify values match (int16 round-trip preserves integer values)
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE(loaded_samples[i].value() == static_cast<int16_t>(original_samples[i].value()));
        }
    }
    
    SECTION("Constant value signal") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("constant_signal.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "constant_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("constant_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // All values should be 42
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
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("sine_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 1000);
        
        // Verify sine wave shape (int16 truncation)
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < 100; ++i) {  // Spot check first 100 samples
            int16_t expected = static_cast<int16_t>(original_samples[i].value());
            REQUIRE(loaded_samples[i].value() == static_cast<float>(expected));
        }
    }
    
    SECTION("Square wave signal") {
        auto original = analog_scenarios::square_wave_500_samples();
        
        auto binary_path = temp_dir.getFilePath("square_wave.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "square_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("square_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 500);
    }
}

//=============================================================================
// Test Case 2: Binary files with headers
//=============================================================================

TEST_CASE("Analog Binary Integration - Files with Headers", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("256-byte header") {
        auto original = analog_scenarios::constant_value_100();
        
        size_t const header_size = 256;
        auto binary_path = temp_dir.getFilePath("signal_with_header.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string(), header_size));
        
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
        
        auto loaded = dm.getData<AnalogTimeSeries>("header_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify constant value is preserved despite header
        auto loaded_samples = loaded->getAllSamples();
        for (auto const& sample : loaded_samples) {
            REQUIRE(sample.value() == 42.0f);
        }
    }
    
    SECTION("512-byte header") {
        auto original = analog_scenarios::simple_ramp_100();
        
        size_t const header_size = 512;
        auto binary_path = temp_dir.getFilePath("signal_512_header.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string(), header_size));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "header_512_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 512}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("header_512_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
    }
}

//=============================================================================
// Test Case 3: Multi-channel binary files
//=============================================================================

TEST_CASE("Analog Binary Integration - Multi-Channel Files", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Two channels - ramp and inverted ramp") {
        auto signals = analog_scenarios::two_channel_ramps();
        
        auto binary_path = temp_dir.getFilePath("two_channel.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "multichannel"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 2},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // Both channels should be loaded with suffixes _0 and _1
        auto loaded_ch0 = dm.getData<AnalogTimeSeries>("multichannel_0");
        auto loaded_ch1 = dm.getData<AnalogTimeSeries>("multichannel_1");
        
        REQUIRE(loaded_ch0 != nullptr);
        REQUIRE(loaded_ch1 != nullptr);
        REQUIRE(loaded_ch0->getNumSamples() == signals[0]->getNumSamples());
        REQUIRE(loaded_ch1->getNumSamples() == signals[1]->getNumSamples());
        
        // Verify channel 0 is ascending ramp
        auto samples_ch0 = loaded_ch0->getAllSamples();
        REQUIRE(samples_ch0[0].value() == 0.0f);
        REQUIRE(samples_ch0[99].value() == 99.0f);
        
        // Verify channel 1 is descending ramp
        auto samples_ch1 = loaded_ch1->getAllSamples();
        REQUIRE(samples_ch1[0].value() == 99.0f);
        REQUIRE(samples_ch1[99].value() == 0.0f);
    }
    
    SECTION("Four channels - constant values") {
        auto signals = analog_scenarios::four_channel_constants();
        
        auto binary_path = temp_dir.getFilePath("four_channel.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16MultiChannel(signals, binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "quad_channel"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 4},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        // All four channels should be loaded
        for (int ch = 0; ch < 4; ++ch) {
            std::string key = "quad_channel_" + std::to_string(ch);
            auto loaded = dm.getData<AnalogTimeSeries>(key);
            REQUIRE(loaded != nullptr);
            REQUIRE(loaded->getNumSamples() == 50);
            
            // Verify constant value for each channel
            float expected_value = 10.0f * (ch + 1);  // 10, 20, 30, 40
            auto samples = loaded->getAllSamples();
            for (auto const& sample : samples) {
                REQUIRE(sample.value() == expected_value);
            }
        }
    }
}

//=============================================================================
// Test Case 4: Float32 binary format (using memory-mapped loading)
// Note: In-memory loading currently only supports int16 format.
// Float32 requires memory-mapped loading.
//=============================================================================

TEST_CASE("Analog Binary Integration - Float32 Format", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Float32 ramp signal with memory mapping") {
        auto original = analog_scenarios::simple_ramp_100();
        
        auto binary_path = temp_dir.getFilePath("float32_ramp.bin");
        REQUIRE(analog_scenarios::writeBinaryFloat32(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "float32_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0},
                {"binary_data_type", "float32"},
                {"use_memory_mapped", true}  // Float32 requires memory-mapped loading
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("float32_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Float32 should preserve exact values
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        for (size_t i = 0; i < original->getNumSamples(); ++i) {
            REQUIRE(loaded_samples[i].value() == original_samples[i].value());
        }
    }
    
    SECTION("Float32 sine wave with memory mapping") {
        auto original = analog_scenarios::sine_wave_1000_samples();
        
        auto binary_path = temp_dir.getFilePath("float32_sine.bin");
        REQUIRE(analog_scenarios::writeBinaryFloat32(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "float32_sine"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0},
                {"binary_data_type", "float32"},
                {"use_memory_mapped", true}  // Float32 requires memory-mapped loading
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("float32_sine");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == 1000);
        
        // Spot check some values match exactly
        auto original_samples = original->getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        
        REQUIRE(loaded_samples[0].value() == original_samples[0].value());
        REQUIRE(loaded_samples[500].value() == original_samples[500].value());
        REQUIRE(loaded_samples[999].value() == original_samples[999].value());
    }
}

//=============================================================================
// Test Case 5: Memory-mapped loading
//=============================================================================

TEST_CASE("Analog Binary Integration - Memory Mapped Loading", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Memory-mapped int16 file") {
        auto original = analog_scenarios::sine_wave_1000_samples();
        
        auto binary_path = temp_dir.getFilePath("mmap_signal.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "mmap_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0},
                {"use_memory_mapped", true}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("mmap_test");
        REQUIRE(loaded != nullptr);
        REQUIRE(loaded->getNumSamples() == original->getNumSamples());
        
        // Verify data is accessible through memory-mapped storage
        auto loaded_samples = loaded->getAllSamples();
        auto original_samples = original->getAllSamples();
        
        for (size_t i = 0; i < 100; ++i) {  // Spot check
            int16_t expected = static_cast<int16_t>(original_samples[i].value());
            REQUIRE(loaded_samples[i].value() == static_cast<float>(expected));
        }
    }
}

//=============================================================================
// Test Case 6: Loading multiple binary files in one config
//=============================================================================

TEST_CASE("Analog Binary Integration - Multiple Files", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Load two separate binary files") {
        auto original1 = analog_scenarios::simple_ramp_100();
        auto original2 = analog_scenarios::constant_value_100();
        
        auto binary_path1 = temp_dir.getFilePath("analog1.bin");
        auto binary_path2 = temp_dir.getFilePath("analog2.bin");
        
        REQUIRE(analog_scenarios::writeBinaryInt16(original1.get(), binary_path1.string()));
        REQUIRE(analog_scenarios::writeBinaryInt16(original2.get(), binary_path2.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "ramp_signal"},
                {"filepath", binary_path1.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            },
            {
                {"data_type", "analog"},
                {"name", "constant_signal"},
                {"filepath", binary_path2.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded1 = dm.getData<AnalogTimeSeries>("ramp_signal");
        auto loaded2 = dm.getData<AnalogTimeSeries>("constant_signal");
        
        REQUIRE(loaded1 != nullptr);
        REQUIRE(loaded2 != nullptr);
        REQUIRE(loaded1->getNumSamples() == original1->getNumSamples());
        REQUIRE(loaded2->getNumSamples() == original2->getNumSamples());
        
        // Verify first file is ramp
        auto samples1 = loaded1->getAllSamples();
        REQUIRE(samples1[0].value() == 0.0f);
        REQUIRE(samples1[50].value() == 50.0f);
        
        // Verify second file is constant
        auto samples2 = loaded2->getAllSamples();
        REQUIRE(samples2[0].value() == 42.0f);
        REQUIRE(samples2[50].value() == 42.0f);
    }
}

//=============================================================================
// Test Case 7: Verify time indices are correctly assigned
//=============================================================================

TEST_CASE("Analog Binary Integration - Time Index Assignment", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Sequential time indices from 0") {
        auto original = analog_scenarios::simple_ramp_100();
        
        auto binary_path = temp_dir.getFilePath("time_index_test.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "time_test"},
                {"filepath", binary_path.string()},
                {"format", "binary"},
                {"num_channels", 1},
                {"header_size", 0}
            }
        });
        
        DataManager dm;
        load_data_from_json_config(&dm, config, temp_dir.getPathString());
        
        auto loaded = dm.getData<AnalogTimeSeries>("time_test");
        REQUIRE(loaded != nullptr);
        
        // Time indices should be 0, 1, 2, ... n-1
        auto loaded_samples = loaded->getAllSamples();
        for (size_t i = 0; i < loaded->getNumSamples(); ++i) {
            REQUIRE(loaded_samples[i].time_frame_index.getValue() == static_cast<int64_t>(i));
        }
    }
}

//=============================================================================
// Test Case 8: Scale factor and offset value (memory-mapped)
//=============================================================================

TEST_CASE("Analog Binary Integration - Scale and Offset", "[analog][binary][integration][datamanager][mmap]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
    SECTION("Scale factor doubles values") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("scale_test.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
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
        
        auto loaded = dm.getData<AnalogTimeSeries>("scaled_signal");
        REQUIRE(loaded != nullptr);
        
        // Original constant value was 42, scaled by 2 should be 84
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(84.0f, 1.0f));
    }
    
    SECTION("Offset value adds to values") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("offset_test.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
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
        
        auto loaded = dm.getData<AnalogTimeSeries>("offset_signal");
        REQUIRE(loaded != nullptr);
        
        // Original constant value was 42, with offset 100 should be 142
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(142.0f, 1.0f));
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
        
        auto loaded = dm.getData<AnalogTimeSeries>("half_scaled");
        REQUIRE(loaded != nullptr);
        
        // Ramp value at index 50 was 50, scaled by 0.5 should be 25
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[50].value(), WithinAbs(25.0f, 1.0f));
    }
    
    SECTION("Negative offset subtracts from values") {
        auto original = analog_scenarios::constant_value_100();
        
        auto binary_path = temp_dir.getFilePath("negative_offset.bin");
        REQUIRE(analog_scenarios::writeBinaryInt16(original.get(), binary_path.string()));
        
        json config = json::array({
            {
                {"data_type", "analog"},
                {"name", "neg_offset"},
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
        
        auto loaded = dm.getData<AnalogTimeSeries>("neg_offset");
        REQUIRE(loaded != nullptr);
        
        // Original value was 42, with offset -40 should be 2
        auto samples = loaded->getAllSamples();
        REQUIRE_THAT(samples[0].value(), WithinAbs(2.0f, 1.0f));
    }
}

//=============================================================================
// Test Case 9: Error handling
//=============================================================================

TEST_CASE("Analog Binary Integration - Error Handling", "[analog][binary][integration][datamanager]") {
    TempBinaryAnalogTestDirectory temp_dir;
    
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
}
