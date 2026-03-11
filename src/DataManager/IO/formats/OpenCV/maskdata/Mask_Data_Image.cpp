#include "Mask_Data_Image.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Masks/Mask_Data.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "utils/string_manip.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <regex>
#include <vector>

std::shared_ptr<MaskData> load(ImageMaskLoaderOptions const & opts) {
    auto mask_data = std::make_shared<MaskData>();

    // Validate directory
    if (!std::filesystem::exists(opts.directory_path) || !std::filesystem::is_directory(opts.directory_path)) {
        std::cerr << "Error: Directory does not exist: " << opts.directory_path << std::endl;
        return mask_data;
    }

    // Get list of image files matching the pattern
    std::vector<std::filesystem::path> image_files;
    std::string const pattern = opts.file_pattern;

    // Convert wildcard pattern to regex
    std::string const regex_pattern = std::regex_replace(pattern, std::regex("\\*"), ".*");
    std::regex const file_regex(regex_pattern, std::regex_constants::icase);

    for (auto const & entry: std::filesystem::directory_iterator(opts.directory_path)) {
        if (entry.is_regular_file()) {
            std::string const filename = entry.path().filename().string();
            if (std::regex_match(filename, file_regex)) {
                image_files.push_back(entry.path());
            }
        }
    }

    if (image_files.empty()) {
        std::cerr << "Warning: No image files found matching pattern '" << opts.file_pattern
                  << "' in directory: " << opts.directory_path << std::endl;
        return mask_data;
    }

    // Sort files to ensure consistent ordering
    std::sort(image_files.begin(), image_files.end());

    int files_loaded = 0;
    int files_skipped = 0;

    std::cout << "Loading mask images from directory: " << opts.directory_path << std::endl;
    std::cout << "Found " << image_files.size() << " image files matching pattern: " << opts.file_pattern << std::endl;

    std::vector<ImageSize> mask_sizes;

    for (auto const & file_path: image_files) {
        std::string const filename = file_path.filename().string();
        std::string stem = file_path.stem().string();// Remove extension

        // Remove prefix if specified
        if (!opts.filename_prefix.empty()) {
            if (stem.find(opts.filename_prefix) != 0) {
                std::cerr << "Warning: File '" << filename
                          << "' does not start with expected prefix '" << opts.filename_prefix << "'" << std::endl;
                files_skipped++;
                continue;
            }
            stem = stem.substr(opts.filename_prefix.length());// Remove prefix
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
        cv::Mat image = cv::imread(file_path.string(), cv::IMREAD_GRAYSCALE);
        if (image.empty()) {
            std::cerr << "Warning: Could not load image: " << file_path.string() << std::endl;
            files_skipped++;
            continue;
        }

        // Extract mask points from image
        std::vector<Point2D<uint32_t>> mask_points;
        int const width = image.cols;
        int const height = image.rows;
        mask_sizes.push_back(ImageSize{width, height});

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Get pixel intensity (0-255)
                uint8_t const pixel_value = image.at<uint8_t>(y, x);

                // Apply threshold and inversion logic
                bool is_mask_pixel;
                if (opts.invert_mask) {
                    is_mask_pixel = pixel_value < opts.threshold_value;
                } else {
                    is_mask_pixel = pixel_value >= opts.threshold_value;
                }

                if (is_mask_pixel) {
                    //mask_points.emplace_back(static_cast<float>(x), static_cast<float>(y)); // This fails on mac
                    mask_points.emplace_back(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                }
            }
        }

        // Add mask to data if we have points
        if (!mask_points.empty()) {
            mask_data->addAtTime(TimeFrameIndex(static_cast<size_t>(frame_number)),
                                 Mask2D(std::move(mask_points)),
                                 NotifyObservers::No);
            files_loaded++;
        } else {
            std::cout << "Warning: No mask pixels found in image: " << filename << std::endl;
            files_skipped++;
        }
    }

    // If all masks loaded have same size, change image size to mask data to that size
    if (!mask_sizes.empty()) {
        auto first_size = mask_sizes[0];
        if (std::all_of(mask_sizes.begin(), mask_sizes.end(), [&](ImageSize const & size) {
                return size == first_size;
            })) {
            mask_data->setImageSize(first_size);
        }
    }

    // Notify observers once at the end
    if (files_loaded > 0) {
        mask_data->notifyObservers();
    }

    std::cout << "Image mask loading complete: " << files_loaded << " files loaded";
    if (files_skipped > 0) {
        std::cout << ", " << files_skipped << " files skipped";
    }
    std::cout << std::endl;

    return mask_data;
}

