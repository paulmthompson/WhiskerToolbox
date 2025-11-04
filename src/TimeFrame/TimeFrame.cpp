#include "TimeFrame.hpp"

#include <cmath>
#include <cstdint>// For int64_t

TimeFrame::TimeFrame(std::vector<int> const & times) {
    _times = times;
    _total_frame_count = static_cast<int>(times.size());
}

int TimeFrame::getTimeAtIndex(TimeFrameIndex index) const {
    if (index < TimeFrameIndex(0) || static_cast<size_t>(index.getValue()) >= _times.size()) {
        std::cout << "Index " << index.getValue() << " out of range" << " for time frame of size " << _times.size() << std::endl;
        return 0;
    }
    return _times[static_cast<size_t>(index.getValue())];
}

TimeFrameIndex TimeFrame::getIndexAtTime(float time, bool preceding) const {
    // Binary search to find the index closest to the given time
    auto it = std::lower_bound(_times.begin(), _times.end(), time);

    // If exact match found
    if (it != _times.end() && static_cast<float>(*it) == time) {
        return TimeFrameIndex(std::distance(_times.begin(), it));
    }

    // If time is beyond the last time point
    if (it == _times.end()) {
        return TimeFrameIndex(static_cast<int64_t>(_times.size() - 1));
    }

    // If time is before the first time point
    if (it == _times.begin()) {
        return TimeFrameIndex(0);
    }

    // Find the closest time point, preceding by default
    // If preceding is false, we would return the next time point
    if (preceding) {
        auto prev = it - 1;
        if (std::abs(static_cast<float>(*prev) - time) <= std::abs(static_cast<float>(*it) - time)) {
            return TimeFrameIndex(std::distance(_times.begin(), prev));
        } else {
            return TimeFrameIndex(std::distance(_times.begin(), it));
        }
    } else {
        // If not preceding, return the next time point
        return TimeFrameIndex(std::distance(_times.begin(), it));
    }
}

int TimeFrame::checkFrameInbounds(int frame_id) const {

    if (frame_id < 0) {
        frame_id = 0;
    } else if (frame_id >= _total_frame_count) {
        frame_id = _total_frame_count;
    }
    return frame_id;
}

TimeFrameIndex getTimeIndexForSeries(TimeFrameIndex source_index,
                              TimeFrame const * source_time_frame,
                              TimeFrame const * destination_time_frame) {
    if (source_time_frame == destination_time_frame) {
        // Frames are the same. The time value can be used directly.
        return source_index;
    } else {
        auto destination_index = destination_time_frame->getIndexAtTime(static_cast<float>(source_index.getValue()));
        return destination_index;
    }
}

std::pair<TimeFrameIndex, TimeFrameIndex> convertTimeFrameRange(
        TimeFrameIndex const start_index,
        TimeFrameIndex const stop_index,
        TimeFrame const & from_time_frame,
        TimeFrame const & to_time_frame) {

    // Get the time values from the source timeframe
    auto start_time_value = from_time_frame.getTimeAtIndex(start_index);
    auto stop_time_value = from_time_frame.getTimeAtIndex(stop_index);

    // Convert to indices in the target timeframe
    auto target_start_index = to_time_frame.getIndexAtTime(static_cast<float>(start_time_value), false);
    auto target_stop_index = to_time_frame.getIndexAtTime(static_cast<float>(stop_time_value));

    return {target_start_index, target_stop_index};
}

// ========== Filename-based TimeFrame Creation Implementation ==========

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <regex>

std::shared_ptr<TimeFrame> createTimeFrameFromFilenames(FilenameTimeFrameOptions const & options) {
    try {
        // Check if directory exists - be more permissive for WSL/Windows mounted drives
        std::cout << "Checking directory: " << options.folder_path << std::endl;
        
        bool directory_accessible = false;
        try {
            // First try the standard check
            if (std::filesystem::exists(options.folder_path) && std::filesystem::is_directory(options.folder_path)) {
                directory_accessible = true;
                std::cout << "Directory verified via filesystem::exists" << std::endl;
            } else {
                // If that fails, try to iterate the directory as a fallback
                // This can work even when exists() fails on WSL mounted drives
                std::filesystem::directory_iterator dir_iter(options.folder_path);
                directory_accessible = true; // If we get here, directory is accessible
                std::cout << "Directory verified via directory_iterator (filesystem::exists failed)" << std::endl;
            }
        } catch (std::filesystem::filesystem_error const & e) {
            std::cerr << "Error accessing directory: " << options.folder_path << " - " << e.what() << std::endl;
            return nullptr;
        }
        
        if (!directory_accessible) {
            std::cerr << "Error: Directory is not accessible: " << options.folder_path << std::endl;
            return nullptr;
        }

        std::vector<int64_t> extracted_values;
        std::regex const pattern(options.regex_pattern);
        std::smatch match;

        // Iterate through files in directory
        for (auto const & entry: std::filesystem::directory_iterator(options.folder_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string const filename = entry.path().filename().string();

                        // Check file extension
            if (!options.file_extension.empty() && 
                !filename.ends_with(options.file_extension)) {
                continue;
            }

            // Extract numerical value using regex
            if (std::regex_search(filename, match, pattern)) {
                if (match.size() >= 2) {// Must have at least one capture group
                    try {
                        int64_t const value = std::stoll(match[1].str());
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

        if (extracted_values.empty()) {
            std::cerr << "Error: No valid numerical values extracted from filenames" << std::endl;
            return nullptr;
        }

        // Sort values if requested
        if (options.sort_ascending) {
            std::sort(extracted_values.begin(), extracted_values.end());
        }

        // Create TimeFrame based on mode
        std::vector<int> time_values;

        switch (options.mode) {
            case FilenameTimeFrameMode::FOUND_VALUES: {
                // Use only the extracted values
                time_values.reserve(extracted_values.size());
                for (int64_t val: extracted_values) {
                    time_values.push_back(static_cast<int>(val));
                }
                break;
            }
            case FilenameTimeFrameMode::ZERO_TO_MAX: {
                // Create range from 0 to maximum value
                auto max_val = *std::max_element(extracted_values.begin(), extracted_values.end());
                time_values.resize(static_cast<size_t>(max_val + 1));
                std::iota(time_values.begin(), time_values.end(), 0);
                break;
            }
            case FilenameTimeFrameMode::MIN_TO_MAX: {
                // Create range from minimum to maximum value
                auto [min_it, max_it] = std::minmax_element(extracted_values.begin(), extracted_values.end());
                int64_t const min_val = *min_it;
                int64_t const max_val = *max_it;
                auto const range_size = static_cast<size_t>(max_val - min_val + 1);
                time_values.resize(range_size);
                std::iota(time_values.begin(), time_values.end(), static_cast<int>(min_val));
                break;
            }
        }

        std::cout << "Created TimeFrame from " << extracted_values.size()
                  << " filenames with " << time_values.size() << " time points" << std::endl;

        return std::make_shared<TimeFrame>(time_values);

    } catch (std::exception const & e) {
        std::cerr << "Error creating TimeFrame from filenames: " << e.what() << std::endl;
        return nullptr;
    }
}
