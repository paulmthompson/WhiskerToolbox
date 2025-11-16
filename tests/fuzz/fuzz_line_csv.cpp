// Fuzz target for Line CSV parser
// Tests the robustness of CSV parsing for line/whisker data

#include "Lines/IO/CSV/Line_Data_CSV.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <fstream>
#include <filesystem>

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Skip empty inputs or very large inputs
    if (size == 0 || size > 1024 * 1024) { // Limit to 1MB
        return 0;
    }
    
    // Create a temporary file with the fuzzed data
    std::string temp_path = "/tmp/fuzz_line_csv_" + std::to_string(getpid()) + ".csv";
    
    try {
        // Write the fuzzed data to a temporary file
        std::ofstream temp_file(temp_path, std::ios::binary);
        if (!temp_file) {
            return 0;
        }
        temp_file.write(reinterpret_cast<const char*>(data), size);
        temp_file.close();
        
        // Test 1: Simple single-file loader
        try {
            auto result = load_line_csv(temp_path);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable - we're testing for crashes
        }
        
        // Test 2: Single file with options
        try {
            CSVSingleFileLineLoaderOptions opts;
            opts.filepath = temp_path;
            opts.delimiter = ",";
            opts.coordinate_delimiter = ",";
            opts.has_header = false;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test 3: With header
        try {
            CSVSingleFileLineLoaderOptions opts;
            opts.filepath = temp_path;
            opts.delimiter = ",";
            opts.coordinate_delimiter = ",";
            opts.has_header = true;
            opts.header_identifier = "Frame";
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test 4: Different delimiters
        try {
            CSVSingleFileLineLoaderOptions opts;
            opts.filepath = temp_path;
            opts.delimiter = "\t";
            opts.coordinate_delimiter = " ";
            opts.has_header = false;
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test 5: Parse float vector helper
        try {
            std::string str(reinterpret_cast<const char*>(data), std::min(size, size_t(1024)));
            auto result = parse_string_to_float_vector(str, ",");
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test 6: Parse with different delimiter
        try {
            std::string str(reinterpret_cast<const char*>(data), std::min(size, size_t(1024)));
            auto result = parse_string_to_float_vector(str, " ");
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
