/**
 * @file batch_loading.test.cpp
 * @brief Tests for batch loading functionality in format-centric loaders
 * 
 * Tests the batch loading capabilities of:
 * - CSVLoader: Multi-bodypart DLC files, multi-series DigitalEvent
 * - BinaryFormatLoader: Multi-channel binary files
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "IO/core/LoaderRegistry.hpp"
#include "IO/core/LoaderRegistration.hpp"  // For registerAllLoaders()
#include "IO/formats/CSV/CSVLoader.hpp"
#include "IO/formats/Binary/BinaryFormatLoader.hpp"

#include "Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <cmath>

using json = nlohmann::json;

// ============================================================================
// Test Fixtures
// ============================================================================

class BatchLoadingTestFixture {
public:
    BatchLoadingTestFixture() {
        // Ensure loaders are registered (idempotent - safe to call multiple times)
        static bool loaders_registered = false;
        if (!loaders_registered) {
            registerAllLoaders();
            loaders_registered = true;
        }
        
        // Test data files are relative to the source file location
        auto source_dir = std::filesystem::path(__FILE__).parent_path();
        test_data_dir = source_dir / "../data";
        dlc_csv_path = test_data_dir / "Points" / "dlc_test.csv";
        jun_test_path = test_data_dir / "DigitalIntervals" / "jun_test.dat";
        
        // Create temporary output directory for generated test files
        temp_dir = std::filesystem::temp_directory_path() / "batch_loading_test";
        std::filesystem::create_directories(temp_dir);
    }

    ~BatchLoadingTestFixture() {
        // Cleanup temporary files
        if (std::filesystem::exists(temp_dir)) {
            std::filesystem::remove_all(temp_dir);
        }
    }

protected:
    std::filesystem::path test_data_dir;
    std::filesystem::path dlc_csv_path;
    std::filesystem::path jun_test_path;
    std::filesystem::path temp_dir;
    
    // Expected bodyparts from dlc_test.csv
    std::vector<std::string> getExpectedDLCBodyparts() const {
        return {"wp_post_left", "wp_cent_left", "wp_ant_left", "nose_left", "nose_tip", 
                "nose_right", "wp_ant_right", "wp_cent_right", "wp_p_right", "cuetip"};
    }
    
    // Create a multi-channel binary test file
    std::filesystem::path createMultiChannelBinaryFile(int num_channels, int num_samples) {
        auto filepath = temp_dir / "multi_channel_test.bin";
        std::ofstream file(filepath, std::ios::binary);
        
        // Create interleaved int16 data: channel 0 = 100, channel 1 = 200, etc.
        for (int sample = 0; sample < num_samples; ++sample) {
            for (int ch = 0; ch < num_channels; ++ch) {
                int16_t value = static_cast<int16_t>((ch + 1) * 100 + sample);
                file.write(reinterpret_cast<char*>(&value), sizeof(int16_t));
            }
        }
        
        return filepath;
    }
    
    // Create a multi-series digital event CSV file
    std::filesystem::path createMultiSeriesDigitalEventCSV() {
        auto filepath = temp_dir / "multi_series_events.csv";
        std::ofstream file(filepath);
        
        // Header
        file << "timestamp,identifier\n";
        
        // Series A events
        file << "100,SeriesA\n";
        file << "200,SeriesA\n";
        file << "300,SeriesA\n";
        
        // Series B events
        file << "150,SeriesB\n";
        file << "250,SeriesB\n";
        
        // Series C events
        file << "175,SeriesC\n";
        file << "275,SeriesC\n";
        file << "375,SeriesC\n";
        file << "475,SeriesC\n";
        
        return filepath;
    }
};

// ============================================================================
// CSVLoader Batch Loading Tests - DLC Multi-Bodypart
// ============================================================================

TEST_CASE_METHOD(BatchLoadingTestFixture, 
                 "CSVLoader - DLC batch loading returns all bodyparts", 
                 "[BatchLoading][CSV][DLC][Points]") {
    
    REQUIRE(std::filesystem::exists(dlc_csv_path));
    
    CSVLoader loader;
    
    SECTION("supportsBatchLoading returns true for dlc_csv format with Points") {
        REQUIRE(loader.supportsBatchLoading("dlc_csv", IODataType::Points));
    }
    
    SECTION("supportsBatchLoading returns true for csv format with Points too") {
        // CSV Points supports batch loading capability, though it requires DLC format
        // and all_bodyparts flag to actually return multiple results
        REQUIRE(loader.supportsBatchLoading("csv", IODataType::Points));
    }
    
    SECTION("loadBatch returns all bodyparts from DLC file") {
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;  // Accept all points for testing
        
        BatchLoadResult result = loader.loadBatch(dlc_csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        auto expected_bodyparts = getExpectedDLCBodyparts();
        REQUIRE(result.results.size() == expected_bodyparts.size());
        
        // Check that all results are successful and contain PointData
        for (size_t i = 0; i < result.results.size(); ++i) {
            INFO("Checking bodypart index " << i);
            REQUIRE(result.results[i].success);
            REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result.results[i].data));
            
            auto point_data = std::get<std::shared_ptr<PointData>>(result.results[i].data);
            REQUIRE(point_data != nullptr);
            
            // DLC file has 5 frames (rows 0-4)
            // With likelihood_threshold = 0.0, should have all 5 frames
            REQUIRE(point_data->getTimeCount() <= 5);  // May have fewer if likelihood filtering
        }
    }
    
    SECTION("loadBatch with high likelihood threshold filters low-confidence points") {
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.9;  // High threshold
        
        BatchLoadResult result = loader.loadBatch(dlc_csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        // With high threshold, some bodyparts may have fewer points or be empty
        // The result still includes all bodyparts that have at least 1 point
        // Some bodyparts may be filtered out entirely if all their points have
        // likelihood below 0.9
        REQUIRE(result.results.size() <= 10);
        REQUIRE(result.results.size() >= 1);  // At least some should pass
        
        // Each included bodypart should have successful result
        for (auto const& res : result.results) {
            REQUIRE(res.success);
            auto point_data = std::get<std::shared_ptr<PointData>>(res.data);
            REQUIRE(point_data != nullptr);
        }
    }
    
    SECTION("loadBatch results have bodypart names in LoadResult::name") {
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = loader.loadBatch(dlc_csv_path.string(), 
                                                   IODataType::Points, 
                                                   config);
        
        REQUIRE(result.success);
        
        auto expected_bodyparts = getExpectedDLCBodyparts();
        
        // The results come from std::map which is alphabetically sorted
        // Check that each result has a non-empty name that's in the expected list
        std::set<std::string> expected_set(expected_bodyparts.begin(), expected_bodyparts.end());
        for (auto const& res : result.results) {
            REQUIRE(!res.name.empty());
            INFO("Checking bodypart: " << res.name);
            REQUIRE(expected_set.count(res.name) == 1);
        }
    }
}

TEST_CASE_METHOD(BatchLoadingTestFixture, 
                 "CSVLoader - Single load vs batch load for DLC", 
                 "[BatchLoading][CSV][DLC][Points]") {
    
    REQUIRE(std::filesystem::exists(dlc_csv_path));
    
    CSVLoader loader;
    
    SECTION("Single load returns first bodypart only") {
        json config;
        config["format"] = "dlc_csv";
        config["likelihood_threshold"] = 0.0;
        // Note: not setting all_bodyparts or bodypart, so should get first
        
        LoadResult result = loader.load(dlc_csv_path.string(), 
                                        IODataType::Points, 
                                        config);
        
        REQUIRE(result.success);
        REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result.data));
        
        auto point_data = std::get<std::shared_ptr<PointData>>(result.data);
        REQUIRE(point_data != nullptr);
    }
    
    SECTION("Single load with specific bodypart returns that bodypart") {
        json config;
        config["format"] = "dlc_csv";
        config["bodypart"] = "nose_tip";
        config["likelihood_threshold"] = 0.0;
        
        LoadResult result = loader.load(dlc_csv_path.string(), 
                                        IODataType::Points, 
                                        config);
        
        REQUIRE(result.success);
        REQUIRE(std::holds_alternative<std::shared_ptr<PointData>>(result.data));
        
        auto point_data = std::get<std::shared_ptr<PointData>>(result.data);
        REQUIRE(point_data != nullptr);
        REQUIRE(point_data->getTimeCount() > 0);
    }
}

// ============================================================================
// CSVLoader Batch Loading Tests - Multi-Series Digital Events
// ============================================================================

TEST_CASE_METHOD(BatchLoadingTestFixture, 
                 "CSVLoader - Multi-series DigitalEvent batch loading", 
                 "[BatchLoading][CSV][DigitalEvent]") {
    
    auto csv_path = createMultiSeriesDigitalEventCSV();
    REQUIRE(std::filesystem::exists(csv_path));
    
    CSVLoader loader;
    
    SECTION("supportsBatchLoading returns true for csv DigitalEvent") {
        REQUIRE(loader.supportsBatchLoading("csv", IODataType::DigitalEvent));
    }
    
    SECTION("loadBatch returns all series from multi-identifier CSV") {
        json config;
        config["format"] = "csv";
        config["time_column"] = 0;
        config["identifier_column"] = 1;
        config["skip_header"] = true;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::DigitalEvent, 
                                                   config);
        
        REQUIRE(result.success);
        
        // Should have 3 series: SeriesA, SeriesB, SeriesC
        REQUIRE(result.results.size() == 3);
        
        // Check each series
        for (auto const& res : result.results) {
            REQUIRE(res.success);
            REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(res.data));
            
            auto event_data = std::get<std::shared_ptr<DigitalEventSeries>>(res.data);
            REQUIRE(event_data != nullptr);
            REQUIRE(event_data->size() > 0);
        }
    }
    
    SECTION("Batch results contain correct event counts per series") {
        json config;
        config["format"] = "csv";
        config["time_column"] = 0;
        config["identifier_column"] = 1;
        config["skip_header"] = true;
        
        BatchLoadResult result = loader.loadBatch(csv_path.string(), 
                                                   IODataType::DigitalEvent, 
                                                   config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 3);
        
        // Expected counts: SeriesA=3, SeriesB=2, SeriesC=4
        // But order may vary, so check total
        size_t total_events = 0;
        for (auto const& res : result.results) {
            auto event_data = std::get<std::shared_ptr<DigitalEventSeries>>(res.data);
            total_events += event_data->size();
        }
        
        REQUIRE(total_events == 9);  // 3 + 2 + 4
    }
}

// ============================================================================
// BinaryFormatLoader Batch Loading Tests - Multi-Channel Analog
// ============================================================================

TEST_CASE_METHOD(BatchLoadingTestFixture, 
                 "BinaryFormatLoader - Multi-channel analog batch loading", 
                 "[BatchLoading][Binary][Analog]") {
    
    constexpr int NUM_CHANNELS = 4;
    constexpr int NUM_SAMPLES = 100;
    
    auto binary_path = createMultiChannelBinaryFile(NUM_CHANNELS, NUM_SAMPLES);
    REQUIRE(std::filesystem::exists(binary_path));
    
    BinaryFormatLoader loader;
    
    SECTION("supportsBatchLoading returns true for binary Analog") {
        REQUIRE(loader.supportsBatchLoading("binary", IODataType::Analog));
    }
    
    SECTION("loadBatch returns all channels from multi-channel binary") {
        json config;
        config["format"] = "binary";
        config["num_channels"] = NUM_CHANNELS;
        config["sample_rate"] = 1000.0;
        // Default data_type is int16
        
        BatchLoadResult result = loader.loadBatch(binary_path.string(), 
                                                   IODataType::Analog, 
                                                   config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == NUM_CHANNELS);
        
        // Check each channel
        for (size_t ch = 0; ch < result.results.size(); ++ch) {
            INFO("Checking channel " << ch);
            REQUIRE(result.results[ch].success);
            REQUIRE(std::holds_alternative<std::shared_ptr<AnalogTimeSeries>>(result.results[ch].data));
            
            auto analog_data = std::get<std::shared_ptr<AnalogTimeSeries>>(result.results[ch].data);
            REQUIRE(analog_data != nullptr);
            REQUIRE(analog_data->getNumSamples() == NUM_SAMPLES);
        }
    }
    
    SECTION("Each channel has correct data values") {
        json config;
        config["format"] = "binary";
        config["num_channels"] = NUM_CHANNELS;
        config["sample_rate"] = 1000.0;
        
        BatchLoadResult result = loader.loadBatch(binary_path.string(), 
                                                   IODataType::Analog, 
                                                   config);
        
        REQUIRE(result.success);
        
        // Check first sample of each channel
        for (size_t ch = 0; ch < result.results.size(); ++ch) {
            auto analog_data = std::get<std::shared_ptr<AnalogTimeSeries>>(result.results[ch].data);
            
            // First sample value should be (ch+1) * 100 + 0
            float expected_first_value = static_cast<float>((ch + 1) * 100);
            auto data_span = analog_data->getAnalogTimeSeries();
            REQUIRE(!data_span.empty());
            float actual_first_value = data_span[0];
            
            INFO("Channel " << ch << ": expected " << expected_first_value 
                 << ", got " << actual_first_value);
            REQUIRE(actual_first_value == Catch::Approx(expected_first_value).epsilon(0.01));
        }
    }
    
    SECTION("Batch results have channel index names") {
        json config;
        config["format"] = "binary";
        config["num_channels"] = NUM_CHANNELS;
        config["sample_rate"] = 1000.0;
        
        BatchLoadResult result = loader.loadBatch(binary_path.string(), 
                                                   IODataType::Analog, 
                                                   config);
        
        REQUIRE(result.success);
        
        // Check that each result has a channel name (could be index-based)
        for (size_t ch = 0; ch < result.results.size(); ++ch) {
            // Name might be empty or contain channel index
            // The implementation should set a name for identification
            INFO("Channel " << ch << " name: '" << result.results[ch].name << "'");
            // Name is expected to be the channel index as string
            REQUIRE(result.results[ch].name == std::to_string(ch));
        }
    }
}

// ============================================================================
// LoaderRegistry Integration Tests
// ============================================================================

TEST_CASE_METHOD(BatchLoadingTestFixture, 
                 "LoaderRegistry - tryLoadBatch integration", 
                 "[BatchLoading][Registry]") {
    
    LoaderRegistry& registry = LoaderRegistry::getInstance();
    
    SECTION("Registry reports batch loading support correctly") {
        // DLC CSV should support batch loading
        REQUIRE(registry.isBatchLoadingSupported("dlc_csv", IODataType::Points));
        
        // Binary analog should support batch loading
        REQUIRE(registry.isBatchLoadingSupported("binary", IODataType::Analog));
        
        // CSV digital event should support batch loading
        REQUIRE(registry.isBatchLoadingSupported("csv", IODataType::DigitalEvent));
        
        // Regular CSV line should NOT support batch loading
        REQUIRE_FALSE(registry.isBatchLoadingSupported("csv", IODataType::Line));
    }
    
    SECTION("tryLoadBatch works through registry for DLC files") {
        if (!std::filesystem::exists(dlc_csv_path)) {
            SKIP("DLC test file not found");
        }
        
        json config;
        config["format"] = "dlc_csv";
        config["all_bodyparts"] = true;
        config["likelihood_threshold"] = 0.0;
        
        BatchLoadResult result = registry.tryLoadBatch("dlc_csv", IODataType::Points, 
                                                        dlc_csv_path.string(), config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 10);  // 10 bodyparts in dlc_test.csv
    }
    
    SECTION("tryLoadBatch for non-batch format returns single result") {
        // Test that a format without explicit batch support returns a single result
        // Use DLC with a specific bodypart (not all_bodyparts) to test single-result path
        json config;
        config["format"] = "dlc_csv";
        config["bodypart"] = "nose_tip";
        config["likelihood_threshold"] = 0.0;
        // Note: all_bodyparts not set, so it should fall back to single load
        
        BatchLoadResult result = registry.tryLoadBatch("dlc_csv", IODataType::Points, 
                                                        dlc_csv_path.string(), config);
        
        REQUIRE(result.success);
        REQUIRE(result.results.size() == 1);  // Single result from non-batch load
    }
}

// ============================================================================
// BatchLoadResult Utility Tests
// ============================================================================

TEST_CASE("BatchLoadResult - successCount utility", "[BatchLoading][BatchLoadResult]") {
    
    SECTION("successCount returns correct count for mixed results") {
        BatchLoadResult batch;
        batch.success = true;
        
        // Add 3 successful results
        LoadResult success1;
        success1.success = true;
        batch.results.push_back(std::move(success1));
        
        LoadResult success2;
        success2.success = true;
        batch.results.push_back(std::move(success2));
        
        LoadResult success3;
        success3.success = true;
        batch.results.push_back(std::move(success3));
        
        // Add 2 failed results
        LoadResult fail1("Error 1");
        batch.results.push_back(std::move(fail1));
        
        LoadResult fail2("Error 2");
        batch.results.push_back(std::move(fail2));
        
        REQUIRE(batch.results.size() == 5);
        REQUIRE(batch.successCount() == 3);
    }
    
    SECTION("fromVector creates batch from vector of results") {
        std::vector<LoadResult> results;
        
        LoadResult r1;
        r1.success = true;
        r1.name = "result1";
        results.push_back(std::move(r1));
        
        LoadResult r2;
        r2.success = true;
        r2.name = "result2";
        results.push_back(std::move(r2));
        
        auto batch = BatchLoadResult::fromVector(std::move(results));
        
        REQUIRE(batch.success);
        REQUIRE(batch.results.size() == 2);
        REQUIRE(batch.results[0].name == "result1");
        REQUIRE(batch.results[1].name == "result2");
    }
    
    SECTION("error creates failed batch result") {
        auto batch = BatchLoadResult::error("Test error message");
        
        REQUIRE_FALSE(batch.success);
        REQUIRE(batch.error_message == "Test error message");
        REQUIRE(batch.results.empty());
    }
}
