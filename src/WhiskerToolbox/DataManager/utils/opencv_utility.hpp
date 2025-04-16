#ifndef OPENCV_UTILITY_HPP
#define OPENCV_UTILITY_HPP

#include "ImageSize/ImageSize.hpp"
#include "Points/Point_Data.hpp"

#include "opencv2/core/mat.hpp"

#include <vector>
#include <string>

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, ImageSize image_size);

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>>& vec, ImageSize image_size);

void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat & mat, ImageSize image_size);

std::vector<Point2D<float>> create_mask(cv::Mat const & mat);

/**
 *
 * Loads in image that is black and white. Data manager will assume that masked region is "black"
 * so invert option can be used to use "white" mask images.
 *
 * @brief load_mask_from_image
 * @param filename
 * @param invert
 * @return
 */
cv::Mat load_mask_from_image(std::string const & filename, bool invert=false);

void grow_mask(cv::Mat & mat, int dilation_size);

void median_blur(cv::Mat & mat, int kernel_size);

void linear_transform(cv::Mat & mat, double alpha, int beta);

void gamma_transform(cv::Mat & mat, double gamma);

void clahe(cv::Mat & mat, double clip_limit, int grid_size);

void sharpen_image(cv::Mat& img, double sigma = 3.0);

void bilateral_filter(cv::Mat& img, int d = 5, double sigmaColor = 75.0, double sigmaSpace = 75.0);

#endif // OPENCV_UTILITY_HPP
