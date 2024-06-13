
#include "Media/Media_Data.hpp"


MediaData::MediaData() :
    _width{640},
    _height{480}
{
    _rawData = std::vector<uint8_t>(_height * _width);
    setFormat(DisplayFormat::Gray);
};

MediaData::~MediaData()
{

}

void MediaData::setFormat(DisplayFormat const format)
{
    _format = format;
    switch(_format)
    {
    case DisplayFormat::Gray:
        _display_format_bytes = 1;
        break;
    case DisplayFormat::Color:
        _display_format_bytes = 4;
        break;
    default:
        _display_format_bytes = 1;
        break;
    }
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateHeight(int const height)
{
    _height = height;
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::updateWidth(int const width)
{
    _width = width;
    _rawData.resize(_height * _width * _display_format_bytes);
};

void MediaData::LoadMedia(std::string const& name)
{
    doLoadMedia(name);
}

std::vector<uint8_t> const& MediaData::getRawData(int const frame_number)
{
    LoadFrame(frame_number);

    return _rawData;
}

std::vector<uint8_t> MediaData::getProcessedData(const int frame_number)
{
    LoadFrame(frame_number);

    std::vector<uint8_t> output = _rawData;

    auto m2 = convert_vector_to_mat(output, getWidth(),getHeight());

    //cv::convertScaleAbs(m2, m2, 1.5, 0.0);
    //cv::medianBlur(m2,m2,15);

    for (auto const & [key, process] : _process_chain)
    {
        process(m2);
    }

    m2.reshape(1,getWidth()*getHeight());

    output.assign(m2.data, m2.data + m2.total() *m2.channels());

    return output;
}

cv::Mat convert_vector_to_mat(std::vector<uint8_t>& vec, int const width, int const height)
{

    cv::Mat m2{vec, false};

    m2 = m2.reshape(0, width);

    return m2;
}

void convert_mat_to_vector(std::vector<uint8_t>& vec, cv::Mat const & mat, const int width, const int height)
{

    mat.reshape(1,width*height);

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
