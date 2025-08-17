#ifndef MEDIADATA_OPENCV_UTILITY_HPP
#define MEDIADATA_OPENCV_UTILITY_HPP

#include "CoreGeometry/ImageSize.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>

/**
 * @brief Convert a vector of uint8_t to an OpenCV Mat
 * @param vec Vector of uint8_t data
 * @param image_size Size of the image (width and height)
 * @return OpenCV Mat representing the image
 */
cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, ImageSize image_size);

#endif // MEDIADATA_OPENCV_UTILITY_HPP
