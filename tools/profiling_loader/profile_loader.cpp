/**
 * @file profile_loader.cpp
 * @brief Simple executable to profile the load_data_from_json_config function
 * 
 * This tool loads data using various JSON configurations and can be profiled
 * with tools like heaptrack to analyze memory allocation patterns.
 */

#include "DataManager/DataManager.hpp"
#include <iostream>
#include <string>
#include <filesystem>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_config_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " config_analog_csv.json" << std::endl;
        return 1;
    }

    std::string config_file = argv[1];
    
    // Check if file exists
    if (!std::filesystem::exists(config_file)) {
        std::cerr << "Error: Config file does not exist: " << config_file << std::endl;
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Profiling Loader Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config file: " << config_file << std::endl;
    std::cout << std::endl;

    // Create DataManager
    auto dm = std::make_unique<DataManager>();
    
    std::cout << "Loading data..." << std::endl;
    
    // Load data from JSON config
    auto data_info_list = load_data_from_json_config(dm.get(), config_file);
    
    std::cout << "Loading complete!" << std::endl;
    std::cout << "Loaded " << data_info_list.size() << " data items" << std::endl;
    
    // Print what was loaded
    for (const auto& info : data_info_list) {
        std::cout << "  - " << info.key << " (" << info.data_class << ")" << std::endl;
    }
    
    // Get all keys to verify data is in memory
    auto all_keys = dm->getAllKeys();
    std::cout << "\nDataManager contains " << all_keys.size() << " keys:" << std::endl;
    for (const auto& key : all_keys) {
        std::cout << "  - " << key << std::endl;
    }
    
    std::cout << "\nTest completed successfully!" << std::endl;
    
    return 0;
}
