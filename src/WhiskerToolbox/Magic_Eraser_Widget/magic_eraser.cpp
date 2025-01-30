
#include "magic_eraser.hpp"

#include "DataManager/utils/opencv_utility.hpp"

#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/photo.hpp"

MagicEraser::MagicEraser()
{

}

std::vector<uint8_t> MagicEraser::applyMagicEraser(std::vector<uint8_t> & image, int width, int height, std::vector<uint8_t> & mask)
{

    auto output = apply_magic_eraser(image,width,height,mask);
    return output;
}

cv::Mat MagicEraser::_createBackgroundImage(std::vector<uint8_t> const & image, int width, int height)
{
    // Convert the input vector to a cv::Mat
    cv::Mat inputImage{image, false};

    // The number of rows = height, number of columns = width
    // Apply median blur filter
    cv::Mat medianBlurredImage;
    cv::medianBlur(inputImage, medianBlurredImage, 25);

    return medianBlurredImage;
}

std::vector<uint8_t> apply_magic_eraser(std::vector<uint8_t> & image, int width, int height, std::vector<uint8_t> & mask)
{
    // Convert the input vector to a cv::Mat
    cv::Mat inputImage = convert_vector_to_mat(image, width, height );
    cv::Mat inputImage3Channel;
    cv::cvtColor(inputImage, inputImage3Channel, cv::COLOR_GRAY2BGR);

    // Apply median blur filter
    cv::Mat medianBlurredImage;
    cv::medianBlur(inputImage, medianBlurredImage, 25);
    cv::Mat medianBlurredImage3Channel;
    cv::cvtColor(medianBlurredImage, medianBlurredImage3Channel, cv::COLOR_GRAY2BGR);

    cv::Mat maskImage = convert_vector_to_mat(mask, width, height);

    cv::Mat smoothedMask;
    cv::GaussianBlur(maskImage, smoothedMask, cv::Size(15, 15), 0);

    cv::threshold(smoothedMask, smoothedMask, 1, 255, cv::THRESH_BINARY);

    for (int y = 0; y < smoothedMask.rows; ++y) {
        for (int x = 0; x < smoothedMask.cols; ++x) {
            if (smoothedMask.at<uint8_t>(y, x) == 0) {
                smoothedMask.at<uint8_t>(y, x) = 1;
            }
        }
    }

    cv::Mat mask3Channel;
    cv::cvtColor(smoothedMask, mask3Channel, cv::COLOR_GRAY2BGR);

    cv::Mat outputImage;
    cv::Point center(width / 2, height / 2);

    cv::seamlessClone(medianBlurredImage3Channel, inputImage3Channel, mask3Channel, center, outputImage, cv::NORMAL_CLONE);

    cv::Mat outputImageGray;
    cv::cvtColor(outputImage, outputImageGray, cv::COLOR_BGR2GRAY);
    auto output = std::vector<uint8_t>(height * width);

    convert_mat_to_vector(output, outputImageGray, width, height);

    return output;
}
