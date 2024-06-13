#ifndef OPENCV_UTILITY_HPP
#define OPENCV_UTILITY_HPP

#include "Points/Point_Data.hpp"

#include "opencv2/core/mat.hpp"

#include <vector>

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height);
void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat & mat, const int width, const int height);
std::vector<Point2D<float>> create_mask(cv::Mat const & mat);

#endif // OPENCV_UTILITY_HPP
