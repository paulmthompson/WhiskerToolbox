
#include "Media/Image_Data.hpp"

#include "utils/string_manip.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <iostream>
#include <set>


ImageData::ImageData() = default;

void ImageData::doLoadMedia(std::string const & dir_name) {

    auto file_extensions = std::set<std::string>{".png", ".jpg"};

    for (auto const & entry: std::filesystem::directory_iterator(dir_name)) {
        if (file_extensions.count(entry.path().extension().string())) {
            _image_paths.push_back(dir_name / entry.path());
        }
    }

    if (_image_paths.empty()) {
        std::cout << "Warning: No images found in directory with matching extensions ";
        for (auto const & i: file_extensions) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }

    setTotalFrameCount(static_cast<int>(_image_paths.size()));
}

cv::Mat convert_to_display_format(cv::Mat & image, ImageData::DisplayFormat format, ImageData::BitDepth& detected_depth) {
    cv::Mat converted_image;
    
    // Detect bit depth from input image
    if (image.depth() == CV_16U || image.depth() == CV_32F) {
        detected_depth = ImageData::BitDepth::Bit32;
    } else {
        detected_depth = ImageData::BitDepth::Bit8;
    }
    
    if (format == ImageData::DisplayFormat::Gray) {
        if (image.channels() > 1) {
            cv::cvtColor(image, converted_image, cv::COLOR_BGR2GRAY);
        } else {
            converted_image = image.clone();
        }
        
        // Normalize to 0-255 range based on bit depth
        if (detected_depth == ImageData::BitDepth::Bit32) {
            // Convert to float and normalize
            if (converted_image.depth() == CV_16U) {
                // Properly scale 16-bit to 0-255 range using temporary variable
                cv::Mat temp_float;
                converted_image.convertTo(temp_float, CV_32F, 255.0/65535.0);
                converted_image = temp_float;
            } else if (converted_image.depth() != CV_32F) {
                cv::Mat temp_float;
                converted_image.convertTo(temp_float, CV_32F);
                converted_image = temp_float;
            }
            // Only normalize if the data doesn't appear to be already in 0-255 range
            double min_val, max_val;
            cv::minMaxLoc(converted_image, &min_val, &max_val);
            if (max_val > 255.0 || min_val < 0.0) {
                cv::Mat temp_normalized;
                cv::normalize(converted_image, temp_normalized, 0.0, 255.0, cv::NORM_MINMAX, CV_32F);
                converted_image = temp_normalized;
            }
        } else {
            // Ensure 8-bit output
            if (converted_image.depth() != CV_8U) {
                converted_image.convertTo(converted_image, CV_8U);
            }
        }
    } else if (format == ImageData::DisplayFormat::Color) {
        if (image.channels() == 1) {
            cv::cvtColor(image, converted_image, cv::COLOR_GRAY2BGRA);
        } else {
            cv::cvtColor(image, converted_image, cv::COLOR_BGR2BGRA);
        }
        
        // Color images are typically kept as 8-bit for now
        if (converted_image.depth() != CV_8U) {
            converted_image.convertTo(converted_image, CV_8U);
        }
        detected_depth = ImageData::BitDepth::Bit8;
    }
    return converted_image;
}

void ImageData::doLoadFrame(int frame_id) {

    if (frame_id > _image_paths.size()) {
        std::cout << "Error: Requested frame ID is larger than the number of frames in Media Data" << std::endl;
        return;
    }

    // Load image with unchanged depth to detect bit depth
    auto loaded_image = cv::imread(_image_paths[frame_id].string(), cv::IMREAD_UNCHANGED);

    updateHeight(loaded_image.rows);
    updateWidth(loaded_image.cols);

    BitDepth detected_depth;
    auto converted_image = convert_to_display_format(loaded_image, this->getFormat(), detected_depth);
    
    setBitDepth(detected_depth);

    size_t const num_bytes = converted_image.total() * converted_image.elemSize();
    
    if (detected_depth == BitDepth::Bit32) {
        // Data is float, use 32-bit setRawData
        auto* float_ptr = reinterpret_cast<float*>(converted_image.data);
        this->setRawData(MediaStorage::ImageData32(float_ptr, float_ptr + converted_image.total()));
    } else {
        // Data is uint8_t, use 8-bit setRawData
        auto* uint8_ptr = static_cast<uint8_t*>(converted_image.data);
        this->setRawData(MediaStorage::ImageData8(uint8_ptr, uint8_ptr + num_bytes));
    }
}

std::string ImageData::GetFrameID(int frame_id) const {
    return _image_paths[frame_id].filename().string();
}

int ImageData::getFrameIndexFromNumber(int frame_id) {
    for (std::size_t i = 0; i < _image_paths.size(); i++) {
        auto image_frame_id = extract_numbers_from_string(_image_paths[i].filename().string());
        if (std::stoi(image_frame_id) == frame_id) {
            return static_cast<int>(i);
        }
    }
    std::cout << "No matching frame found for requested ID" << std::endl;
    return 0;
}

void ImageData::setImagePaths(std::vector<std::filesystem::path> const & image_paths) {
    _image_paths = image_paths;
    setTotalFrameCount(static_cast<int>(_image_paths.size()));
}
