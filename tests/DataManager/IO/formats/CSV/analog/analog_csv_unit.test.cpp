/**
 * @file analog_csv_unit.test.cpp
 * @brief Unit tests for AnalogTimeSeries CSV direct function calls and legacy APIs
 *
 * These tests exercise the CSV loading functions directly without going through
 * the DataManager JSON config interface. They complement the integration tests
 * in analog_csv_integration.test.cpp.
 *
 * Tests include:
 * 1. Direct save/load via CSVAnalogSaverOptions/CSVAnalogLoaderOptions
 * 2. Single column format loading via direct function
 * 3. Legacy load_analog_series_from_csv function
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "IO/formats/CSV/analogtimeseries/Analog_Time_Series_CSV.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;

class AnalogTimeSeriesCSVUnitTestFixture {
public:
    AnalogTimeSeriesCSVUnitTestFixture() {
        // Set up test directory
        test_dir = std::filesystem::current_path() / "test_analog_csv_unit_output";
        std::filesystem::create_directories(test_dir);

        csv_filename = "test_analog_data.csv";
        csv_filepath = test_dir / csv_filename;

        // Create test AnalogTimeSeries with sample data
        createTestAnalogData();
    }

    ~AnalogTimeSeriesCSVUnitTestFixture() {
        // Clean up - remove test files and directory
        cleanup();
    }

protected:
    void createTestAnalogData() {
        // Create test data with time and values
        std::vector<float> const test_values = {
                                                1.5f, 2.3f, 3.7f, 4.1f, 5.9f, 6.2f, 7.8f, 8.4f, 9.1f, 10.6f};

        std::vector<TimeFrameIndex> const test_times = {
                                                        TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20),
                                                        TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50),
                                                        TimeFrameIndex(60), TimeFrameIndex(70), TimeFrameIndex(80),
                                                        TimeFrameIndex(90)};

        original_analog_data = std::make_shared<AnalogTimeSeries>(test_values, test_times);
    }

    bool saveCSVAnalogData() {
        CSVAnalogSaverOptions save_opts;
        save_opts.filename = csv_filename;
        save_opts.parent_dir = test_dir.string();
        save_opts.precision = 2;
        save_opts.save_header = true;
        save_opts.header = "Time,Data";
        save_opts.delimiter = ",";

        save(original_analog_data.get(), save_opts);
        return std::filesystem::exists(csv_filepath);
    }

    void cleanup() {
        try {
            if (std::filesystem::exists(csv_filepath)) {
                std::filesystem::remove(csv_filepath);
            }
            if (std::filesystem::exists(test_dir) && std::filesystem::is_empty(test_dir)) {
                std::filesystem::remove(test_dir);
            }
        } catch (std::exception const & e) {
            // Best effort cleanup - don't fail test if cleanup fails
            std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
        }
    }

    void verifyAnalogDataEquality(AnalogTimeSeries const & loaded_data) const {
        // Check number of samples
        REQUIRE(loaded_data.getNumSamples() == original_analog_data->getNumSamples());

        // Use getAllSamples() for cleaner iteration over time-value pairs
        auto original_samples = original_analog_data->getAllSamples();
        auto loaded_samples = loaded_data.getAllSamples();

        auto original_it = original_samples.begin();
        auto loaded_it = loaded_samples.begin();

        while (original_it != original_samples.end() && loaded_it != loaded_samples.end()) {
            auto const & original_sample = *original_it;
            auto const & loaded_sample = *loaded_it;

            // Check data values
            REQUIRE_THAT(original_sample.value(), WithinAbs(loaded_sample.value(), 0.01f));

            // Check time frame indices
            REQUIRE(original_sample.time_frame_index.getValue() == loaded_sample.time_frame_index.getValue());

            ++original_it;
            ++loaded_it;
        }
    }

protected:
    std::filesystem::path test_dir;
    std::string csv_filename;
    std::filesystem::path csv_filepath;
    std::shared_ptr<AnalogTimeSeries> original_analog_data;
};

TEST_CASE_METHOD(AnalogTimeSeriesCSVUnitTestFixture,
                 "DM - IO - AnalogTimeSeries - CSV save and load through direct functions",
                 "[AnalogTimeSeries][CSV][IO][unit]") {

    SECTION("Save AnalogTimeSeries to CSV format") {
        bool const save_success = saveCSVAnalogData();
        REQUIRE(save_success);
        REQUIRE(std::filesystem::exists(csv_filepath));
        REQUIRE(std::filesystem::file_size(csv_filepath) > 0);

        // Verify CSV file contents by reading a few lines
        std::ifstream file(csv_filepath);
        std::string line;

        // Check header
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "Time,Data");

        // Check first data line
        REQUIRE(std::getline(file, line));
        REQUIRE(line == "0,1.50");

        file.close();
    }

    SECTION("Load AnalogTimeSeries from CSV format") {
        // First save the data
        REQUIRE(saveCSVAnalogData());

        // Load using CSV loader
        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = csv_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = true;
        load_opts.single_column_format = false;
        load_opts.time_column = 0;
        load_opts.data_column = 1;

        auto loaded_analog_data = load(load_opts);
        REQUIRE(loaded_analog_data != nullptr);

        // Verify the loaded data matches the original
        verifyAnalogDataEquality(*loaded_analog_data);
    }

    SECTION("Load AnalogTimeSeries from single column CSV format") {
        // Create a single column CSV file for testing
        std::filesystem::path const single_col_filepath = test_dir / "single_column.csv";
        std::ofstream single_col_file(single_col_filepath);
        single_col_file << "1.5\n2.3\n3.7\n4.1\n5.9\n";
        single_col_file.close();

        // Load using single column format
        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = single_col_filepath.string();
        load_opts.delimiter = ",";
        load_opts.has_header = false;
        load_opts.single_column_format = true;

        auto loaded_analog_data = load(load_opts);
        REQUIRE(loaded_analog_data != nullptr);
        REQUIRE(loaded_analog_data->getNumSamples() == 5);

        // Check first few values using getAllSamples()
        auto samples = loaded_analog_data->getAllSamples();
        auto it = samples.begin();
        REQUIRE_THAT((*it).value(), WithinAbs(1.5f, 0.01f));
        ++it;
        REQUIRE_THAT((*it).value(), WithinAbs(2.3f, 0.01f));
        ++it;
        REQUIRE_THAT((*it).value(), WithinAbs(3.7f, 0.01f));

        // Clean up
        std::filesystem::remove(single_col_filepath);
    }
}

TEST_CASE_METHOD(AnalogTimeSeriesCSVUnitTestFixture,
                 "DM - IO - AnalogTimeSeries - Legacy CSV loader function",
                 "[AnalogTimeSeries][CSV][IO][unit][legacy]") {

    SECTION("Test legacy load_analog_series_from_csv function") {
        // Create a simple single column CSV file
        std::filesystem::path const legacy_filepath = test_dir / "legacy.csv";
        std::ofstream legacy_file(legacy_filepath);
        legacy_file << "1.0\n2.0\n3.0\n4.0\n5.0\n6.0\n";
        legacy_file.close();

        // Test the legacy load function
        auto data = load_analog_series_from_csv(legacy_filepath.string());

        REQUIRE(data.size() == 6);
        REQUIRE_THAT(data[0], WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(data[1], WithinAbs(2.0f, 0.01f));
        REQUIRE_THAT(data[5], WithinAbs(6.0f, 0.01f));

        // Clean up
        std::filesystem::remove(legacy_filepath);
    }
}

TEST_CASE("DM - IO - AnalogTimeSeries - CSV round-trip edge cases",
          "[AnalogTimeSeries][CSV][IO][unit][roundtrip]") {

    auto const test_dir = std::filesystem::temp_directory_path() / "analog_csv_roundtrip_test";
    std::filesystem::create_directories(test_dir);

    auto roundtrip = [&](AnalogTimeSeries const & original,
                         CSVAnalogSaverOptions const & save_opts,
                         bool expected_header) {
        bool const ok = save(&original, save_opts);
        REQUIRE(ok);

        CSVAnalogLoaderOptions load_opts;
        load_opts.filepath = (std::filesystem::path(save_opts.parent_dir) / save_opts.filename).string();
        load_opts.delimiter = save_opts.delimiter;
        load_opts.has_header = expected_header;
        load_opts.single_column_format = false;
        load_opts.time_column = 0;
        load_opts.data_column = 1;

        auto loaded = load(load_opts);
        REQUIRE(loaded != nullptr);
        return loaded;
    };

    CSVAnalogSaverOptions base_opts;
    base_opts.parent_dir = test_dir.string();
    base_opts.delimiter = ",";
    base_opts.save_header = true;
    base_opts.header = "Time,Data";
    base_opts.precision = 6;

    SECTION("Single sample round-trip") {
        base_opts.filename = "single.csv";
        auto single_sample_vec = std::vector<float>{42.5f};
        auto single_sample_time = std::vector<TimeFrameIndex>{TimeFrameIndex(100)};
        AnalogTimeSeries const original(single_sample_vec, single_sample_time);

        auto loaded = roundtrip(original, base_opts, true);
        REQUIRE(loaded->getNumSamples() == 1);

        auto samples = loaded->getAllSamples();
        auto it = samples.begin();
        CHECK((*it).time_frame_index.getValue() == 100);
        CHECK_THAT((*it).value(), WithinAbs(42.5f, 0.01f));
    }

    SECTION("Multiple samples round-trip") {
        base_opts.filename = "multi.csv";
        std::vector<float> const values = {1.5f, 2.3f, 3.7f, 4.1f, 5.9f};
        std::vector<TimeFrameIndex> const times = {
                                                   TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20),
                                                   TimeFrameIndex(30), TimeFrameIndex(40)};
        AnalogTimeSeries const original(values, times);

        auto loaded = roundtrip(original, base_opts, true);
        REQUIRE(loaded->getNumSamples() == 5);

        auto orig_samples = original.getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        auto orig_it = orig_samples.begin();
        auto loaded_it = loaded_samples.begin();
        while (orig_it != orig_samples.end()) {
            CHECK((*loaded_it).time_frame_index.getValue() == (*orig_it).time_frame_index.getValue());
            CHECK_THAT((*loaded_it).value(), WithinAbs((*orig_it).value(), 0.01f));
            ++orig_it;
            ++loaded_it;
        }
    }

    SECTION("Negative values round-trip") {
        base_opts.filename = "negative.csv";
        std::vector<float> const values = {-3.14f, 0.0f, -100.5f, 999.999f};
        std::vector<TimeFrameIndex> const times = {
                                                   TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
        AnalogTimeSeries const original(values, times);

        auto loaded = roundtrip(original, base_opts, true);
        REQUIRE(loaded->getNumSamples() == 4);

        auto loaded_samples = loaded->getAllSamples();
        auto it = loaded_samples.begin();
        CHECK_THAT((*it).value(), WithinAbs(-3.14f, 0.01f));
        ++it;
        CHECK_THAT((*it).value(), WithinAbs(0.0f, 0.01f));
        ++it;
        CHECK_THAT((*it).value(), WithinAbs(-100.5f, 0.01f));
        ++it;
        CHECK_THAT((*it).value(), WithinAbs(999.999f, 0.01f));
    }

    SECTION("Space-delimited round-trip") {
        base_opts.filename = "space_delim.csv";
        base_opts.delimiter = " ";
        base_opts.save_header = false;

        std::vector<float> const values = {10.0f, 20.0f, 30.0f};
        std::vector<TimeFrameIndex> const times = {
                                                   TimeFrameIndex(5), TimeFrameIndex(10), TimeFrameIndex(15)};
        AnalogTimeSeries const original(values, times);

        auto loaded = roundtrip(original, base_opts, false);
        REQUIRE(loaded->getNumSamples() == 3);

        auto orig_samples = original.getAllSamples();
        auto loaded_samples = loaded->getAllSamples();
        auto orig_it = orig_samples.begin();
        auto loaded_it = loaded_samples.begin();
        while (orig_it != orig_samples.end()) {
            CHECK((*loaded_it).time_frame_index.getValue() == (*orig_it).time_frame_index.getValue());
            CHECK_THAT((*loaded_it).value(), WithinAbs((*orig_it).value(), 0.01f));
            ++orig_it;
            ++loaded_it;
        }
    }

    SECTION("Save returns true on success") {
        base_opts.filename = "success_check.csv";
        auto single_sample_vec = std::vector<float>{1.0f};
        auto single_sample_time = std::vector<TimeFrameIndex>{TimeFrameIndex(0)};
        AnalogTimeSeries const original(single_sample_vec, single_sample_time);
        bool const ok = save(&original, base_opts);
        CHECK(ok);
        CHECK(std::filesystem::exists(test_dir / "success_check.csv"));
    }

    std::filesystem::remove_all(test_dir);
}
