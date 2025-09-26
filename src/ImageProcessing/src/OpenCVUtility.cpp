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

/**
 * @brief Apply single-color channel mapping to grayscale image
 * @param input_mat Input grayscale matrix (8-bit)
 * @param output_mat Output BGR colored matrix
 * @param colormap_type The single-color colormap type
 */
static void _applySingleColorMapping(cv::Mat const& input_mat, cv::Mat& output_mat, ColormapType colormap_type) {
    // Create a 3-channel BGR image
    output_mat = cv::Mat::zeros(input_mat.rows, input_mat.cols, CV_8UC3);
    
    // Define the color channel values (BGR format)
    cv::Vec3b color_values;
    
    switch (colormap_type) {
        case ColormapType::Red:
            color_values = cv::Vec3b(0, 0, 255);    // Red in BGR
            break;
        case ColormapType::Green:
            color_values = cv::Vec3b(0, 255, 0);    // Green in BGR
            break;
        case ColormapType::Blue:
            color_values = cv::Vec3b(255, 0, 0);    // Blue in BGR
            break;
        case ColormapType::Cyan:
            color_values = cv::Vec3b(255, 255, 0);  // Cyan in BGR
            break;
        case ColormapType::Magenta:
            color_values = cv::Vec3b(255, 0, 255);  // Magenta in BGR
            break;
        case ColormapType::Yellow:
            color_values = cv::Vec3b(0, 255, 255);  // Yellow in BGR
            break;
        default:
            color_values = cv::Vec3b(0, 0, 255);    // Default to red
            break;
    }
    
    // Apply the color mapping
    for (int y = 0; y < input_mat.rows; ++y) {
        for (int x = 0; x < input_mat.cols; ++x) {
            uint8_t intensity = input_mat.at<uint8_t>(y, x);
            
            // Scale each channel by the intensity
            cv::Vec3b& pixel = output_mat.at<cv::Vec3b>(y, x);
            pixel[0] = (color_values[0] * intensity) / 255;  // Blue channel
            pixel[1] = (color_values[1] * intensity) / 255;  // Green channel
            pixel[2] = (color_values[2] * intensity) / 255;  // Red channel
        }
    }
}

// New options-based implementations
void linear_transform(cv::Mat & mat, ContrastOptions const& options) {
    double alpha = options.alpha;
    int beta = options.beta;

    // Always calculate alpha and beta from min/max values for consistent behavior
    if (options.display_max <= options.display_min) {
        alpha = 1.0;
        beta = 0;
    } else {
        alpha = 255.0 / (options.display_max - options.display_min);
        beta = static_cast<int>(-alpha * options.display_min);
    }

    mat.convertTo(mat, -1, alpha, beta);
}

void gamma_transform(cv::Mat & mat, GammaOptions const& options) {
    if (mat.depth() == CV_8U) {
        // Original 8-bit implementation using lookup table
        cv::Mat lookupTable(1, 256, CV_8U);
        uchar* p = lookupTable.ptr();
        for (int i = 0; i < 256; ++i) {
            p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, options.gamma) * 255.0);
        }
        cv::LUT(mat, lookupTable, mat);
    } else if (mat.depth() == CV_32F) {
        // 32-bit float implementation using direct computation
        // For float data, we assume it's normalized to 0-255 range
        mat.forEach<float>([&options](float& pixel, const int* /*position*/) {
            // Normalize to 0-1 range, apply gamma, then scale back to 0-255
            float normalized = pixel / 255.0f;
            normalized = std::max(0.0f, std::min(1.0f, normalized)); // Clamp to valid range
            pixel = std::pow(normalized, options.gamma) * 255.0f;
        });
    } else {
        // For other bit depths, convert to 8-bit temporarily, apply gamma, then convert back
        cv::Mat temp;
        mat.convertTo(temp, CV_8U);
        
        cv::Mat lookupTable(1, 256, CV_8U);
        uchar* p = lookupTable.ptr();
        for (int i = 0; i < 256; ++i) {
            p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, options.gamma) * 255.0);
        }
        cv::LUT(temp, lookupTable, temp);
        
        temp.convertTo(mat, mat.depth());
    }
}

