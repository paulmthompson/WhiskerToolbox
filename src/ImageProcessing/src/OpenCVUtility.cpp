#include "ImageProcessing/OpenCVUtility.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/photo.hpp>
#include <iostream>

namespace ImageProcessing {

cv::Mat load_mask_from_image(std::string const & filename, bool const invert) {
    cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);

    if (image.empty()) {
        std::cerr << "Could not open or find the image: " << filename << std::endl;
        return cv::Mat(); // Return an empty matrix
    }

    if (invert) {
        cv::bitwise_not(image, image);
    }

    return image;
}

cv::Mat convert_vector_to_mat(std::vector<uint8_t> & vec, ImageSize const image_size) {
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

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>> & vec, ImageSize const image_size) {
    cv::Mat mask_image(image_size.height, image_size.width, CV_8UC1, cv::Scalar(0));

    for (auto const & point : vec) {
        int x = static_cast<int>(std::round(point.x));
        int y = static_cast<int>(std::round(point.y));

        // Check bounds
        if (x >= 0 && x < image_size.width && y >= 0 && y < image_size.height) {
            mask_image.at<uint8_t>(y, x) = 255; // Set pixel to white (255)
        }
    }

    return mask_image;
}

void convert_mat_to_vector(std::vector<uint8_t> & vec, cv::Mat & mat, ImageSize const image_size) {
    // Check if the matrix has the expected size
    if (mat.rows != image_size.height || mat.cols != image_size.width) {
        std::cerr << "Matrix size does not match the expected image size." << std::endl;
        return;
    }

    // Resize the vector to hold all the pixel data
    vec.resize(mat.rows * mat.cols * mat.channels());

    // Copy data from the matrix to the vector
    std::memcpy(vec.data(), mat.data, vec.size());
}

std::vector<Point2D<uint32_t>> create_mask(cv::Mat const & mat) {
    std::vector<Point2D<uint32_t>> mask;

    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            if (mat.at<uint8_t>(y, x) > 0) { // Assuming a binary mask where 0 is background
                mask.push_back({static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
            }
        }
    }

    return mask;
}

void grow_mask(cv::Mat & mat, int const dilation_size) {
    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                cv::Size(2 * dilation_size + 1, 2 * dilation_size + 1),
                                                cv::Point(dilation_size, dilation_size));
    cv::dilate(mat, mat, element);
}

void median_blur(cv::Mat & mat, int const kernel_size) {
    cv::medianBlur(mat, mat, kernel_size);
}

// New options-based implementations
void linear_transform(cv::Mat & mat, ContrastOptions const& options) {
    mat.convertTo(mat, -1, options.alpha, options.beta);
}

void gamma_transform(cv::Mat & mat, GammaOptions const& options) {
    cv::Mat lookupTable(1, 256, CV_8U);
    uchar* p = lookupTable.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, options.gamma) * 255.0);
    }
    cv::LUT(mat, lookupTable, mat);
}

void clahe(cv::Mat & mat, ClaheOptions const& options) {
    cv::Ptr<cv::CLAHE> clahe_ptr = cv::createCLAHE(options.clip_limit, cv::Size(options.grid_size, options.grid_size));
    clahe_ptr->apply(mat, mat);
}

void sharpen_image(cv::Mat & mat, SharpenOptions const& options) {
    cv::Mat blurred;
    cv::GaussianBlur(mat, blurred, cv::Size(0, 0), options.sigma);
    cv::addWeighted(mat, 1.0 + 1.0, blurred, -1.0, 0, mat);
}

void bilateral_filter(cv::Mat & mat, BilateralOptions const& options) {
    cv::Mat temp;
    cv::bilateralFilter(mat, temp, options.diameter, options.sigma_color, options.sigma_spatial);
    mat = temp;
}

void median_filter(cv::Mat & mat, MedianOptions const& options) {
    if (options.kernel_size >= 3 && options.kernel_size % 2 == 1) {
        cv::medianBlur(mat, mat, options.kernel_size);
    }
}

std::vector<Point2D<uint32_t>> dilate_mask(std::vector<Point2D<uint32_t>> const& mask, ImageSize image_size, MaskDilationOptions const& options) {
    if (mask.empty() || !options.active) {
        return mask;
    }
    
    // Convert point-based mask to cv::Mat
    cv::Mat mask_mat = cv::Mat::zeros(image_size.height, image_size.width, CV_8UC1);
    
    // Fill the mask matrix with points
    for (auto const& point : mask) {
        int x = static_cast<int>(std::round(point.x));
        int y = static_cast<int>(std::round(point.y));
        if (x >= 0 && x < image_size.width && y >= 0 && y < image_size.height) {
            mask_mat.at<uint8_t>(y, x) = 255;
        }
    }
    
    // Apply dilation/erosion
    dilate_mask_mat(mask_mat, options);
    
    // Convert back to point-based representation
    return create_mask(mask_mat);
}

