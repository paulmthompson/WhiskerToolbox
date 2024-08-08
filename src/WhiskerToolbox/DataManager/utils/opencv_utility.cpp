#include "opencv_utility.hpp"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

#include <algorithm>



cv::Mat load_mask_from_image(std::string const & filename, bool const invert)
{
    auto mat = cv::imread(filename,cv::IMREAD_GRAYSCALE);

    cv::threshold(mat,mat,127,255,cv::THRESH_BINARY);

    mat.convertTo(mat,CV_8U);

    if (invert)
    {
        cv::bitwise_not(mat, mat);
    }

    return mat;
}

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height)
{

    cv::Mat m2{vec, false};

    // The number of rows = height, number of columns = width
    m2 = m2.reshape(0, height);

    return m2;
}

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>>& vec, int const width, int const height)
{

    // Mat is constructed with (# of rows, # columns)
    // The number of rows = height, number of columns = width
    cv::Mat mat = cv::Mat::zeros(height, width, CV_8U);
    mat = mat.setTo(255);

    for (auto const & p : vec)
    {
        auto x_pixel = static_cast<int>(std::lround(p.x));
        auto y_pixel = static_cast<int>(std::lround(p.y));

        x_pixel = std::clamp(x_pixel, 0, width - 1);
        y_pixel = std::clamp(y_pixel, 0, height - 1);

        //At (row # (max=height), column # (max=width))
        auto & pixel = mat.at<uint8_t>(y_pixel, x_pixel);

        pixel = 0;
    }

    return mat;
}

void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat & mat, const int width, const int height)
{

    mat = mat.reshape(1,width*height);

    //https://stackoverflow.com/questions/26681713/convert-mat-to-array-vector-in-opencv
    if (mat.isContinuous()) {
        vec.assign(mat.data, mat.data + mat.total() * mat.channels());
    } else {
        for (int i = 0; i< mat.rows; i++) {
            vec.insert(vec.end(), mat.ptr<uchar>(i),  mat.ptr<uchar>(i)+mat.cols*mat.channels());
        }
    }
}

std::vector<Point2D<float>> create_mask(cv::Mat const & mat)
{
    std::vector<Point2D<float>> mask_points;
    for (int x_pixel = 0; x_pixel < mat.cols; x_pixel ++) //Number of cols = width
    {
        for (int y_pixel = 0; y_pixel < mat.rows; y_pixel++) // Number of rows = height
        {
            //At (row # (max=height), column # (max=width))
            auto & pixel = mat.at<uint8_t>(y_pixel, x_pixel);

            if (pixel == 0)
            {
                mask_points.push_back(Point2D<float>{static_cast<float>(x_pixel),static_cast<float>(y_pixel)});
            }
        }
    }

    return mask_points;
}

void grow_mask(cv::Mat & mat, int const dilation_size)
{
    cv::Mat element = cv::Mat::ones(dilation_size,dilation_size,CV_8U);

    cv::bitwise_not(mat, mat);
    cv::dilate(mat, mat, element,cv::Point(-1,-1),1);
    cv::bitwise_not(mat, mat);
}

void median_blur(cv::Mat & mat, int const kernel_size)
{
    cv::medianBlur(mat, mat , kernel_size);
}

void linear_transform(cv::Mat & mat, double alpha, int beta)
{
    mat.convertTo(mat, -1, alpha, beta);
}

void clahe(cv::Mat & mat, double const clip_limit, int const grid_size)
{
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(clip_limit);
    clahe->setTilesGridSize(cv::Size(grid_size,grid_size));
    clahe->apply(mat, mat);
}

void sharpen_image(cv::Mat& img, const double sigma )
{
    cv::Mat img_blur, img_sharp;

    // Apply Gaussian blur to the image
    cv::GaussianBlur(img, img_blur, cv::Size(0, 0), sigma);

    // Subtract the blurred image from the original image (scaled)
    cv::addWeighted(img, 1.5, img_blur, -0.5, 0, img_sharp);

    // Copy the sharpened image back to the original cv::Mat
    img_sharp.copyTo(img);
}

void bilateral_filter(cv::Mat& img, int d, double sigmaColor, double sigmaSpace)
{
    cv::Mat img_filtered;

    // Apply bilateral filter to the image
    cv::bilateralFilter(img, img_filtered, d, sigmaColor, sigmaSpace);

    // Copy the filtered image back to the original cv::Mat
    img_filtered.copyTo(img);
}