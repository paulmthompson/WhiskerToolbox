#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/IO/CSV/MultiColumnBinaryCSV.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <fstream>
#include <filesystem>

namespace {

// Helper to get the path to the test data file relative to the build directory
std::string getTestDataPath() {
    return "data/DigitalIntervals/jun_test.dat";
}

} // anonymous namespace


TEST_CASE("MultiColumnBinaryCSV - Get column names", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    std::string const test_file = getTestDataPath();
    
    // Skip test if file doesn't exist
    if (!std::filesystem::exists(test_file)) {
        SKIP("Test file not found: " << test_file);
    }
    
    auto columns = getColumnNames(test_file, 5, "\t");
    
    REQUIRE(!columns.empty());
    
    // The file has Time, v0, v1, v2, v3, y0, y1, y2, y3 columns
    // But actual data only has 5 columns (Time + 4 data columns)
    CHECK(columns.size() >= 5);
    
    // Check that Time is the first column
    if (!columns.empty()) {
        CHECK(columns[0] == "Time");
    }
}


TEST_CASE("MultiColumnBinaryCSV - Load intervals from real file", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    std::string const test_file = getTestDataPath();
    
    // Skip test if file doesn't exist
    if (!std::filesystem::exists(test_file)) {
        SKIP("Test file not found: " << test_file);
    }
    
    MultiColumnBinaryCSVLoaderOptions opts;
    opts.filepath = test_file;
    opts.header_lines_to_skip = rfl::Validator<int, rfl::Minimum<0>>(5);
    opts.data_column = rfl::Validator<int, rfl::Minimum<0>>(1);  // v0 column (second column after Time)
    opts.delimiter = "\t";
    opts.binary_threshold = 0.5;
    
    auto result = load(opts);
    
    REQUIRE(result != nullptr);
    
    // The v0 column is all 1s in the test data, so we should get exactly 1 interval
    // covering the entire data range
    auto const & intervals = result->view();
    
    // Should have at least one interval
    CHECK(result->size() >= 1);
    
    // First interval should start at 0
    if (!intervals.empty()) {
        CHECK(intervals[0].value().start == 0);
    }
}


TEST_CASE("MultiColumnBinaryCSV - Load TimeFrame from real file", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    std::string const test_file = getTestDataPath();
    
    // Skip test if file doesn't exist
    if (!std::filesystem::exists(test_file)) {
        SKIP("Test file not found: " << test_file);
    }
    
    SECTION("With sampling rate") {
        MultiColumnBinaryCSVTimeFrameOptions opts;
        opts.filepath = test_file;
        opts.header_lines_to_skip = rfl::Validator<int, rfl::Minimum<0>>(5);
        opts.time_column = rfl::Validator<int, rfl::Minimum<0>>(0);
        opts.delimiter = "\t";
        opts.sampling_rate = rfl::Validator<double, rfl::Minimum<0.0>>(14000.0);  // 14kHz
        
        auto result = load(opts);
        
        REQUIRE(result != nullptr);
        
        // Should have loaded multiple time values
        CHECK(result->getTotalFrameCount() > 0);
        
        // First time value should be 0 (0.0 * 14000)
        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 0);
    }
    
    SECTION("Check time progression") {
        MultiColumnBinaryCSVTimeFrameOptions opts;
        opts.filepath = test_file;
        opts.header_lines_to_skip = rfl::Validator<int, rfl::Minimum<0>>(5);
        opts.time_column = rfl::Validator<int, rfl::Minimum<0>>(0);
        opts.delimiter = "\t";
        opts.sampling_rate = rfl::Validator<double, rfl::Minimum<0.0>>(1000.0);  // 1kHz for easier values
        
        auto result = load(opts);
        
        REQUIRE(result != nullptr);
        
        if (result->getTotalFrameCount() >= 2) {
            // Time values should be increasing
            int time0 = result->getTimeAtIndex(TimeFrameIndex(0));
            int time1 = result->getTimeAtIndex(TimeFrameIndex(1));
            CHECK(time1 >= time0);
        }
    }
}


TEST_CASE("MultiColumnBinaryCSV - Zero column (no intervals)", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    std::string const test_file = getTestDataPath();
    
    // Skip test if file doesn't exist
    if (!std::filesystem::exists(test_file)) {
        SKIP("Test file not found: " << test_file);
    }
    
    MultiColumnBinaryCSVLoaderOptions opts;
    opts.filepath = test_file;
    opts.header_lines_to_skip = rfl::Validator<int, rfl::Minimum<0>>(5);
    opts.data_column = rfl::Validator<int, rfl::Minimum<0>>(2);  // v1 column (should be all zeros)
    opts.delimiter = "\t";
    opts.binary_threshold = 0.5;
    
    auto result = load(opts);
    
    REQUIRE(result != nullptr);
    
    // The v1 column is all 0s, so we should get no intervals
    auto const & intervals = result->view();
    CHECK(intervals.empty());
}


TEST_CASE("MultiColumnBinaryCSV - Options defaults", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    MultiColumnBinaryCSVLoaderOptions opts;
    opts.filepath = "test.dat";
    
    CHECK(opts.getHeaderLinesToSkip() == 5);
    CHECK(opts.getTimeColumn() == 0);
    CHECK(opts.getDataColumn() == 1);
    CHECK(opts.getDelimiter() == "\t");
    CHECK(opts.getSamplingRate() == 0.0);
    CHECK(opts.getBinaryThreshold() == 0.5);
}


TEST_CASE("MultiColumnBinaryCSV - TimeFrame options defaults", "[DataManager][DigitalInterval][MultiColumnBinary]") {
    MultiColumnBinaryCSVTimeFrameOptions opts;
    opts.filepath = "test.dat";
    
    CHECK(opts.getHeaderLinesToSkip() == 5);
    CHECK(opts.getTimeColumn() == 0);
    CHECK(opts.getDelimiter() == "\t");
    CHECK(opts.getSamplingRate() == 1.0);
}
