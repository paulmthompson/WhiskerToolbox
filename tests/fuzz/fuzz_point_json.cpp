// Fuzz target for Point JSON configuration parser
// Tests the robustness of JSON parsing for point data configuration

#include "Points/IO/JSON/Point_Data_JSON.hpp"
#include <nlohmann/json.hpp>
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
    
    // Create temporary files for both JSON config and CSV data
    std::string temp_csv_path = "/tmp/fuzz_point_json_" + std::to_string(getpid()) + ".csv";
    std::string temp_json_str(reinterpret_cast<const char*>(data), size);
    
    try {
        // Create a dummy CSV file for the parser to reference
        std::ofstream temp_csv(temp_csv_path);
        temp_csv << "0,10.5,20.3\n";
        temp_csv << "1,11.2,21.8\n";
        temp_csv << "2,12.1,22.5\n";
        temp_csv.close();
        
        // Try to parse the fuzzed data as JSON
        nlohmann::json json_obj;
        try {
            json_obj = nlohmann::json::parse(temp_json_str);
        } catch (...) {
            // If JSON parsing fails, that's expected for random input
            std::filesystem::remove(temp_csv_path);
            return 0;
        }
        
        // Test the Point JSON loader with the parsed JSON
        try {
            auto result = load_into_PointData(temp_csv_path, json_obj);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable - we're looking for crashes
        }
        
        // Test with DLC format in JSON
        try {
            // Add DLC-specific fields to the JSON if they don't exist
            nlohmann::json dlc_json = json_obj;
            if (!dlc_json.contains("format")) {
                dlc_json["format"] = "dlc_csv";
            }
            auto result = load_into_PointData(temp_csv_path, dlc_json);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
        // Test the multi-bodypart loader
        try {
            auto result = load_multiple_PointData_from_dlc(temp_csv_path, json_obj);
            (void)result;
        } catch (...) {
            // Exceptions are acceptable
        }
        
    } catch (...) {
        // Clean up and return
    }
    
    // Clean up the temporary files
    try {
        std::filesystem::remove(temp_csv_path);
    } catch (...) {
        // Ignore cleanup errors
    }
    
    return 0;
}
