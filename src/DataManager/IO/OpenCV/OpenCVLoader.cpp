#include "OpenCVLoader.hpp"
#include "../DataFactory.hpp"
#include "../LoaderRegistry.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/masks.hpp"
#include "utils/string_manip.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <vector>

std::string OpenCVLoader::getFormatId() const {
    return "image";
}

bool OpenCVLoader::supportsDataType(IODataType data_type) const {
    using enum IODataType;
    switch (data_type) {
        case Mask:
            return true;
        default:
            return false;
    }
}

LoadResult OpenCVLoader::loadData(
    std::string const& file_path,
    IODataType data_type,
    nlohmann::json const& config,
    DataFactory* factory
) const {
    if (!factory) {
        return LoadResult("Factory is null");
    }
    
    try {
        using enum IODataType;
        if (data_type == Mask) {
            return loadMaskData(file_path, config, factory);
        }
        return LoadResult("Unsupported data type for OpenCV loader");
    } catch (std::exception const& e) {
        return LoadResult("OpenCV loading error: " + std::string(e.what()));
    }
}

LoadResult OpenCVLoader::loadMaskData(
    std::string const& file_path,
    nlohmann::json const& config,
    DataFactory* factory
) const {
    try {
        // Extract configuration with defaults
        std::string file_pattern = "*.png";
        std::string filename_prefix = "";
        int threshold_value = 128;
        bool invert_mask = false;
        
        if (config.contains("file_pattern")) {
            file_pattern = config["file_pattern"].get<std::string>();
        }
        if (config.contains("filename_prefix")) {
            filename_prefix = config["filename_prefix"].get<std::string>();
        }
        if (config.contains("threshold_value")) {
            threshold_value = config["threshold_value"].get<int>();
        }
        if (config.contains("invert_mask")) {
            invert_mask = config["invert_mask"].get<bool>();
        }
        
        // The file_path should be a directory containing image files
        std::string directory_path = file_path;
        
        // Validate directory
        if (!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path)) {
            return LoadResult("Error: Directory does not exist: " + directory_path);
        }

        // Get list of image files matching the pattern
        std::vector<std::filesystem::path> image_files;

        // Convert wildcard pattern to regex
        std::string regex_pattern = std::regex_replace(file_pattern, std::regex("\\*"), ".*");
        std::regex file_regex(regex_pattern, std::regex_constants::icase);

        for (auto const & entry: std::filesystem::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, file_regex)) {
                    image_files.push_back(entry.path());
                }
            }
        }

        if (image_files.empty()) {
            return LoadResult("Warning: No image files found matching pattern '" + file_pattern + "' in directory: " + directory_path);
        }

        // Sort files to ensure consistent ordering
        std::sort(image_files.begin(), image_files.end());

        int files_loaded = 0;
        int files_skipped = 0;

        std::cout << "Loading mask images from directory: " << directory_path << std::endl;
        std::cout << "Found " << image_files.size() << " image files matching pattern: " << file_pattern << std::endl;

        // Convert to raw data format
        MaskDataRaw raw_data;

        for (auto const & file_path_entry: image_files) {
            std::string filename = file_path_entry.filename().string();
            std::string stem = file_path_entry.stem().string(); // Remove extension

            // Remove prefix if specified
            if (!filename_prefix.empty()) {
                if (stem.find(filename_prefix) != 0) {
                    std::cerr << "Warning: File '" << filename
                              << "' does not start with expected prefix '" << filename_prefix << "'" << std::endl;
                    files_skipped++;
                    continue;
                }
                stem = stem.substr(filename_prefix.length()); // Remove prefix
            }

            // Parse frame number
            int frame_number;
            try {
                frame_number = std::stoi(stem);
            } catch (std::exception const & e) {
                std::cerr << "Warning: Could not parse frame number from filename: " << filename << std::endl;
                files_skipped++;
                continue;
            }

            // Load the image using OpenCV
            cv::Mat image = cv::imread(file_path_entry.string(), cv::IMREAD_GRAYSCALE);
            if (image.empty()) {
                std::cerr << "Warning: Could not load image: " << file_path_entry.string() << std::endl;
                files_skipped++;
                continue;
            }

            // Extract mask points from image
            Mask2D mask_points;
            int const width = image.cols;
            int const height = image.rows;

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    // Get pixel intensity (0-255)
                    uint8_t pixel_value = image.at<uint8_t>(y, x);

                    // Apply threshold and inversion logic
                    bool is_mask_pixel;
                    if (invert_mask) {
                        is_mask_pixel = pixel_value < threshold_value;
                    } else {
                        is_mask_pixel = pixel_value >= threshold_value;
                    }

                    if (is_mask_pixel) {
                        mask_points.push_back(Point2D<uint32_t>{static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
                    }
                }
            }

            // Add mask to raw data if we have points
            if (!mask_points.empty()) {
                raw_data.time_masks[frame_number] = {std::move(mask_points)};
                files_loaded++;
                
                // Store image dimensions (use the last loaded image dimensions)
                raw_data.image_width = static_cast<uint32_t>(width);
                raw_data.image_height = static_cast<uint32_t>(height);
            } else {
                std::cout << "Warning: No mask pixels found in image: " << filename << std::endl;
                files_skipped++;
            }
        }

        if (files_loaded == 0) {
            return LoadResult("No valid mask data found in any image files");
        }

        // Extract image size from config if available (overrides detected size)
        if (config.contains("image_width")) {
            raw_data.image_width = config["image_width"].get<uint32_t>();
        }
        if (config.contains("image_height")) {
            raw_data.image_height = config["image_height"].get<uint32_t>();
        }

        // Create MaskData using factory
        auto mask_data = factory->createMaskDataFromRaw(raw_data);

        std::cout << "OpenCV image mask loading complete: " << files_loaded << " files loaded";
        if (files_skipped > 0) {
            std::cout << ", " << files_skipped << " files skipped";
        }
        std::cout << std::endl;

        return LoadResult(std::move(mask_data));
        
    } catch (std::exception const& e) {
        return LoadResult("Error loading OpenCV mask data: " + std::string(e.what()));
    }
}

// Note: OpenCV registration is now handled by the LoaderRegistration system
// An OpenCVFormatLoader would need to be created to wrap this class

// Export function to ensure linking
extern "C" void ensure_opencv_loader_registration() {
    // This function doesn't need to do anything - its existence
    // forces the linker to include this object file
}
