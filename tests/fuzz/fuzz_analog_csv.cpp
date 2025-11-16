// Fuzz target for AnalogTimeSeries CSV parser
// Tests the robustness of CSV parsing for analog time series data

#include "AnalogTimeSeries/IO/CSV/Analog_Time_Series_CSV.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <fstream>
#include <filesystem>

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Skip empty inputs
    if (size == 0 || size > 1024 * 1024) { // Limit to 1MB
        return 0;
    }
    
    // Create a temporary file with the fuzzed data
    std::string temp_path = "/tmp/fuzz_analog_csv_" + std::to_string(getpid()) + ".csv";
    
    try {
        // Write the fuzzed data to a temporary file
        std::ofstream temp_file(temp_path, std::ios::binary);
        if (!temp_file) {
            return 0;
        }
        temp_file.write(reinterpret_cast<const char*>(data), size);
        temp_file.close();
        
        // Test 1: Simple single-column parser
        try {
            auto result = load_analog_series_from_csv(temp_path);
            // If parsing succeeds, that's fine - we're just looking for crashes
            (void)result; // Suppress unused variable warning
        } catch (...) {
            // Exceptions are acceptable - we're testing for crashes, not logic errors
        }
        
        // Test 2: Advanced parser with various options
        // Test single column format
        try {
            CSVAnalogLoaderOptions opts;
            opts.filepath = temp_path;
            opts.single_column_format = true;
            opts.delimiter = ",";
            opts.has_header = false;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test with header
        try {
            CSVAnalogLoaderOptions opts;
            opts.filepath = temp_path;
            opts.single_column_format = true;
            opts.delimiter = ",";
            opts.has_header = true;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test two-column format
        try {
            CSVAnalogLoaderOptions opts;
            opts.filepath = temp_path;
            opts.single_column_format = false;
            opts.time_column = 0;
            opts.data_column = 1;
            opts.delimiter = ",";
            opts.has_header = false;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test with different delimiters
        try {
            CSVAnalogLoaderOptions opts;
            opts.filepath = temp_path;
            opts.single_column_format = false;
            opts.time_column = 0;
            opts.data_column = 1;
            opts.delimiter = "\t";
            opts.has_header = false;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
    } catch (...) {
        // Clean up and return
    }
    
    // Clean up the temporary file
    try {
        std::filesystem::remove(temp_path);
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return 0;
}
