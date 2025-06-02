#ifndef OPENCV_UTILITY_HPP
#define OPENCV_UTILITY_HPP

#include "ImageSize/ImageSize.hpp"
#include "Points/points.hpp"
#include "ProcessingOptions.hpp"

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Image loading and conversion functions
cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, ImageSize image_size);

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>>& vec, ImageSize image_size);

void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat & mat, ImageSize image_size);

std::vector<Point2D<float>> create_mask(cv::Mat const & mat);

/**
 * @brief Load mask from image file
 * @param filename Path to image file
 * @param invert Whether to invert the mask
 * @return OpenCV matrix containing the mask
 */
cv::Mat load_mask_from_image(std::string const & filename, bool invert=false);

// Basic image processing functions
void grow_mask(cv::Mat & mat, int dilation_size);

void median_blur(cv::Mat & mat, int kernel_size);

// Enhanced image processing functions using options structures

/**
 * @brief Apply linear contrast and brightness transformation
 * @param mat Image matrix to transform (modified in-place)
 * @param options ContrastOptions containing alpha (contrast) and beta (brightness) parameters
 */
void linear_transform(cv::Mat & mat, ContrastOptions const& options);

/**
 * @brief Apply gamma correction transformation
 * @param mat Image matrix to transform (modified in-place)
 * @param options GammaOptions containing gamma correction value
 */
void gamma_transform(cv::Mat & mat, GammaOptions const& options);

/**
 * @brief Apply CLAHE (Contrast Limited Adaptive Histogram Equalization)
 * @param mat Image matrix to transform (modified in-place)
 * @param options ClaheOptions containing clip limit and grid size parameters
 */
void clahe(cv::Mat & mat, ClaheOptions const& options);

/**
 * @brief Apply image sharpening filter
 * @param mat Image matrix to transform (modified in-place)
 * @param options SharpenOptions containing sigma parameter for sharpening
 */
void sharpen_image(cv::Mat& mat, SharpenOptions const& options);

/**
 * @brief Apply bilateral filtering for noise reduction while preserving edges
 * @param mat Image matrix to transform (modified in-place)
 * @param options BilateralOptions containing diameter, color sigma, and spatial sigma parameters
 */
void bilateral_filter(cv::Mat& mat, BilateralOptions const& options);

/**
 * @brief Apply median filtering for noise reduction
 * @param mat Image matrix to transform (modified in-place)
 * @param options MedianOptions containing kernel size parameter
 */
void median_filter(cv::Mat& mat, MedianOptions const& options);

/**
 * @brief Apply dilation or erosion to a point-based mask
 * @param mask Input mask as vector of 2D points
 * @param image_size Size of the image/mask canvas
 * @param options Dilation options specifying operation parameters
 * @return Modified mask as vector of 2D points
 */
std::vector<Point2D<float>> dilate_mask(std::vector<Point2D<float>> const& mask, ImageSize image_size, MaskDilationOptions const& options);

/**
 * @brief Apply dilation or erosion to a cv::Mat mask
 * @param mat Input/output mask matrix (modified in place)
 * @param options Dilation options specifying operation parameters
 */
void dilate_mask_mat(cv::Mat& mat, MaskDilationOptions const& options);

#endif // OPENCV_UTILITY_HPP
