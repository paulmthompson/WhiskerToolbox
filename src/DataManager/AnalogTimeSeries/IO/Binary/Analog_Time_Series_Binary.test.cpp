#include "Analog_Time_Series_Binary.hpp"
#include "DataManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TEST_CASE("BinaryAnalogLoader - Single channel int16", "[analog][binary][loader]") {
    // Create test data using builder
    auto builder = AnalogTimeSeriesBuilder()
        .withRamp(0, 99, 0.0f, 99.0f);
    
    // Write to temporary file
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_single_channel.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string()));
    
    // Load using BinaryAnalogLoaderOptions
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 1;
    opts.header_size = 0;
    opts.binary_data_type = "int16";
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 1);
    REQUIRE(series_list[0]->getNumSamples() == 100);
    
    // Verify data
    auto const& data = series_list[0]->getAnalogTimeSeries();
    REQUIRE(data.size() == 100);
    REQUIRE(data[0] == Catch::Approx(0.0f));
    REQUIRE(data[99] == Catch::Approx(99.0f));
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - Multiple channels int16", "[analog][binary][loader][multichannel]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withConstant(100.0f, 0, 49, 1);
    
    // Write 3 interleaved channels
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_multi_channel.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string(), 0, 3));
    
    // Load using BinaryAnalogLoaderOptions
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 3;
    opts.header_size = 0;
    opts.binary_data_type = "int16";
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 3);
    
    // Verify each channel
    for (size_t ch = 0; ch < 3; ++ch) {
        REQUIRE(series_list[ch]->getNumSamples() == 50);
        auto const& data = series_list[ch]->getAnalogTimeSeries();
        REQUIRE(data.size() == 50);
        // Each channel should have value offset by channel number
        REQUIRE(data[0] == Catch::Approx(100.0f + static_cast<float>(ch)));
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - With header", "[analog][binary][loader][header]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withSineWave(0, 99, 0.01f, 50.0f);
    
    // Write with 256-byte header
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_with_header.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string(), 256, 1));
    
    // Load with header size specified
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 1;
    opts.header_size = 256;
    opts.binary_data_type = "int16";
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 1);
    REQUIRE(series_list[0]->getNumSamples() == 100);
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - Memory mapped single channel", "[analog][binary][loader][mmap]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withRamp(0, 199, 0.0f, 199.0f);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_single.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string()));
    
    // Load with memory mapping
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 1;
    opts.header_size = 0;
    opts.binary_data_type = "int16";
    opts.use_memory_mapped = true;
    opts.num_samples = 200;
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 1);
    REQUIRE(series_list[0]->getNumSamples() == 200);
    
    // Verify data access works
    auto samples = series_list[0]->getAllSamples();
    int count = 0;
    for (auto const& [time, value] : samples) {
        REQUIRE(value == Catch::Approx(static_cast<float>(count)));
        count++;
    }
    REQUIRE(count == 200);
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - Memory mapped multiple channels", "[analog][binary][loader][mmap][multichannel]") {
    // Create test data with 2 channels
    auto builder = AnalogTimeSeriesBuilder()
        .withConstant(50.0f, 0, 99, 1);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_mmap_multi.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string(), 0, 2));
    
    // Load with memory mapping
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 2;
    opts.header_size = 0;
    opts.binary_data_type = "int16";
    opts.use_memory_mapped = true;
    opts.num_samples = 100;
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 2);
    
    // Verify each channel
    for (size_t ch = 0; ch < 2; ++ch) {
        REQUIRE(series_list[ch]->getNumSamples() == 100);
        
        auto samples = series_list[ch]->getAllSamples();
        int count = 0;
        for (auto const& [time, value] : samples) {
            REQUIRE(value == Catch::Approx(50.0f + static_cast<float>(ch)));
            count++;
        }
        REQUIRE(count == 100);
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - DataManager JSON loading single channel", "[analog][binary][datamanager][json]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withTriangleWave(0, 99, 100.0f);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_dm_single.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string()));
    
    // Create JSON configuration matching DataManager expectations
    // Note: "data_type" and "filepath" are DataManager-level fields
    // Other fields are passed to BinaryAnalogLoaderOptions via reflect-cpp
    // Do NOT include a nested "data_type" field as it conflicts with the binary data type
    json config = json::array();
    config.push_back({
        {"data_type", "analog"},  // DataManager level - identifies this as analog data
        {"name", "test_analog"},
        {"filepath", temp_file.string()},  // DataManager level - will be passed as filename
        {"format", "int16"},  // Identifies the binary format/loader to use
        {"num_channels", 1},
        {"header_size", 0}
    });
    
    // Load through DataManager
    DataManager dm;
    auto data_info = load_data_from_json_config(&dm, config, temp_file.parent_path().string());
    
    // Verify data was loaded
    auto keys = dm.getKeys<AnalogTimeSeries>();
    REQUIRE(keys.size() == 1);
    REQUIRE(keys[0] == "test_analog_0");
    
    auto series = dm.getData<AnalogTimeSeries>("test_analog_0");
    REQUIRE(series != nullptr);
    REQUIRE(series->getNumSamples() == 100);
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - DataManager JSON loading multiple channels", "[analog][binary][datamanager][json][multichannel]") {
    // Create test data with 3 channels
    auto builder = AnalogTimeSeriesBuilder()
        .withConstant(75.0f, 0, 49, 1);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_dm_multi.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string(), 0, 3));
    
    // Create JSON configuration with CORRECT field names
    json config = json::array();
    config.push_back({
        {"data_type", "analog"},
        {"name", "multi_analog"},
        {"filepath", temp_file.string()},
        {"format", "int16"},
        {"num_channels", 3},
        {"header_size", 0}
    });
    
    // Load through DataManager
    DataManager dm;
    auto data_info = load_data_from_json_config(&dm, config, temp_file.parent_path().string());
    
    // Verify all 3 channels were loaded
    auto keys = dm.getKeys<AnalogTimeSeries>();
    REQUIRE(keys.size() == 3);
    
    // Check channel naming: multi_analog_0, multi_analog_1, multi_analog_2
    REQUIRE(std::find(keys.begin(), keys.end(), "multi_analog_0") != keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "multi_analog_1") != keys.end());
    REQUIRE(std::find(keys.begin(), keys.end(), "multi_analog_2") != keys.end());
    
    // Verify each channel
    for (size_t ch = 0; ch < 3; ++ch) {
        std::string key = "multi_analog_" + std::to_string(ch);
        auto series = dm.getData<AnalogTimeSeries>(key);
        REQUIRE(series != nullptr);
        REQUIRE(series->getNumSamples() == 50);
        
        // Verify data values
        auto const& data = series->getAnalogTimeSeries();
        REQUIRE(data.size() == 50);
        REQUIRE(data[0] == Catch::Approx(75.0f + static_cast<float>(ch)));
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - DataManager JSON with memory mapping", "[analog][binary][datamanager][json][mmap]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withSineWave(0, 199, 0.01f, 100.0f);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_dm_mmap.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string()));
    
    // Create JSON configuration with memory mapping enabled
    json config = json::array();
    config.push_back({
        {"data_type", "analog"},
        {"name", "mmap_analog"},
        {"filepath", temp_file.string()},
        {"format", "int16"},
        {"num_channels", 1},
        {"header_size", 0},
        {"use_memory_mapped", true},
        {"num_samples", 200}
    });
    
    // Load through DataManager
    DataManager dm;
    auto data_info = load_data_from_json_config(&dm, config, temp_file.parent_path().string());
    
    // Verify data was loaded with memory mapping
    auto keys = dm.getKeys<AnalogTimeSeries>();
    REQUIRE(keys.size() == 1);
    REQUIRE(keys[0] == "mmap_analog_0");
    
    auto series = dm.getData<AnalogTimeSeries>("mmap_analog_0");
    REQUIRE(series != nullptr);
    REQUIRE(series->getNumSamples() == 200);
    
    // Verify data access works (memory mapped data should work transparently)
    auto value = series->getAtTime(TimeFrameIndex(50));
    REQUIRE(value.has_value());
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - float32 data type", "[analog][binary][loader][float32]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withRamp(0, 49, 1.5f, 50.5f);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_float32.bin";
    REQUIRE(builder.writeToBinaryFloat32(temp_file.string()));
    
    // Write float32 data directly (since loader expects it)
    std::ofstream out(temp_file, std::ios::binary);
    for (int i = 0; i < 50; ++i) {
        float val = 1.5f + static_cast<float>(i);
        out.write(reinterpret_cast<char const*>(&val), sizeof(float));
    }
    out.close();
    
    // Load with memory mapping and float32 data type
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 1;
    opts.header_size = 0;
    opts.binary_data_type = "float32";
    opts.use_memory_mapped = true;
    opts.num_samples = 50;
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 1);
    REQUIRE(series_list[0]->getNumSamples() == 50);
    
    // Verify float values
    auto samples = series_list[0]->getAllSamples();
    int count = 0;
    for (auto const& [time, value] : samples) {
        REQUIRE(value == Catch::Approx(1.5f + static_cast<float>(count)).margin(0.01f));
        count++;
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}

TEST_CASE("BinaryAnalogLoader - Header with multiple channels", "[analog][binary][loader][header][multichannel]") {
    // Create test data
    auto builder = AnalogTimeSeriesBuilder()
        .withConstant(123.0f, 0, 29, 1);
    
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / "test_header_multi.bin";
    REQUIRE(builder.writeToBinaryInt16(temp_file.string(), 128, 2));
    
    // Load with header and multiple channels
    BinaryAnalogLoaderOptions opts;
    opts.filename = temp_file.string();
    opts.num_channels = 2;
    opts.header_size = 128;
    opts.binary_data_type = "int16";
    
    auto series_list = load(opts);
    
    REQUIRE(series_list.size() == 2);
    
    for (size_t ch = 0; ch < 2; ++ch) {
        REQUIRE(series_list[ch]->getNumSamples() == 30);
        auto const& data = series_list[ch]->getAnalogTimeSeries();
        REQUIRE(data[0] == Catch::Approx(123.0f + static_cast<float>(ch)));
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
}
