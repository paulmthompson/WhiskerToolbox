
#include "magic_eraser.hpp"

#include "ImageProcessing/OpenCVUtility.hpp"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/photo.hpp"


std::vector<uint8_t> MagicEraser::applyMagicEraser(std::vector<uint8_t> & image, ImageSize const image_size, std::vector<uint8_t> & mask) {

    auto output = apply_magic_eraser(image, image_size, mask);
    return output;
}

cv::Mat MagicEraser::_createBackgroundImage(std::vector<uint8_t> const & image, ImageSize const image_size) {

    static_cast<void>(image_size);

    // Convert the input vector to a cv::Mat
    cv::Mat const inputImage{image, false};

    // The number of rows = height, number of columns = width
    // Apply median blur filter
    cv::Mat medianBlurredImage;
    cv::medianBlur(inputImage, medianBlurredImage, 25);

    return medianBlurredImage;
}

std::vector<uint8_t> apply_magic_eraser(std::vector<uint8_t> & image, ImageSize const image_size, std::vector<uint8_t> & mask) {
    // Convert the input vector to a cv::Mat
    cv::Mat const inputImage = ImageProcessing::convert_vector_to_mat(image, image_size);
    cv::Mat inputImage3Channel;
    cv::cvtColor(inputImage, inputImage3Channel, cv::COLOR_GRAY2BGR);

    // Apply median blur filter
    cv::Mat medianBlurredImage;
    cv::medianBlur(inputImage, medianBlurredImage, 25);
    cv::Mat medianBlurredImage3Channel;
    cv::cvtColor(medianBlurredImage, medianBlurredImage3Channel, cv::COLOR_GRAY2BGR);

    cv::Mat const maskImage = ImageProcessing::convert_vector_to_mat(mask, image_size);

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
    cv::Point const center(image_size.width / 2, image_size.height / 2);

    cv::seamlessClone(medianBlurredImage3Channel, inputImage3Channel, mask3Channel, center, outputImage, cv::NORMAL_CLONE);

    cv::Mat outputImageGray;
    cv::cvtColor(outputImage, outputImageGray, cv::COLOR_BGR2GRAY);
    auto output = std::vector<uint8_t>(static_cast<size_t>(image_size.height * image_size.width));

    ImageProcessing::convert_mat_to_vector(output, outputImageGray, image_size);

    return output;
}
