// Fuzz target for Point CSV parser
// Tests the robustness of CSV parsing for point/keypoint data

#include "Points/IO/CSV/Point_Data_CSV.hpp"
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
    std::string temp_path = "/tmp/fuzz_point_csv_" + std::to_string(getpid()) + ".csv";
    
    try {
        // Write the fuzzed data to a temporary file
        std::ofstream temp_file(temp_path, std::ios::binary);
        if (!temp_file) {
            return 0;
        }
        temp_file.write(reinterpret_cast<const char*>(data), size);
        temp_file.close();
        
        // Test with various column configurations
        // Standard 3-column format: frame, x, y
        try {
            CSVPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 0;
            opts.x_column = 1;
            opts.y_column = 2;
            opts.column_delim = ',';
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable - we're testing for crashes
        }
        
        // Different column order
        try {
            CSVPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 2;
            opts.x_column = 0;
            opts.y_column = 1;
            opts.column_delim = ',';
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Tab-separated
        try {
            CSVPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 0;
            opts.x_column = 1;
            opts.y_column = 2;
            opts.column_delim = '\t';
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Space-separated
        try {
            CSVPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 0;
            opts.x_column = 1;
            opts.y_column = 2;
            opts.column_delim = ' ';
            auto result = load(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test DLC (DeepLabCut) format parser
        try {
            DLCPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 0;
            opts.likelihood_threshold = 0.5f;
            auto result = load_dlc_csv(opts);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // DLC with different threshold
        try {
            DLCPointLoaderOptions opts;
            opts.filename = temp_path;
            opts.frame_column = 0;
            opts.likelihood_threshold = 0.9f;
            auto result = load_dlc_csv(opts);
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
