#include "opencv_utility.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>

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

std::vector<Point2D<float>> create_mask(cv::Mat const & mat) {
    std::vector<Point2D<float>> mask;

    for (int y = 0; y < mat.rows; ++y) {
        for (int x = 0; x < mat.cols; ++x) {
            if (mat.at<uint8_t>(y, x) > 0) { // Assuming a binary mask where 0 is background
                mask.push_back({static_cast<float>(x), static_cast<float>(y)});
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
    // Ensure kernel size is odd and >= 3
    int kernel_size = options.kernel_size;
    if (kernel_size < 3) {
        kernel_size = 3;
    }
    if (kernel_size % 2 == 0) {
        kernel_size += 1; // Make it odd
    }
    
    cv::medianBlur(mat, mat, kernel_size);
}