void clahe(cv::Mat & mat, ClaheOptions const& options) {
    if (mat.depth() == CV_8U) {
        // CLAHE works directly with 8-bit data
        cv::Ptr<cv::CLAHE> clahe_ptr = cv::createCLAHE(options.clip_limit, cv::Size(options.grid_size, options.grid_size));
        clahe_ptr->apply(mat, mat);
    } else if (mat.depth() == CV_32F) {
        // For 32-bit float, convert to 8-bit, apply CLAHE, then convert back
        cv::Mat temp_8bit;
        mat.convertTo(temp_8bit, CV_8U);
        
        cv::Ptr<cv::CLAHE> clahe_ptr = cv::createCLAHE(options.clip_limit, cv::Size(options.grid_size, options.grid_size));
        clahe_ptr->apply(temp_8bit, temp_8bit);
        
        temp_8bit.convertTo(mat, CV_32F);
    } else {
        // For other bit depths, convert to 8-bit, apply CLAHE, then convert back
        cv::Mat temp_8bit;
        mat.convertTo(temp_8bit, CV_8U);
        
        cv::Ptr<cv::CLAHE> clahe_ptr = cv::createCLAHE(options.clip_limit, cv::Size(options.grid_size, options.grid_size));
        clahe_ptr->apply(temp_8bit, temp_8bit);
        
        temp_8bit.convertTo(mat, mat.depth());
    }
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
    // Sanitize kernel size: odd and >= 3
    int k = options.kernel_size;
    if (k < 3) k = 3;
    if ((k % 2) == 0) k += 1;

    // If not 8-bit single-channel grayscale, cap at 5 per OpenCV docs
    bool is_8bit_grayscale = (mat.depth() == CV_8U) && (mat.channels() == 1);
    if (!is_8bit_grayscale && k > 5) {
        // Ensure resulting value is odd and <= 5
        k = 5;
    }

    if (k >= 3 && (k % 2) == 1) {
        cv::medianBlur(mat, mat, k);
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

void apply_colormap(cv::Mat& mat, ColormapOptions const& options) {
    if (!options.active || options.colormap == ColormapType::None) {
        return;
    }
    
    // Only apply to grayscale images
    if (mat.channels() != 1) {
        return;
    }
    
    cv::Mat normalized_mat;
    if (options.normalize) {
        // Normalize to 0-255 range and convert to 8-bit for colormap application
        cv::normalize(mat, normalized_mat, 0, 255, cv::NORM_MINMAX, CV_8UC1);
    } else {
        // Convert to 8-bit if necessary for colormap application
        if (mat.depth() == CV_8U) {
            normalized_mat = mat.clone();
        } else {
            mat.convertTo(normalized_mat, CV_8U);
        }
    }
    
    cv::Mat colored_mat;
    
    // Check if it's a single-color channel mapping
    if (options.colormap == ColormapType::Red || options.colormap == ColormapType::Green || 
        options.colormap == ColormapType::Blue || options.colormap == ColormapType::Cyan ||
        options.colormap == ColormapType::Magenta || options.colormap == ColormapType::Yellow) {
        
        // Apply single-color channel mapping
        _applySingleColorMapping(normalized_mat, colored_mat, options.colormap);
    } else {
        // Apply standard OpenCV colormap
        int cv_colormap = cv::COLORMAP_JET; // default
        
        switch (options.colormap) {
            case ColormapType::Jet:
                cv_colormap = cv::COLORMAP_JET;
                break;
            case ColormapType::Hot:
                cv_colormap = cv::COLORMAP_HOT;
                break;
            case ColormapType::Cool:
                cv_colormap = cv::COLORMAP_COOL;
                break;
            case ColormapType::Spring:
                cv_colormap = cv::COLORMAP_SPRING;
                break;
            case ColormapType::Summer:
                cv_colormap = cv::COLORMAP_SUMMER;
                break;
            case ColormapType::Autumn:
                cv_colormap = cv::COLORMAP_AUTUMN;
                break;
            case ColormapType::Winter:
                cv_colormap = cv::COLORMAP_WINTER;
                break;
            case ColormapType::Rainbow:
                cv_colormap = cv::COLORMAP_RAINBOW;
                break;
            case ColormapType::Ocean:
                cv_colormap = cv::COLORMAP_OCEAN;
                break;
            case ColormapType::Pink:
                cv_colormap = cv::COLORMAP_PINK;
                break;
            case ColormapType::HSV:
                cv_colormap = cv::COLORMAP_HSV;
                break;
            case ColormapType::Parula:
                cv_colormap = cv::COLORMAP_PARULA;
                break;
            case ColormapType::Viridis:
                cv_colormap = cv::COLORMAP_VIRIDIS;
                break;
            case ColormapType::Plasma:
                cv_colormap = cv::COLORMAP_PLASMA;
                break;
            case ColormapType::Inferno:
                cv_colormap = cv::COLORMAP_INFERNO;
                break;
            case ColormapType::Magma:
                cv_colormap = cv::COLORMAP_MAGMA;
                break;
            case ColormapType::Turbo:
                cv_colormap = cv::COLORMAP_TURBO;
                break;
            default:
                cv_colormap = cv::COLORMAP_JET;
                break;
        }
        
        cv::applyColorMap(normalized_mat, colored_mat, cv_colormap);
    }
    
    // Apply alpha blending if needed
    if (options.alpha < 1.0) {
        // Convert original grayscale to BGR for blending
        cv::Mat gray_bgr;
        cv::cvtColor(normalized_mat, gray_bgr, cv::COLOR_GRAY2BGR);
        
        // Blend colored image with grayscale
        cv::addWeighted(colored_mat, options.alpha, gray_bgr, 1.0 - options.alpha, 0, colored_mat);
    }
    
    // Return BGR format (not BGRA) to maintain compatibility
    mat = colored_mat;
}

/**
 * @brief Apply colormap to grayscale data for display purposes only
 * @param grayscale_data Input grayscale image data
 * @param image_size Dimensions of the image
 * @param options Colormap options
 * @return BGR image data if colormap is applied, empty vector if not
 */
std::vector<uint8_t> apply_colormap_for_display(std::vector<uint8_t> const& grayscale_data, 
                                               ImageSize image_size,
                                               ColormapOptions const& options) {
    if (!options.active || options.colormap == ColormapType::None) {
        return {}; // Return empty vector to indicate no colormap applied
    }
    
    // Create cv::Mat from grayscale data
    cv::Mat gray_mat(image_size.height, image_size.width, CV_8UC1, 
                     const_cast<uint8_t*>(grayscale_data.data()));
    
    cv::Mat normalized_mat;
    if (options.normalize) {
        cv::normalize(gray_mat, normalized_mat, 0, 255, cv::NORM_MINMAX, CV_8UC1);
    } else {
        normalized_mat = gray_mat.clone();
    }
    
    // Apply the colormap
    cv::Mat colored_mat;
    
    // Check if it's a single-color channel mapping
    if (options.colormap == ColormapType::Red || options.colormap == ColormapType::Green || 
        options.colormap == ColormapType::Blue || options.colormap == ColormapType::Cyan ||
        options.colormap == ColormapType::Magenta || options.colormap == ColormapType::Yellow) {
        
        // Apply single-color channel mapping
        _applySingleColorMapping(normalized_mat, colored_mat, options.colormap);
    } else {
        // Apply standard OpenCV colormap
        int cv_colormap = cv::COLORMAP_JET; // default
        
        switch (options.colormap) {
            case ColormapType::Jet:
                cv_colormap = cv::COLORMAP_JET;
                break;
            case ColormapType::Hot:
                cv_colormap = cv::COLORMAP_HOT;
                break;
            case ColormapType::Cool:
                cv_colormap = cv::COLORMAP_COOL;
                break;
            case ColormapType::Spring:
                cv_colormap = cv::COLORMAP_SPRING;
                break;
            case ColormapType::Summer:
                cv_colormap = cv::COLORMAP_SUMMER;
                break;
            case ColormapType::Autumn:
                cv_colormap = cv::COLORMAP_AUTUMN;
                break;
            case ColormapType::Winter:
                cv_colormap = cv::COLORMAP_WINTER;
                break;
            case ColormapType::Rainbow:
                cv_colormap = cv::COLORMAP_RAINBOW;
                break;
            case ColormapType::Ocean:
                cv_colormap = cv::COLORMAP_OCEAN;
                break;
            case ColormapType::Pink:
                cv_colormap = cv::COLORMAP_PINK;
                break;
            case ColormapType::HSV:
                cv_colormap = cv::COLORMAP_HSV;
                break;
            case ColormapType::Parula:
                cv_colormap = cv::COLORMAP_PARULA;
                break;
            case ColormapType::Viridis:
                cv_colormap = cv::COLORMAP_VIRIDIS;
                break;
            case ColormapType::Plasma:
                cv_colormap = cv::COLORMAP_PLASMA;
                break;
            case ColormapType::Inferno:
                cv_colormap = cv::COLORMAP_INFERNO;
                break;
            case ColormapType::Magma:
                cv_colormap = cv::COLORMAP_MAGMA;
                break;
            case ColormapType::Turbo:
                cv_colormap = cv::COLORMAP_TURBO;
                break;
            default:
                cv_colormap = cv::COLORMAP_JET;
                break;
        }
        
        cv::applyColorMap(normalized_mat, colored_mat, cv_colormap);
    }
    
    // Apply alpha blending if needed
    if (options.alpha < 1.0) {
        cv::Mat gray_bgr;
        cv::cvtColor(normalized_mat, gray_bgr, cv::COLOR_GRAY2BGR);
        cv::addWeighted(colored_mat, options.alpha, gray_bgr, 1.0 - options.alpha, 0, colored_mat);
    }
    
    // Convert to BGRA for Qt display
    cv::Mat bgra_mat;
    cv::cvtColor(colored_mat, bgra_mat, cv::COLOR_BGR2BGRA);
    
    // Convert to vector
    std::vector<uint8_t> result(bgra_mat.total() * bgra_mat.elemSize());
    std::memcpy(result.data(), bgra_mat.data, result.size());
    
    return result;
}

} // namespace ImageProcessing