void dilate_mask_mat(cv::Mat& mat, MaskDilationOptions const& options) {
    if (!options.active) {
        return;
    }
    
    int kernel_size = options.is_grow_mode ? options.grow_size : options.shrink_size;
    
    if (kernel_size <= 0) {
        return;
    }
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size));
    
    if (options.is_grow_mode) {
        cv::dilate(mat, mat, kernel);
    } else {
        cv::erode(mat, mat, kernel);
    }
}

std::vector<uint8_t> apply_magic_eraser_with_options(std::vector<uint8_t> const& image, 
                                                    ImageSize image_size, 
                                                    std::vector<uint8_t> const& mask,
                                                    MagicEraserOptions const& options) {
    // Create mutable copies for the conversion functions
    std::vector<uint8_t> image_copy = image;
    std::vector<uint8_t> mask_copy = mask;
    
    // Convert the input vector to a cv::Mat
    cv::Mat inputImage = convert_vector_to_mat(image_copy, image_size);
    cv::Mat inputImage3Channel;
    cv::cvtColor(inputImage, inputImage3Channel, cv::COLOR_GRAY2BGR);

    // Apply median blur filter with configurable size
    cv::Mat medianBlurredImage;
    int filter_size = options.median_filter_size;
    // Ensure filter size is odd and within valid range
    if (filter_size % 2 == 0) filter_size += 1;
    if (filter_size < 3) filter_size = 3;
    if (filter_size > 101) filter_size = 101;
    
    cv::medianBlur(inputImage, medianBlurredImage, filter_size);
    cv::Mat medianBlurredImage3Channel;
    cv::cvtColor(medianBlurredImage, medianBlurredImage3Channel, cv::COLOR_GRAY2BGR);

    cv::Mat maskImage = convert_vector_to_mat(mask_copy, image_size);

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

    convert_mat_to_vector(output, outputImageGray, image_size);

    return output;
}

void apply_magic_eraser(cv::Mat& mat, MagicEraserOptions const& options) {

    auto mask = options.mask;
    // Only apply if we have a valid mask
    if (mask.empty() || options.image_size.width <= 0 || options.image_size.height <= 0) {
        return;
    }
    
    // Check if the image dimensions match the stored mask dimensions
    if (mat.rows != options.image_size.height || mat.cols != options.image_size.width) {
        std::cerr << "Magic eraser: Image dimensions don't match stored mask dimensions" << std::endl;
        return;
    }
    
    // Convert the image to 3-channel for seamless cloning
    cv::Mat inputImage3Channel;
    if (mat.channels() == 1) {
        cv::cvtColor(mat, inputImage3Channel, cv::COLOR_GRAY2BGR);
    } else {
        inputImage3Channel = mat.clone();
    }

    // Apply median blur filter with configurable size
    cv::Mat medianBlurredImage;
    int filter_size = options.median_filter_size;
    // Ensure filter size is odd and within valid range
    if (filter_size % 2 == 0) filter_size += 1;
    if (filter_size < 3) filter_size = 3;
    if (filter_size > 101) filter_size = 101;
    
    cv::Mat grayMat;
    if (mat.channels() == 3) {
        cv::cvtColor(mat, grayMat, cv::COLOR_BGR2GRAY);
    } else {
        grayMat = mat.clone();
    }
    
    cv::medianBlur(grayMat, medianBlurredImage, filter_size);
    cv::Mat medianBlurredImage3Channel;
    cv::cvtColor(medianBlurredImage, medianBlurredImage3Channel, cv::COLOR_GRAY2BGR);

    // Create mask image from stored mask data
    cv::Mat maskImage = convert_vector_to_mat(mask, options.image_size);

    cv::Mat smoothedMask;
    cv::GaussianBlur(maskImage, smoothedMask, cv::Size(15, 15), 0);

    cv::threshold(smoothedMask, smoothedMask, 1, 255, cv::THRESH_BINARY);

    // Ensure mask has valid values for seamless cloning
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
    cv::Point const center(options.image_size.width / 2, options.image_size.height / 2);

    cv::seamlessClone(medianBlurredImage3Channel, inputImage3Channel, mask3Channel, center, outputImage, cv::NORMAL_CLONE);

    // Convert back to the original format
    if (mat.channels() == 1) {
        cv::cvtColor(outputImage, mat, cv::COLOR_BGR2GRAY);
    } else {
        mat = outputImage;
    }
}

} // namespace ImageProcessing
