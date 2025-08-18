#include "Image_Data_Loader.hpp"

#include "Media/Image_Data.hpp"
#include "utils/string_manip.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <vector>

std::shared_ptr<ImageData> load(ImageLoaderOptions const & opts) {
    auto image_data = std::make_shared<ImageData>();

    // Validate directory
    if (!std::filesystem::exists(opts.directory_path) || !std::filesystem::is_directory(opts.directory_path)) {
        std::cerr << "Error: Directory does not exist: " << opts.directory_path << std::endl;
        return image_data;
    }

    // Get list of image files matching the criteria
    std::vector<std::filesystem::path> image_files;
    std::regex filename_regex;

    // Compile regex pattern if provided
    if (!opts.filename_pattern.empty()) {
        try {
            filename_regex = std::regex(opts.filename_pattern);
        } catch (std::regex_error const & e) {
            std::cerr << "Error: Invalid regex pattern '" << opts.filename_pattern << "': " << e.what() << std::endl;
            return image_data;
        }
    }

    // Function to check if a file should be included
    auto should_include_file = [&](std::filesystem::path const & file_path) -> bool {
        // Check file extension
        std::string extension = file_path.extension().string();
        if (opts.file_extensions.find(extension) == opts.file_extensions.end()) {
            return false;
        }

        // Check filename pattern if provided
        if (!opts.filename_pattern.empty()) {
            std::string filename = file_path.filename().string();
            if (!std::regex_search(filename, filename_regex)) {
                return false;
            }
        }

        return true;
    };

    // Collect files
    if (opts.recursive_search) {
        for (auto const & entry: std::filesystem::recursive_directory_iterator(opts.directory_path)) {
            if (entry.is_regular_file() && should_include_file(entry.path())) {
                image_files.push_back(entry.path());
            }
        }
    } else {
        for (auto const & entry: std::filesystem::directory_iterator(opts.directory_path)) {
            if (entry.is_regular_file() && should_include_file(entry.path())) {
                image_files.push_back(entry.path());
            }
        }
    }

    if (image_files.empty()) {
        std::cout << "Warning: No image files found in directory with matching criteria: " << opts.directory_path << std::endl;
        std::cout << "  Extensions: ";
        for (auto const & ext: opts.file_extensions) {
            std::cout << ext << " ";
        }
        if (!opts.filename_pattern.empty()) {
            std::cout << "  Pattern: " << opts.filename_pattern;
        }
        std::cout << std::endl;
        return image_data;
    }

    // Sort files if requested
    if (opts.sort_by_name) {
        std::sort(image_files.begin(), image_files.end());
    }

    // Set up the ImageData object
    image_data->setFormat(opts.display_format);

    // Store the file paths in the ImageData object using the new method
    image_data->setImagePaths(image_files);

    std::cout << "Loaded " << image_files.size() << " image files from " << opts.directory_path << std::endl;

    return image_data;
}