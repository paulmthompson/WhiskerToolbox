#include "utils/opencv_utility.hpp"

#include <iostream>

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, ImageSize image_size) {
    // Determine the number of channels
    int channels = static_cast<int>(vec.size()) / (image_size.width * image_size.height);

    // Determine the OpenCV type based on the number of channels
    int cv_type;
    if (channels == 1) {
        cv_type = CV_8UC1; // Grayscale
    } else if (channels == 3) {
        cv_type = CV_8UC3; // BGR
    } else if (channels == 4) {
        cv_type = CV_8UC4; // BGRA
    } else {
        std::cerr << "Unsupported number of channels: " << channels << std::endl;
        return cv::Mat(); // Return an empty matrix
    }

    return cv::Mat(image_size.height, image_size.width, cv_type, vec.data());
}