bool save(MaskData const * mask_data, ImageMaskSaverOptions const & opts) {
    if (!mask_data) {
        spdlog::error("MaskData pointer is null");
        return false;
    }

    // Create output directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(opts.parent_dir, ec);
    if (ec) {
        spdlog::error("Failed to create directory '{}': {}", opts.parent_dir, ec.message());
        return false;
    }

    int files_saved = 0;
    int files_skipped = 0;
    bool had_error = false;

    auto mask_data_size = mask_data->getImageSize();

    spdlog::info("Saving mask images to directory: {}", opts.parent_dir);

    // Iterate through all masks with their timestamps
    for (auto const & [time, entity_id, mask]: mask_data->flattened_data()) {

        // Create output image with desired dimensions
        cv::Mat output_img = cv::Mat::zeros(opts.image_height, opts.image_width, CV_8UC1);
        output_img.setTo(opts.background_value);

        // Resize each mask and draw it on the output image
        ImageSize const dest_size{opts.image_width, opts.image_height};

        // Resize the mask using our new resize function
        Mask2D const resized_mask = resize_mask(mask, mask_data_size, dest_size);

        // Draw the resized mask on the output image
        for (Point2D<uint32_t> const & point: resized_mask) {
            int const x = static_cast<int>(point.x);
            int const y = static_cast<int>(point.y);

            // Check bounds (should already be valid after resize, but be safe)
            if (x >= 0 && x < opts.image_width && y >= 0 && y < opts.image_height) {
                output_img.at<uint8_t>(y, x) = static_cast<uint8_t>(opts.mask_value);
            }
        }

        // Generate filename using the pad_frame_id utility
        std::string filename;
        if (!opts.filename_prefix.empty()) {
            filename = opts.filename_prefix;
        }

        // Add zero-padded frame number
        filename += pad_frame_id(time.getValue(), opts.frame_number_padding);

        // Add extension based on format
        std::string extension = "." + opts.image_format;
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        filename += extension;

        // Full path
        std::filesystem::path const full_path = std::filesystem::path(opts.parent_dir) / filename;

        // Check if file exists and handle according to overwrite setting
        if (std::filesystem::exists(full_path) && !opts.overwrite_existing) {
            spdlog::debug("Skipping existing file: {}", full_path.string());
            files_skipped++;
            continue;
        }

        // Atomic write: save to temp file with correct extension, then rename over target
        std::filesystem::path const temp_path =
                std::filesystem::path(opts.parent_dir) / ("_tmp_" + filename);

        if (cv::imwrite(temp_path.string(), output_img)) {
            std::error_code rename_ec;
            std::filesystem::rename(temp_path, full_path, rename_ec);
            if (rename_ec) {
                spdlog::error("Failed to rename temp file to '{}': {}",
                              full_path.string(), rename_ec.message());
                std::filesystem::remove(temp_path, rename_ec);
                had_error = true;
            } else {
                files_saved++;
            }
        } else {
            spdlog::warn("Failed to save image: {}", full_path.string());
            std::error_code remove_ec;
            std::filesystem::remove(temp_path, remove_ec);
            had_error = true;
        }
    }

    spdlog::info("Image mask saving complete: {} files saved, {} files skipped",
                 files_saved, files_skipped);

    return !had_error;
}
