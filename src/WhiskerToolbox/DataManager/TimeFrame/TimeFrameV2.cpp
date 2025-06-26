#include "TimeFrameV2.hpp"
#include "TimeFrame.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <regex>

namespace TimeFrameUtils {

// Helper function to extract values from filenames (shared by both TimeFrameV2 functions)
static std::vector<int64_t> extractValuesFromFilenames(FilenameTimeFrameOptions const & options) {
    std::vector<int64_t> extracted_values;

    try {
        // Check if directory exists
        if (!std::filesystem::exists(options.folder_path) || !std::filesystem::is_directory(options.folder_path)) {
            std::cerr << "Error: Directory does not exist: " << options.folder_path << std::endl;
            return extracted_values;
        }

        std::regex pattern(options.regex_pattern);
        std::smatch match;

        // Iterate through files in directory
        for (auto const & entry: std::filesystem::directory_iterator(options.folder_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().filename().string();

            // Check file extension
            if (!options.file_extension.empty() &&
                !filename.ends_with(options.file_extension)) {
                continue;
            }

            // Extract numerical value using regex
            if (std::regex_search(filename, match, pattern)) {
                if (match.size() >= 2) {// Must have at least one capture group
                    try {
                        int64_t value = std::stoll(match[1].str());
                        extracted_values.push_back(value);
                    } catch (std::exception const & e) {
                        std::cout << "Warning: Could not parse number from filename " << filename
                                  << " (matched: '" << match[1].str() << "'): " << e.what() << std::endl;
                    }
                } else {
                    std::cout << "Warning: Regex pattern matched but no capture group found in filename: "
                              << filename << std::endl;
                }
            } else {
                std::cout << "Warning: No match found for pattern in filename: " << filename << std::endl;
            }
        }

        // Sort values if requested
        if (options.sort_ascending) {
            std::sort(extracted_values.begin(), extracted_values.end());
        }

    } catch (std::exception const & e) {
        std::cerr << "Error extracting values from filenames: " << e.what() << std::endl;
        extracted_values.clear();
    }

    return extracted_values;
}

std::shared_ptr<CameraTimeFrame> createCameraTimeFrameFromFilenames(FilenameTimeFrameOptions const & options) {
    auto extracted_values = extractValuesFromFilenames(options);

    if (extracted_values.empty()) {
        std::cerr << "Error: No valid numerical values extracted from filenames for CameraTimeFrame" << std::endl;
        return nullptr;
    }

    try {
        switch (options.mode) {
            case FilenameTimeFrameMode::FOUND_VALUES: {
                // Use sparse storage with extracted values
                std::cout << "Created sparse CameraTimeFrame from " << extracted_values.size()
                          << " filenames" << std::endl;
                return std::make_shared<CameraTimeFrame>(std::move(extracted_values));
            }
            case FilenameTimeFrameMode::ZERO_TO_MAX: {
                // Use dense storage from 0 to maximum value
                auto max_val = *std::max_element(extracted_values.begin(), extracted_values.end());
                std::cout << "Created dense CameraTimeFrame from 0 to " << max_val
                          << " (" << (max_val + 1) << " frames)" << std::endl;
                return std::make_shared<CameraTimeFrame>(0, max_val + 1, 1);
            }
            case FilenameTimeFrameMode::MIN_TO_MAX: {
                // Use dense storage from minimum to maximum value
                auto [min_it, max_it] = std::minmax_element(extracted_values.begin(), extracted_values.end());
                int64_t min_val = *min_it;
                int64_t max_val = *max_it;
                int64_t count = max_val - min_val + 1;
                std::cout << "Created dense CameraTimeFrame from " << min_val << " to " << max_val
                          << " (" << count << " frames)" << std::endl;
                return std::make_shared<CameraTimeFrame>(min_val, count, 1);
            }
        }
    } catch (std::exception const & e) {
        std::cerr << "Error creating CameraTimeFrame from filenames: " << e.what() << std::endl;
        return nullptr;
    }

    return nullptr;// Should never reach here
}

std::shared_ptr<UncalibratedTimeFrame> createUncalibratedTimeFrameFromFilenames(FilenameTimeFrameOptions const & options) {
    auto extracted_values = extractValuesFromFilenames(options);

    if (extracted_values.empty()) {
        std::cerr << "Error: No valid numerical values extracted from filenames for UncalibratedTimeFrame" << std::endl;
        return nullptr;
    }

    try {
        switch (options.mode) {
            case FilenameTimeFrameMode::FOUND_VALUES: {
                // Use sparse storage with extracted values
                std::cout << "Created sparse UncalibratedTimeFrame from " << extracted_values.size()
                          << " filenames" << std::endl;
                return std::make_shared<UncalibratedTimeFrame>(std::move(extracted_values));
            }
            case FilenameTimeFrameMode::ZERO_TO_MAX: {
                // Use dense storage from 0 to maximum value
                auto max_val = *std::max_element(extracted_values.begin(), extracted_values.end());
                std::cout << "Created dense UncalibratedTimeFrame from 0 to " << max_val
                          << " (" << (max_val + 1) << " indices)" << std::endl;
                return std::make_shared<UncalibratedTimeFrame>(0, max_val + 1, 1);
            }
            case FilenameTimeFrameMode::MIN_TO_MAX: {
                // Use dense storage from minimum to maximum value
                auto [min_it, max_it] = std::minmax_element(extracted_values.begin(), extracted_values.end());
                int64_t min_val = *min_it;
                int64_t max_val = *max_it;
                int64_t count = max_val - min_val + 1;
                std::cout << "Created dense UncalibratedTimeFrame from " << min_val << " to " << max_val
                          << " (" << count << " indices)" << std::endl;
                return std::make_shared<UncalibratedTimeFrame>(min_val, count, 1);
            }
        }
    } catch (std::exception const & e) {
        std::cerr << "Error creating UncalibratedTimeFrame from filenames: " << e.what() << std::endl;
        return nullptr;
    }

    return nullptr;// Should never reach here
}

}// namespace TimeFrameUtils