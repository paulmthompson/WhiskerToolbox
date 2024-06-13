#include "opencv_utility.hpp"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

cv::Mat load_mask(std::string const & filename)
{
    auto mat = cv::imread(filename);

    cv::threshold(mat,mat,127,255,cv::THRESH_BINARY);

    return mat;
}

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height)
{

    cv::Mat m2{vec, false};

    m2 = m2.reshape(0, width);

    return m2;
}

cv::Mat convert_vector_to_mat(std::vector<Point2D<float>>& vec, int const width, int const height)
{

    cv::Mat mat = cv::Mat::zeros(width, height, CV_8U);

    for (auto const & p : vec)
    {
        auto x_pixel = std::lround(p.x);
        auto y_pixel = std::lround(p.y);

        auto & pixel = mat.at<cv::Vec3b>(y_pixel, x_pixel);

        pixel = cv::Vec3b(255, 255, 255);
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
    for (int x_pixel = 0; x_pixel < mat.cols; x_pixel ++)
    {
        for (int y_pixel = 0; y_pixel < mat.rows; y_pixel++)
        {
            auto & pixel = mat.at<cv::Vec3b>(y_pixel, x_pixel);

            if (pixel == cv::Vec3b(0,0,0))
            {
                mask_points.push_back(Point2D<float>{static_cast<float>(y_pixel),static_cast<float>(x_pixel)});
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
